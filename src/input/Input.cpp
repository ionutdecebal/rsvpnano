#include "input/Input.h"

#include <algorithm>
#include <atomic>
#include <esp_log.h>

#include "board/BoardInput.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace Input {
    namespace {

        constexpr uint32_t kControlsPollMs = 5;
        constexpr UBaseType_t kSamplerPriority = 2;
        constexpr uint32_t kSamplerStackBytes = 4096;
        constexpr UBaseType_t kEventQueueLength = 8;
        constexpr UBaseType_t kTouchQueueLength = 32;

        struct ControlsState {
            bool initialized = false;
            ActionMask stableShortActions = ActionNone;
            ActionMask stableLongActions = ActionNone;
            ActionMask candidateShortActions = ActionNone;
            ActionMask candidateLongActions = ActionNone;
            ActionMask activeShortActions = ActionNone;
            ActionMask activeLongActions = ActionNone;
            bool releasedEvent = false;
            uint32_t candidateSinceMs = 0;
            uint32_t pressStartedMs = 0;
            uint32_t lastPressDurationMs = 0;
        };

        ControlsState gControls;
        ControlTiming gControlTiming;
        TouchTiming gTouchTiming;
        QueueHandle_t gEventQueue = nullptr;
        QueueHandle_t gTouchQueue = nullptr;
        TaskHandle_t gSamplerTask = nullptr;
        std::atomic_bool gPaused = false;
        std::atomic_bool gPauseAcknowledged = false;
        bool gTouchInitialized = false;
        bool gTouchActive = false;
        bool gTouchProbeFailureLogged = false;
        bool gTouchReadFailureLogged = false;
        uint8_t gTouchReleaseSamples = 0;
        uint8_t gTouchReadFailures = 0;
        uint32_t gTouchBackoffUntilMs = 0;
        uint32_t gTouchIgnoreUntilMs = 0;

        struct TouchSample {
            ui::TouchContact contact;
            ui::TouchSampleResult result = ui::TouchSampleResult::None;
        };

        bool anyAction(ActionMask shortActions, ActionMask longActions) {
            return shortActions != ActionNone || longActions != ActionNone;
        }

        void resetControls(ActionMask shortActions, ActionMask longActions, uint32_t nowMs) {
            gControls.initialized = true;
            gControls.stableShortActions = shortActions;
            gControls.stableLongActions = longActions;
            gControls.candidateShortActions = shortActions;
            gControls.candidateLongActions = longActions;
            gControls.activeShortActions = ActionNone;
            gControls.activeLongActions = ActionNone;
            gControls.releasedEvent = false;
            gControls.candidateSinceMs = nowMs;
            gControls.pressStartedMs = anyAction(shortActions, longActions) ? nowMs : 0;
            gControls.lastPressDurationMs = 0;
        }

        void updateControls(ActionMask shortActions, ActionMask longActions, uint32_t nowMs) {
            if (!gControls.initialized) {
                resetControls(shortActions, longActions, nowMs);
                return;
            }

            gControls.releasedEvent = false;

            {
                // Debounce the raw bitmask before changing the stable pressed controls.
                if (shortActions != gControls.candidateShortActions || longActions != gControls.candidateLongActions) {
                    gControls.candidateShortActions = shortActions;
                    gControls.candidateLongActions = longActions;
                    gControls.candidateSinceMs = nowMs;
                }

                if (gControls.candidateShortActions == gControls.stableShortActions
                    && gControls.candidateLongActions == gControls.stableLongActions) {
                    return;
                }

                if (nowMs - gControls.candidateSinceMs < gControlTiming.debounceMs) {
                    return;
                }
            }

            const ActionMask previousShortActions = gControls.stableShortActions;
            const ActionMask previousLongActions = gControls.stableLongActions;
            gControls.stableShortActions = gControls.candidateShortActions;
            gControls.stableLongActions = gControls.candidateLongActions;

            {
                // Track the gesture lifetime for whichever controls became active together.
                if (!anyAction(previousShortActions, previousLongActions)
                    && anyAction(gControls.stableShortActions, gControls.stableLongActions)) {
                    gControls.activeShortActions = gControls.stableShortActions;
                    gControls.activeLongActions = gControls.stableLongActions;
                    gControls.pressStartedMs = nowMs;
                    gControls.lastPressDurationMs = 0;
                    return;
                }

                if (anyAction(previousShortActions, previousLongActions)
                    && !anyAction(gControls.stableShortActions, gControls.stableLongActions)) {
                    gControls.releasedEvent = true;
                    gControls.lastPressDurationMs = nowMs - gControls.pressStartedMs;
                    return;
                }

                if (previousShortActions != gControls.stableShortActions
                    || previousLongActions != gControls.stableLongActions) {
                    gControls.activeShortActions = gControls.stableShortActions;
                    gControls.activeLongActions = gControls.stableLongActions;
                    gControls.pressStartedMs = nowMs;
                    gControls.lastPressDurationMs = 0;
                }
            }
        }

        bool pollControlsEvent(ActionMask shortActions, ActionMask longActions, uint32_t nowMs, ActionMask& event) {
            updateControls(shortActions, longActions, nowMs);

            if (anyAction(gControls.stableShortActions, gControls.stableLongActions)
                && anyAction(gControls.activeShortActions, gControls.activeLongActions)
                && nowMs - gControls.pressStartedMs >= gControlTiming.longPressMs) {
                const ActionMask actions = gControls.activeLongActions;
                if (actions == ActionNone) {
                    gControls.activeShortActions = ActionNone;
                    gControls.activeLongActions = ActionNone;
                    return false;
                }
                event = actions;
                gControls.activeShortActions = ActionNone;
                gControls.activeLongActions = ActionNone;
                return true;
            }

            if (gControls.releasedEvent && anyAction(gControls.activeShortActions, gControls.activeLongActions)
                && gControls.lastPressDurationMs <= gControlTiming.shortPressMaxMs) {
                const ActionMask actions = gControls.activeShortActions;
                if (actions == ActionNone) {
                    gControls.activeShortActions = ActionNone;
                    gControls.activeLongActions = ActionNone;
                    return false;
                }
                event = actions;
                gControls.activeShortActions = ActionNone;
                gControls.activeLongActions = ActionNone;
                return true;
            }

            if (gControls.releasedEvent) {
                gControls.activeShortActions = ActionNone;
                gControls.activeLongActions = ActionNone;
            }
            return false;
        }

        template<typename T>
        void enqueueLatest(QueueHandle_t queue, const T& value) {
            if (xQueueSend(queue, &value, 0) == pdTRUE)
                return;
            T discarded;
            xQueueReceive(queue, &discarded, 0);
            xQueueSend(queue, &value, 0);
        }

        bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
            return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
        }

        void sampleInputs(void*) {
            ESP_LOGI("input", "sampler started task=%s core=%d", pcTaskGetName(nullptr), xPortGetCoreID());
            uint32_t nextControlsMs = millis();
            uint32_t nextTouchMs = nextControlsMs;
            gTouchBackoffUntilMs = nextTouchMs;
            bool touchNotified = false;

            while (true) {
                if (gPaused.load()) {
                    gPauseAcknowledged.store(true);
                    vTaskDelay(pdMS_TO_TICKS(1));
                    continue;
                }
                if (gPauseAcknowledged.exchange(false)) {
                    const PressActions actions = Board::Input::currentActions();
                    const uint32_t resumedAtMs = millis();
                    resetControls(actions.shortPress, actions.longPress, resumedAtMs);
                    gTouchInitialized = false;
                    gTouchActive = false;
                    gTouchReleaseSamples = 0;
                    gTouchReadFailures = 0;
                    gTouchBackoffUntilMs = resumedAtMs;
                    xQueueReset(gTouchQueue);
                    TouchSample reset;
                    reset.result = ui::TouchSampleResult::Reset;
                    enqueueLatest(gTouchQueue, reset);
                }

                const uint32_t nowMs = millis();
                if (deadlineReached(nowMs, nextControlsMs)) {
                    const PressActions actions = Board::Input::currentActions();
                    ActionMask event = ActionNone;
                    if (pollControlsEvent(actions.shortPress, actions.longPress, nowMs, event))
                        enqueueLatest(gEventQueue, event);
                    nextControlsMs = nowMs + kControlsPollMs;
                }

                if (!gTouchInitialized && deadlineReached(nowMs, gTouchBackoffUntilMs)) {
                    gTouchInitialized = Board::Input::beginTouch();
                    gTouchReadFailures = 0;
                    if (gTouchInitialized) {
                        ESP_LOGI("input", "touch controller ready%s", gTouchProbeFailureLogged ? " after retry" : "");
                        gTouchProbeFailureLogged = false;
                        gTouchIgnoreUntilMs = nowMs + gTouchTiming.recoveryEventIgnoreMs;
                    } else {
                        if (!gTouchProbeFailureLogged)
                            ESP_LOGW("input", "touch controller probe failed; retrying");
                        gTouchProbeFailureLogged = true;
                        gTouchBackoffUntilMs = nowMs + gTouchTiming.recoveryRetryMs;
                    }
                }

                // A retained IRQ starts acquisition promptly; transport failures still honor retry backoff.
                if (gTouchInitialized
                    && ((touchNotified && gTouchReadFailures == 0) || deadlineReached(nowMs, nextTouchMs))) {
                    const uint32_t readyIntervalMs = std::max<uint32_t>(1, gTouchTiming.readyPollIntervalMs);
                    const uint32_t packetIntervalMs = std::max<uint32_t>(1, gTouchTiming.pollIntervalMs);
                    nextTouchMs = nowMs + readyIntervalMs;
                    if (gTouchActive || Board::Input::touchReady()) {
                        nextTouchMs = nowMs + packetIntervalMs;
                        TouchSample sample;
                        sample.contact.sampledAtMs = nowMs;
                        if (Board::Input::readTouch(sample.contact)) {
                            if (gTouchReadFailureLogged)
                                ESP_LOGI("input", "touch packet reads recovered");
                            gTouchReadFailureLogged = false;
                            gTouchReadFailures = 0;
                            bool emitContact = true;
                            if (sample.contact.touched) {
                                gTouchActive = true;
                                gTouchReleaseSamples = 0;
                            } else if (gTouchActive
                                       && ++gTouchReleaseSamples
                                              < std::max<uint8_t>(1, gTouchTiming.releaseConfirmSamples)) {
                                emitContact = false;
                            } else {
                                gTouchActive = false;
                                gTouchReleaseSamples = 0;
                            }
                            if (!gTouchActive)
                                nextTouchMs = nowMs + readyIntervalMs;
                            if (emitContact && deadlineReached(nowMs, gTouchIgnoreUntilMs)) {
                                sample.result = ui::TouchSampleResult::Contact;
                                enqueueLatest(gTouchQueue, sample);
                            }
                        } else if (++gTouchReadFailures >= gTouchTiming.maxConsecutiveReadFailures) {
                            if (!gTouchReadFailureLogged)
                                ESP_LOGW("input", "touch reset after %u packet read failures",
                                         static_cast<unsigned>(gTouchReadFailures));
                            gTouchReadFailureLogged = true;
                            gTouchInitialized = false;
                            gTouchActive = false;
                            gTouchReleaseSamples = 0;
                            gTouchBackoffUntilMs = nowMs + gTouchTiming.recoveryRetryMs;
                            sample.result = ui::TouchSampleResult::Reset;
                            enqueueLatest(gTouchQueue, sample);
                        } else if (!gTouchActive) {
                            nextTouchMs = nowMs + gTouchTiming.failureBackoffMs;
                        }
                    }
                }

                touchNotified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1)) != 0;
            }
        }

    } // namespace

    bool begin() {
        gControls.initialized = false;
        gPaused.store(false);
        gPauseAcknowledged.store(false);
        gTouchInitialized = false;
        gTouchActive = false;
        gTouchProbeFailureLogged = false;
        gTouchReadFailureLogged = false;
        gTouchReleaseSamples = 0;
        gTouchReadFailures = 0;
        if (!Board::Input::begin()) {
            ESP_LOGE("input", "board input initialization failed");
            return false;
        }

        gControlTiming = Board::Input::controlTiming();
        gTouchTiming = Board::Input::touchTiming();
        const PressActions actions = Board::Input::currentActions();
        resetControls(actions.shortPress, actions.longPress, millis());
        gEventQueue = xQueueCreate(kEventQueueLength, sizeof(ActionMask));
        gTouchQueue = xQueueCreate(kTouchQueueLength, sizeof(TouchSample));
        if (gEventQueue == nullptr || gTouchQueue == nullptr) {
            ESP_LOGE("input", "queue allocation failed event=%u touch=%u", gEventQueue != nullptr ? 1U : 0U,
                     gTouchQueue != nullptr ? 1U : 0U);
            end();
            return false;
        }
        if (xTaskCreate(sampleInputs, "input", kSamplerStackBytes, nullptr, kSamplerPriority, &gSamplerTask)
            != pdPASS) {
            ESP_LOGE("input", "sampler task allocation failed");
            end();
            return false;
        }
        return true;
    }

    void end() {
        cancel();
        Board::Input::end();
        if (gSamplerTask != nullptr) {
            vTaskDelete(gSamplerTask);
            gSamplerTask = nullptr;
        }
        if (gEventQueue != nullptr) {
            vQueueDelete(gEventQueue);
            gEventQueue = nullptr;
        }
        if (gTouchQueue != nullptr) {
            vQueueDelete(gTouchQueue);
            gTouchQueue = nullptr;
        }
        gControls.initialized = false;
    }

    void IRAM_ATTR notifyTouchFromISR() {
        if (gSamplerTask == nullptr)
            return;
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(gSamplerTask, &higherPriorityTaskWoken);
        if (higherPriorityTaskWoken)
            portYIELD_FROM_ISR();
    }

    void cancel() {
        gPauseAcknowledged.store(false);
        gPaused.store(true);
        while (gSamplerTask != nullptr && !gPauseAcknowledged.load())
            delay(1);
        if (gEventQueue != nullptr)
            xQueueReset(gEventQueue);
        if (gTouchQueue != nullptr)
            xQueueReset(gTouchQueue);
        Board::Input::cancel();
    }

    void resume() {
        if (gSamplerTask == nullptr)
            return;
        gPaused.store(false);
    }

    bool poll(ActionMask& actions) {
        actions = ActionNone;
        if (gEventQueue == nullptr) {
            return false;
        }
        return xQueueReceive(gEventQueue, &actions, 0) == pdTRUE;
    }

    ui::TouchSampleResult pollTouch(ui::TouchContact& contact) {
        TouchSample sample;
        if (gTouchQueue == nullptr || xQueueReceive(gTouchQueue, &sample, 0) != pdTRUE)
            return ui::TouchSampleResult::None;
        contact = sample.contact;
        return sample.result;
    }

} // namespace Input
