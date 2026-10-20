#include "logging/Logger.h"

#include <Arduino.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_rom_sys.h>
#include <esp_system.h>
#include <esp_core_dump.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace {

    constexpr uint32_t kCrashRecordMagic = 0x52535650; // RSVP
    constexpr uint16_t kCrashRecordVersion = 1;
    constexpr size_t kStoredBacktraceAddresses = 16;

    struct CrashRecord {
        uint32_t magic;
        uint16_t version;
        uint16_t size;
        char phase[32];
        char task[configMAX_TASK_NAME_LEN];
        char panicReason[48];
        char allocationFunction[32];
        uint32_t freeInternalHeap;
        uint32_t minimumInternalHeap;
        uint32_t largestInternalBlock;
        uint32_t freePsram;
        uint32_t stackHighWaterMark;
        uint32_t failedAllocationSize;
        uint32_t failedAllocationCaps;
        uintptr_t panicPc;
        uint32_t backtrace[kStoredBacktraceAddresses];
        uint8_t backtraceLength;
        int8_t core;
        int8_t panicCore;
        bool allocationFailed;
        bool panicCaptured;
        bool backtraceCorrupt;
        bool backtraceContinues;
    };

    // Survives panic/watchdog resets so diagnostics remain available on the next boot.
    RTC_NOINIT_ATTR CrashRecord retainedRecord;
    CrashRecord previousRecord;
    bool hasPreviousRecord = false;

    template<size_t Size>
    void copyText(char (&destination)[Size], const char* source) {
        // Also called after heap failure and during panic; do not allocate or take libc locks here.
        size_t index = 0;
        if (source != nullptr) {
            while (index + 1 < Size && source[index] != '\0') {
                destination[index] = source[index];
                ++index;
            }
        }
        destination[index] = '\0';
    }

    bool valid(const CrashRecord& record) {
        return record.magic == kCrashRecordMagic && record.version == kCrashRecordVersion
            && record.size == sizeof(CrashRecord);
    }

    const char* resetReasonName(esp_reset_reason_t reason) {
        switch (reason) {
        case ESP_RST_POWERON:
            return "poweron";
        case ESP_RST_EXT:
            return "external";
        case ESP_RST_SW:
            return "software";
        case ESP_RST_PANIC:
            return "panic";
        case ESP_RST_INT_WDT:
            return "interrupt_watchdog";
        case ESP_RST_TASK_WDT:
            return "task_watchdog";
        case ESP_RST_WDT:
            return "watchdog";
        case ESP_RST_DEEPSLEEP:
            return "deep_sleep";
        case ESP_RST_BROWNOUT:
            return "brownout";
        case ESP_RST_SDIO:
            return "sdio";
        case ESP_RST_USB:
            return "usb";
        case ESP_RST_JTAG:
            return "jtag";
        case ESP_RST_EFUSE:
            return "efuse";
        case ESP_RST_PWR_GLITCH:
            return "power_glitch";
        case ESP_RST_CPU_LOCKUP:
            return "cpu_lockup";
        case ESP_RST_UNKNOWN:
        default:
            return "unknown";
        }
    }

    bool abnormalReset(esp_reset_reason_t reason) {
        switch (reason) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_BROWNOUT:
        case ESP_RST_EFUSE:
        case ESP_RST_PWR_GLITCH:
        case ESP_RST_CPU_LOCKUP:
        case ESP_RST_UNKNOWN:
            return true;
        default:
            return false;
        }
    }

    void logSavedCrash() {
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
        // A flash dump can survive OTA, so it can recover a crash from the
        // preceding build even when its panic callback was not linked. Report
        // only control-flow diagnostics, never stack bytes or private data.
        if (esp_core_dump_image_check() != ESP_OK)
            return;
        static esp_core_dump_summary_t summary;
        if (esp_core_dump_get_summary(&summary) != ESP_OK) {
            ESP_LOGW("crash", "saved dump exists but its summary is unavailable");
            return;
        }
        ESP_LOGE("crash", "saved dump (may predate this boot) task=%.16s pc=0x%08x elf_sha256=%.*s",
                 summary.exc_task, static_cast<unsigned>(summary.exc_pc), APP_ELF_SHA256_SZ - 1,
                 reinterpret_cast<const char*>(summary.app_elf_sha256));
#if CONFIG_IDF_TARGET_ARCH_XTENSA
        ESP_LOGE("crash", "saved exception cause=%u address=0x%08x depth=%u corrupt=%d",
                 static_cast<unsigned>(summary.ex_info.exc_cause),
                 static_cast<unsigned>(summary.ex_info.exc_vaddr),
                 static_cast<unsigned>(summary.exc_bt_info.depth), summary.exc_bt_info.corrupted);
        // Use the normal USB log route after reboot, not the ROM panic UART.
        const size_t count = std::min<size_t>(summary.exc_bt_info.depth,
                                            sizeof(summary.exc_bt_info.bt) / sizeof(summary.exc_bt_info.bt[0]));
        for (size_t i = 0; i < count; ++i)
            ESP_LOGE("crash", "saved frame[%u]=0x%08x", static_cast<unsigned>(i),
                     static_cast<unsigned>(summary.exc_bt_info.bt[i]));
#endif
#endif
    }

    void allocationFailed(size_t size, uint32_t caps, const char* functionName) {
        retainedRecord.failedAllocationSize = static_cast<uint32_t>(size);
        retainedRecord.failedAllocationCaps = caps;
        copyText(retainedRecord.allocationFunction, functionName);
        retainedRecord.allocationFailed = true;
        esp_rom_printf("\n[alloc-failed] phase=%s task=%s core=%d size=%u caps=0x%08x function=%s\n",
                       retainedRecord.phase, retainedRecord.task, retainedRecord.core,
                       retainedRecord.failedAllocationSize, retainedRecord.failedAllocationCaps,
                       retainedRecord.allocationFunction);
    }

    void panic(arduino_panic_info_t* info, void*) {
        retainedRecord.panicCaptured = true;
        if (info != nullptr) {
            retainedRecord.panicCore = static_cast<int8_t>(info->core);
            retainedRecord.panicPc = reinterpret_cast<uintptr_t>(info->pc);
            retainedRecord.backtraceCorrupt = info->backtrace_corrupt;
            retainedRecord.backtraceContinues = info->backtrace_continues;
            copyText(retainedRecord.panicReason, info->reason);
            const size_t length = std::min<size_t>(info->backtrace_len, kStoredBacktraceAddresses);
            retainedRecord.backtraceLength = static_cast<uint8_t>(length);
            for (size_t index = 0; index < length; ++index)
                retainedRecord.backtrace[index] = info->backtrace[index];
        }

        esp_rom_printf("\n[panic-context] phase=%s task=%s checkpoint_core=%d reason=%s panic_core=%d pc=%p\n",
                       retainedRecord.phase, retainedRecord.task, retainedRecord.core,
                       retainedRecord.panicReason, retainedRecord.panicCore,
                       reinterpret_cast<void*>(retainedRecord.panicPc));
        esp_rom_printf("[panic-context] heap=%u min_heap=%u largest=%u psram=%u stack_free=%u",
                       retainedRecord.freeInternalHeap, retainedRecord.minimumInternalHeap,
                       retainedRecord.largestInternalBlock, retainedRecord.freePsram,
                       retainedRecord.stackHighWaterMark);
        if (retainedRecord.allocationFailed) {
            esp_rom_printf(" last_alloc=%u caps=0x%08x function=%s", retainedRecord.failedAllocationSize,
                           retainedRecord.failedAllocationCaps, retainedRecord.allocationFunction);
        }
        esp_rom_printf("\n");
    }

} // namespace

namespace Logger {

    void begin() {
        Serial.setDebugOutput(true);
        if (valid(retainedRecord)) {
            previousRecord = retainedRecord;
            hasPreviousRecord = true;
        }

        retainedRecord = {};
        retainedRecord.magic = kCrashRecordMagic;
        retainedRecord.version = kCrashRecordVersion;
        retainedRecord.size = sizeof(CrashRecord);
        retainedRecord.core = -1;
        retainedRecord.panicCore = -1;
        checkpoint("board_init");

        set_arduino_panic_handler(panic, nullptr);
        if (heap_caps_register_failed_alloc_callback(allocationFailed) != ESP_OK)
            ESP_LOGW("diag", "failed-allocation callback unavailable");
    }

    void checkpoint(const char* phase) {
        copyText(retainedRecord.phase, phase);
        copyText(retainedRecord.task, pcTaskGetName(nullptr));
        retainedRecord.core = static_cast<int8_t>(xPortGetCoreID());
        retainedRecord.freeInternalHeap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        retainedRecord.minimumInternalHeap =
            heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        retainedRecord.largestInternalBlock =
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        retainedRecord.freePsram = ESP.getFreePsram();
        retainedRecord.stackHighWaterMark = uxTaskGetStackHighWaterMark(nullptr);
    }

    void startupCheckpoint(const char* phase) {
        checkpoint(phase);
        ESP_LOGI("startup", "phase=%s task=%s core=%d heap=%u min_heap=%u largest=%u psram=%u stack_free=%u",
                 retainedRecord.phase, retainedRecord.task, retainedRecord.core, retainedRecord.freeInternalHeap,
                 retainedRecord.minimumInternalHeap, retainedRecord.largestInternalBlock, retainedRecord.freePsram,
                 retainedRecord.stackHighWaterMark);
    }

    void logResetReason() {
        const esp_reset_reason_t reason = esp_reset_reason();
        ESP_LOGI("diag", "reset=%s(%d)", resetReasonName(reason), static_cast<int>(reason));
        logSavedCrash();
        if (!hasPreviousRecord || !abnormalReset(reason)) {
            hasPreviousRecord = false;
            return;
        }

        ESP_LOGE("crash", "previous phase=%s task=%s core=%d heap=%u min_heap=%u largest=%u psram=%u "
                          "stack_free=%u",
                 previousRecord.phase, previousRecord.task, previousRecord.core, previousRecord.freeInternalHeap,
                 previousRecord.minimumInternalHeap, previousRecord.largestInternalBlock, previousRecord.freePsram,
                 previousRecord.stackHighWaterMark);
        if (previousRecord.allocationFailed) {
            ESP_LOGE("crash", "previous allocation_failed size=%u caps=0x%08x function=%s",
                     previousRecord.failedAllocationSize, previousRecord.failedAllocationCaps,
                     previousRecord.allocationFunction);
        }
        if (previousRecord.panicCaptured) {
            ESP_LOGE("crash", "previous panic reason=%s core=%d pc=%p backtrace_len=%u corrupt=%d continues=%d",
                     previousRecord.panicReason, previousRecord.panicCore,
                     reinterpret_cast<void*>(previousRecord.panicPc),
                     static_cast<unsigned>(previousRecord.backtraceLength),
                     previousRecord.backtraceCorrupt, previousRecord.backtraceContinues);
            for (uint8_t index = 0; index < previousRecord.backtraceLength; ++index)
                ESP_LOGE("crash", "previous frame[%u]=0x%08x", static_cast<unsigned>(index),
                         static_cast<unsigned>(previousRecord.backtrace[index]));
        }
        hasPreviousRecord = false;
    }

} // namespace Logger
