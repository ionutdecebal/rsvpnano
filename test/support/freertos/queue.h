#pragma once

#include <cstring>
#include <deque>
#include <vector>

#include "FreeRTOS.h"

struct FakeQueue {
    size_t capacity;
    size_t itemSize;
    std::deque<std::vector<uint8_t>> items;
};
using QueueHandle_t = FakeQueue*;
inline QueueHandle_t xQueueCreate(size_t capacity, size_t size) {
    return new FakeQueue{capacity, size, {}};
}
inline BaseType_t xQueueSend(QueueHandle_t queue, const void* value, TickType_t) {
    if (queue->items.size() == queue->capacity)
        return pdFALSE;
    const auto* bytes = static_cast<const uint8_t*>(value);
    queue->items.emplace_back(bytes, bytes + queue->itemSize);
    return pdTRUE;
}
inline BaseType_t xQueueReceive(QueueHandle_t queue, void* value, TickType_t) {
    if (queue->items.empty())
        return pdFALSE;
    std::memcpy(value, queue->items.front().data(), queue->itemSize);
    queue->items.pop_front();
    return pdTRUE;
}
inline void xQueueReset(QueueHandle_t queue) {
    queue->items.clear();
}
inline void vQueueDelete(QueueHandle_t queue) {
    delete queue;
}
