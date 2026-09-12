#include <array>
#include <unity.h>

// Exercise the production routine with the GPIO behavior that matters here:
// IDF wake setup changes the normal trigger; wake disable does not restore it;
// Arduino pinMode preserves that trigger and enables a non-disabled interrupt.
#include "../../src/sleep/EspLightSleep.cpp"

namespace {
    struct Pin {
        gpio_int_type_t trigger = GPIO_INTR_DISABLE;
        bool interruptEnabled = false;
        bool wakeEnabled = false;
    };
    enum class Failure {
        none,
        pinArm,
        gpioArm,
        timerArm,
        sleep
    };
    std::array<Pin, 2> pins;
    constexpr std::array wakePins = {GPIO_NUM_0, GPIO_NUM_1};
    Failure failure;
    esp_sleep_wakeup_cause_t wakeCause;
    bool gpioWakeEnabled;
    bool timerWakeEnabled;
    uint64_t timerMicroseconds;
    unsigned sleepCalls;
    unsigned pinModeEnabledInterrupts;

    void assertCleanedUp() {
        for (const Pin& pin: pins) {
            TEST_ASSERT_FALSE(pin.wakeEnabled);
            TEST_ASSERT_FALSE(pin.interruptEnabled);
            TEST_ASSERT_EQUAL(GPIO_INTR_DISABLE, pin.trigger);
        }
        TEST_ASSERT_FALSE(gpioWakeEnabled);
        TEST_ASSERT_FALSE(timerWakeEnabled);
    }
} // namespace

void pinMode(int pin, int) {
    pins[pin].interruptEnabled = pins[pin].trigger != GPIO_INTR_DISABLE;
    if (pins[pin].interruptEnabled)
        ++pinModeEnabledInterrupts;
}
int digitalRead(int) {
    return 1;
}
void delay(unsigned long) {}

esp_err_t gpio_wakeup_enable(gpio_num_t pin, gpio_int_type_t type) {
    if (failure == Failure::pinArm && pin == GPIO_NUM_1)
        return ESP_FAIL;
    pins[pin].trigger = type;
    pins[pin].wakeEnabled = true;
    return ESP_OK;
}
esp_err_t gpio_wakeup_disable(gpio_num_t pin) {
    pins[pin].wakeEnabled = false;
    return ESP_OK;
}
esp_err_t gpio_intr_disable(gpio_num_t pin) {
    pins[pin].interruptEnabled = false;
    return ESP_OK;
}
esp_err_t gpio_set_intr_type(gpio_num_t pin, gpio_int_type_t type) {
    pins[pin].trigger = type;
    return ESP_OK;
}
esp_err_t esp_sleep_enable_gpio_wakeup() {
    if (failure == Failure::gpioArm)
        return ESP_FAIL;
    gpioWakeEnabled = true;
    return ESP_OK;
}
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t microseconds) {
    if (failure == Failure::timerArm)
        return ESP_FAIL;
    timerMicroseconds = microseconds;
    timerWakeEnabled = true;
    return ESP_OK;
}
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_wakeup_cause_t source) {
    if (source == ESP_SLEEP_WAKEUP_GPIO)
        gpioWakeEnabled = false;
    if (source == ESP_SLEEP_WAKEUP_TIMER)
        timerWakeEnabled = false;
    return ESP_OK;
}
esp_err_t esp_light_sleep_start() {
    ++sleepCalls;
    return failure == Failure::sleep ? ESP_FAIL : ESP_OK;
}
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() {
    return wakeCause;
}

void setUp() {
    pins = {};
    failure = Failure::none;
    wakeCause = ESP_SLEEP_WAKEUP_GPIO;
    gpioWakeEnabled = timerWakeEnabled = false;
    timerMicroseconds = 0;
    sleepCalls = pinModeEnabledInterrupts = 0;
}
void tearDown() {}

void test_repeated_gpio_wake_does_not_reenable_level_interrupt() {
    TEST_ASSERT_EQUAL(EspLightSleep::WakeReason::input, EspLightSleep::wait(wakePins, 250));
    assertCleanedUp();
    TEST_ASSERT_EQUAL(EspLightSleep::WakeReason::input, EspLightSleep::wait(wakePins, 250));
    assertCleanedUp();
    TEST_ASSERT_EQUAL_UINT32(2, sleepCalls);
    TEST_ASSERT_EQUAL_UINT32(0, pinModeEnabledInterrupts);
    TEST_ASSERT_EQUAL_UINT64(250000, timerMicroseconds);
}

void test_timer_wake_cleans_up() {
    wakeCause = ESP_SLEEP_WAKEUP_TIMER;
    TEST_ASSERT_EQUAL(EspLightSleep::WakeReason::timer, EspLightSleep::wait(wakePins, 250));
    assertCleanedUp();
}

void test_indefinite_sleep_does_not_arm_timer() {
    TEST_ASSERT_EQUAL(EspLightSleep::WakeReason::input, EspLightSleep::wait(wakePins, 0));
    TEST_ASSERT_EQUAL_UINT64(0, timerMicroseconds);
    assertCleanedUp();
}

void test_setup_and_sleep_failures_clean_up() {
    for (const Failure selected: {Failure::pinArm, Failure::gpioArm, Failure::timerArm, Failure::sleep}) {
        setUp();
        failure = selected;
        TEST_ASSERT_EQUAL(EspLightSleep::WakeReason::error, EspLightSleep::wait(wakePins, 250));
        assertCleanedUp();
        TEST_ASSERT_EQUAL_UINT32(selected == Failure::sleep ? 1 : 0, sleepCalls);
    }
}

void test_unexpected_wake_cause_cleans_up() {
    wakeCause = ESP_SLEEP_WAKEUP_UNDEFINED;
    TEST_ASSERT_EQUAL(EspLightSleep::WakeReason::error, EspLightSleep::wait(wakePins, 250));
    assertCleanedUp();
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_repeated_gpio_wake_does_not_reenable_level_interrupt);
    RUN_TEST(test_timer_wake_cleans_up);
    RUN_TEST(test_indefinite_sleep_does_not_arm_timer);
    RUN_TEST(test_setup_and_sleep_failures_clean_up);
    RUN_TEST(test_unexpected_wake_cause_cleans_up);
    return UNITY_END();
}
