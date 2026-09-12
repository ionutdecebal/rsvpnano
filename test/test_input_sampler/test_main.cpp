#include <unity.h>

#include <vector>

// Run the real sampler, replacing only its hardware and RTOS environment.
#include "../../src/input/Input.cpp"

namespace {
    struct Finished {};
    enum class Scenario {
        edge,
        duringRead,
        retry,
        hold,
        polling
    };
    Scenario scenario;
    bool pending;
    std::vector<uint32_t> reads;
    std::vector<uint32_t> controlReads;

    void interrupt() {
        pending = true;
        Input::notifyTouchFromISR();
    }

    void tick() {
        if (FakeTask::now == 3 && scenario != Scenario::polling)
            interrupt();
        // IRQ noise during backoff must not cause another immediate failed transfer.
        if (scenario == Scenario::retry && FakeTask::now == 4)
            interrupt();
        if (FakeTask::now >= 85)
            throw Finished{};
    }
} // namespace

namespace Board::Input {
    bool begin() {
        return true;
    }
    void end() {}
    void cancel() {}
    ::Input::ControlTiming controlTiming() {
        return {};
    }
    ::Input::TouchTiming touchTiming() {
        return {.failureBackoffMs = 30};
    }
    ::Input::PressActions currentActions() {
        controlReads.push_back(FakeTask::now);
        return {};
    }
    ui::TouchSurface touchSurface() {
        return {480, 480};
    }
    bool beginTouch() {
        return true;
    }
    bool touchReady() {
        return scenario == Scenario::polling || pending;
    }
    bool readTouch(ui::TouchContact& contact) {
        pending = false;
        reads.push_back(FakeTask::now);
        if (scenario == Scenario::duringRead && reads.size() == 1)
            interrupt();
        if (scenario == Scenario::retry && reads.size() == 1) {
            pending = true;
            return false;
        }
        contact.touched = scenario == Scenario::hold && reads.size() <= 2;
        return true;
    }
} // namespace Board::Input

void setUp() {
    pending = false;
    reads.clear();
    controlReads.clear();
    FakeTask::now = 0;
    FakeTask::notifications = 0;
    FakeTask::tick = tick;
}

void tearDown() {
    // The fake task has exited through Finished; no concurrent task remains to acknowledge cancel().
    Input::gSamplerTask = nullptr;
    Input::end();
}

void run(Scenario selected) {
    scenario = selected;
    TEST_ASSERT_TRUE(Input::begin());
    try {
        FakeTask::entry(nullptr);
    } catch (const Finished&) {
    }
}

void test_edge_bypasses_poll_deadline_without_idle_report_polling() {
    run(Scenario::edge);
    TEST_ASSERT_EQUAL_UINT32(1, reads.size());
    TEST_ASSERT_EQUAL_UINT32(3, reads[0]);
    TEST_ASSERT_TRUE(controlReads.size() >= 17);
}

void test_interrupt_arriving_during_read_is_retained() {
    run(Scenario::duringRead);
    TEST_ASSERT_EQUAL_UINT32(2, reads.size());
    TEST_ASSERT_EQUAL_UINT32(3, reads[0]);
    TEST_ASSERT_EQUAL_UINT32(3, reads[1]);
}

void test_failed_read_retries_without_new_edge_and_respects_backoff() {
    run(Scenario::retry);
    TEST_ASSERT_EQUAL_UINT32(2, reads.size());
    TEST_ASSERT_EQUAL_UINT32(3, reads[0]);
    TEST_ASSERT_EQUAL_UINT32(33, reads[1]);
}

void test_active_contact_keeps_sampling_and_confirms_release() {
    run(Scenario::hold);
    const std::vector<uint32_t> expected = {3, 23, 43, 63};
    TEST_ASSERT_TRUE(reads == expected);
    ui::TouchContact contact;
    TEST_ASSERT_EQUAL(ui::TouchSampleResult::Contact, Input::pollTouch(contact));
    TEST_ASSERT_TRUE(contact.touched);
    TEST_ASSERT_EQUAL(ui::TouchSampleResult::Contact, Input::pollTouch(contact));
    TEST_ASSERT_TRUE(contact.touched);
    TEST_ASSERT_EQUAL(ui::TouchSampleResult::Contact, Input::pollTouch(contact));
    TEST_ASSERT_FALSE(contact.touched);
    TEST_ASSERT_EQUAL(ui::TouchSampleResult::None, Input::pollTouch(contact));
}

void test_polling_boards_keep_existing_periodic_reads() {
    run(Scenario::polling);
    const std::vector<uint32_t> expected = {0, 20, 40, 60, 80};
    TEST_ASSERT_TRUE(reads == expected);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_edge_bypasses_poll_deadline_without_idle_report_polling);
    RUN_TEST(test_interrupt_arriving_during_read_is_retained);
    RUN_TEST(test_failed_read_retries_without_new_edge_and_respects_backoff);
    RUN_TEST(test_active_contact_keeps_sampling_and_confirms_release);
    RUN_TEST(test_polling_boards_keep_existing_periodic_reads);
    return UNITY_END();
}
