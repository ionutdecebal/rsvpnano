#pragma once

#include <cstdint>

#include "focus/FocusTimers.h"

namespace focus {

    enum class Orientation : uint8_t {
        Unknown,
        ShortA,
        ShortB,
        Flat,
    };

    enum class Phase : uint8_t {
        WaitingFocus,
        Focus,
        PausedFocus,
        WaitingBreak,
        Break,
        PausedBreak,
        Complete,
    };

    class Session {
    public:
        void begin(const Timer& timer);
        void beginManual(const Timer& timer, uint32_t nowMs);
        void resume(uint32_t nowMs);
        void pause(uint32_t nowMs);
        void update(uint32_t nowMs, Orientation orientation);
        void stop();

        Phase phase() const {
            return phase_;
        }
        uint8_t round() const {
            return round_;
        }
        uint8_t rounds() const {
            return rounds_;
        }
        uint32_t remainingMs(uint32_t nowMs) const;
        uint16_t progressPermille(uint32_t nowMs) const;
        bool consumeCompletionCue();

    private:
        static bool shortSide(Orientation orientation);
        static Orientation opposite(Orientation orientation);
        void startPhase(Phase phase, uint32_t nowMs, Orientation orientation);
        void finishPhase(Orientation orientation);

        Phase phase_ = Phase::Complete;
        Orientation activeSide_ = Orientation::Unknown;
        Orientation waitTarget_ = Orientation::Unknown;
        uint32_t startedMs_ = 0;
        uint32_t durationMs_ = 0;
        uint32_t pausedRemainingMs_ = 0;
        uint32_t focusDurationMs_ = 0;
        uint32_t breakDurationMs_ = 0;
        uint8_t round_ = 0;
        uint8_t rounds_ = 0;
        bool targetPresentAtWaitStart_ = false;
        bool completionCuePending_ = false;
        bool manual_ = false;
    };

} // namespace focus
