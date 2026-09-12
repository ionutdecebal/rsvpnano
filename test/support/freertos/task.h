#pragma once

#include "FreeRTOS.h"

namespace FakeTask {
    inline uint32_t now = 0;
    inline uint32_t notifications = 0;
    inline void (*entry)(void*) = nullptr;
    inline void (*tick)() = nullptr;
} // namespace FakeTask
using TaskHandle_t = void*;
inline unsigned long millis() {
    return FakeTask::now;
}
inline const char* pcTaskGetName(void*) {
    return "test-input";
}
inline int xPortGetCoreID() {
    return 0;
}
inline BaseType_t xTaskCreate(void (*entry)(void*), const char*, uint32_t, void*, UBaseType_t, TaskHandle_t* task) {
    FakeTask::entry = entry;
    *task = &FakeTask::entry;
    return pdPASS;
}
inline void vTaskDelete(TaskHandle_t) {}
inline void vTaskDelay(TickType_t ticks) {
    FakeTask::now += ticks;
}
inline void vTaskNotifyGiveFromISR(TaskHandle_t, BaseType_t*) {
    ++FakeTask::notifications;
}
inline uint32_t ulTaskNotifyTake(BaseType_t, TickType_t timeout) {
    if (FakeTask::notifications == 0) {
        FakeTask::now += timeout;
        if (FakeTask::tick)
            FakeTask::tick();
    }
    const uint32_t notifications = FakeTask::notifications;
    FakeTask::notifications = 0;
    return notifications;
}
