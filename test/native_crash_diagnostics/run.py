#!/usr/bin/env python3
"""Exercise production crash reporting across simulated resets, with IDF edges stubbed."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/crash-diagnostics'
OUT.mkdir(parents=True, exist_ok=True)
stub = r'''
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <cstring>
#include <cassert>
#define configMAX_TASK_NAME_LEN 16
#define RTC_NOINIT_ATTR
#define CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH 1
#define CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF 1
#define CONFIG_IDF_TARGET_ARCH_XTENSA 1
#define APP_ELF_SHA256_SZ 65
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define ESP_OK 0
inline std::string usb,rom;
inline void capture(std::string& out,const char* fmt,...){char b[1024];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);out+=b;out+='\n';}
#define ESP_LOGE(tag,...) capture(usb,__VA_ARGS__)
#define ESP_LOGW(tag,...) capture(usb,__VA_ARGS__)
#define ESP_LOGI(tag,...) capture(usb,__VA_ARGS__)
#define esp_rom_printf(...) capture(rom,__VA_ARGS__)
struct SerialStub{void setDebugOutput(bool){}};inline SerialStub Serial;
struct EspStub{uint32_t getFreePsram(){return 8000000;}};inline EspStub ESP;
enum esp_reset_reason_t{ESP_RST_UNKNOWN,ESP_RST_POWERON,ESP_RST_EXT,ESP_RST_SW,ESP_RST_PANIC,ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT,ESP_RST_DEEPSLEEP,ESP_RST_BROWNOUT,ESP_RST_SDIO,ESP_RST_USB,ESP_RST_JTAG,ESP_RST_EFUSE,ESP_RST_PWR_GLITCH,ESP_RST_CPU_LOCKUP};
inline esp_reset_reason_t reason=ESP_RST_POWERON;
inline esp_reset_reason_t esp_reset_reason(){return reason;}
inline const char* pcTaskGetName(void*){return "nimble_host";}
inline int xPortGetCoreID(){return 0;}
inline uint32_t uxTaskGetStackHighWaterMark(void*){return 2048;}
inline uint32_t heap_caps_get_free_size(int){return 50000;}
inline uint32_t heap_caps_get_minimum_free_size(int){return 38000;}
inline uint32_t heap_caps_get_largest_free_block(int){return 30000;}
using AllocationCallback=void(*)(size_t,uint32_t,const char*);
inline AllocationCallback allocationHandler;
inline int heap_caps_register_failed_alloc_callback(AllocationCallback f){allocationHandler=f;return 0;}
struct arduino_panic_info_t{int core;void* pc;bool backtrace_corrupt,backtrace_continues;const char* reason;size_t backtrace_len;uint32_t backtrace[60];};
inline void(*panicHandler)(arduino_panic_info_t*,void*);
inline void set_arduino_panic_handler(decltype(panicHandler) f,void*){panicHandler=f;}
struct esp_core_dump_summary_t{char exc_task[16];uint32_t exc_pc;uint8_t app_elf_sha256[65];struct{uint32_t bt[16],depth;bool corrupted;}exc_bt_info;struct{uint32_t exc_cause,exc_vaddr;}ex_info;};
inline bool validDump=false;inline int summaryResult=0,summaryCalls=0;inline esp_core_dump_summary_t saved{};
inline int esp_core_dump_image_check(){return validDump?0:-1;}
inline int esp_core_dump_get_summary(esp_core_dump_summary_t* s){summaryCalls++;*s=saved;return summaryResult;}
'''
(OUT / 'stub.h').write_text(stub)
for name in ['Arduino.h','esp_attr.h','esp_heap_caps.h','esp_rom_sys.h','esp_system.h','esp_core_dump.h','esp_log.h','freertos/FreeRTOS.h','freertos/task.h']:
    f = OUT / name
    f.parent.mkdir(parents=True, exist_ok=True)
    f.write_text('#include "stub.h"\n')
(OUT / 'main.cpp').write_text(r'''
#include "stub.h"
#include "logging/Logger.cpp"
int main(){
 Logger::begin();Logger::logResetReason();assert(summaryCalls==0);assert(usb.find("saved frame")==std::string::npos);
 Logger::checkpoint("ble_mtu");arduino_panic_info_t p{};p.core=0;p.pc=(void*)0x42012345;p.reason="LoadProhibited";p.backtrace_len=60;for(unsigned i=0;i<60;i++)p.backtrace[i]=0x42000000+i;
 panicHandler(&p,nullptr);reason=ESP_RST_PANIC;Logger::begin();usb.clear();rom.clear();Logger::logResetReason();
 assert(usb.find("phase=ble_mtu")!=std::string::npos);assert(usb.find("reason=LoadProhibited")!=std::string::npos);assert(usb.find("previous frame[15]=0x4200000f")!=std::string::npos);assert(usb.find("previous frame[16]")==std::string::npos);assert(rom.empty());
 // A software/OTA reboot must still recover an older flash dump. Only its
 // public control-flow fields are printed, and excessive depth is bounded.
 validDump=true;reason=ESP_RST_SW;strcpy(saved.exc_task,"nimble_host");strcpy((char*)saved.app_elf_sha256,"old-build-hash");saved.exc_pc=0x42012345;saved.exc_bt_info.depth=100;saved.ex_info.exc_cause=28;saved.ex_info.exc_vaddr=0;for(unsigned i=0;i<16;i++)saved.exc_bt_info.bt[i]=0x42000000+i;
 Logger::begin();usb.clear();Logger::logResetReason();assert(usb.find("may predate this boot")!=std::string::npos);assert(usb.find("old-build-hash")!=std::string::npos);assert(usb.find("saved frame[15]")!=std::string::npos);assert(usb.find("saved frame[16]")==std::string::npos);assert(usb.find("cause=28")!=std::string::npos);
 summaryResult=-1;usb.clear();Logger::logResetReason();assert(usb.find("summary is unavailable")!=std::string::npos);assert(usb.find("saved frame")==std::string::npos);
 validDump=false;summaryCalls=0;usb.clear();Logger::logResetReason();assert(summaryCalls==0);assert(usb.find("saved frame")==std::string::npos);
 puts("Crash diagnostics passed: retained panic, USB backtrace, old flash dump after OTA, bounded/corrupt/missing summaries.");
}
''')
subprocess.run(['g++','-std=c++23','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(OUT),'-I'+str(ROOT/'src'),str(OUT/'main.cpp'),'-o',str(OUT/'check')],check=True)
# LeakSanitizer cannot inspect processes inside this ptrace-based sandbox.
# Keep address/undefined-behaviour checks active for the fixed-size records.
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
subprocess.run([str(OUT/'check')],check=True,env=env)
