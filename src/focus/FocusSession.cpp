#include "focus/FocusSession.h"

#include <algorithm>

namespace focus {

    void Session::begin(const Timer& timer) {
        manual_ = false;
        focusDurationMs_ = static_cast<uint32_t>(timer.focusMinutes) * 60UL * 1000UL;
        breakDurationMs_ = static_cast<uint32_t>(timer.breakMinutes) * 60UL * 1000UL;
        rounds_ = timer.rounds;
        round_ = 1;
        phase_ = Phase::WaitingFocus;
        activeSide_ = Orientation::Unknown;
        waitTarget_ = Orientation::Unknown;
        startedMs_ = 0;
        durationMs_ = 0;
        pausedRemainingMs_ = 0;
        targetPresentAtWaitStart_ = false;
        completionCuePending_ = false;
    }

    void Session::beginManual(const Timer& timer, uint32_t nowMs) {
        begin(timer);
        manual_ = true;
        startPhase(Phase::Focus, nowMs, Orientation::Unknown);
    }

    void Session::pause(uint32_t nowMs) {
        if (!manual_ || phase_ != Phase::Focus) return;
        pausedRemainingMs_ = remainingMs(nowMs);
        phase_ = Phase::PausedFocus;
    }

    void Session::resume(uint32_t nowMs) {
        if (!manual_) return;
        if (phase_ == Phase::WaitingFocus) {
            startPhase(Phase::Focus, nowMs, Orientation::Unknown);
        } else if (phase_ == Phase::PausedFocus) {
            durationMs_ = pausedRemainingMs_;
            startedMs_ = nowMs;
            phase_ = Phase::Focus;
        }
    }

    void Session::update(uint32_t nowMs, Orientation orientation) {
        if (manual_) {
            if ((phase_ != Phase::Focus && phase_ != Phase::Break) || nowMs - startedMs_ < durationMs_)
                return;
            completionCuePending_ = true;
            if (phase_ == Phase::Focus) {
                // Every round ends with a break. Reading cannot restart on its own.
                startPhase(Phase::Break, nowMs, Orientation::Unknown);
            } else {
                phase_ = round_ >= rounds_ ? Phase::Complete : Phase::WaitingFocus;
                if (phase_ == Phase::WaitingFocus) ++round_;
                durationMs_ = pausedRemainingMs_ = 0;
            }
            return;
        }
        if ((phase_ == Phase::Focus || phase_ == Phase::Break) && nowMs - startedMs_ >= durationMs_) {
            finishPhase(orientation);
            completionCuePending_ = true;
            return;
        }

        switch (phase_) {
        case Phase::WaitingFocus:
        case Phase::WaitingBreak:
            if (!shortSide(orientation) || orientation != waitTarget_)
                targetPresentAtWaitStart_ = false;
            if (shortSide(orientation) && (waitTarget_ == Orientation::Unknown || orientation == waitTarget_)
                && !targetPresentAtWaitStart_) {
                startPhase(phase_ == Phase::WaitingFocus ? Phase::Focus : Phase::Break, nowMs, orientation);
            }
            break;
        case Phase::Focus:
        case Phase::Break:
            if (orientation == Orientation::Flat) {
                pausedRemainingMs_ = remainingMs(nowMs);
                phase_ = phase_ == Phase::Focus ? Phase::PausedFocus : Phase::PausedBreak;
            }
            break;
        case Phase::PausedFocus:
        case Phase::PausedBreak:
            if (orientation == activeSide_) {
                durationMs_ = pausedRemainingMs_;
                startedMs_ = nowMs;
                phase_ = phase_ == Phase::PausedFocus ? Phase::Focus : Phase::Break;
            }
            break;
        case Phase::Complete:
            break;
        }
    }

    void Session::stop() {
        phase_ = Phase::Complete;
        completionCuePending_ = false;
    }

    uint32_t Session::remainingMs(uint32_t nowMs) const {
        if (phase_ == Phase::PausedFocus || phase_ == Phase::PausedBreak)
            return pausedRemainingMs_;
        if (phase_ != Phase::Focus && phase_ != Phase::Break)
            return 0;
        const uint32_t elapsed = nowMs - startedMs_;
        return elapsed >= durationMs_ ? 0 : durationMs_ - elapsed;
    }

    uint16_t Session::progressPermille(uint32_t nowMs) const {
        if (phase_ == Phase::WaitingBreak)
            return 1000;
        if (phase_ == Phase::WaitingFocus)
            return 0;
        const uint32_t total = phase_ == Phase::Focus || phase_ == Phase::PausedFocus ? focusDurationMs_
                             : phase_ == Phase::Break || phase_ == Phase::PausedBreak ? breakDurationMs_
                                                                                      : 0;
        if (total == 0)
            return phase_ == Phase::Complete ? 1000 : 0;
        const uint32_t remaining = remainingMs(nowMs);
        return static_cast<uint16_t>(static_cast<uint64_t>(total - std::min(total, remaining)) * 1000U / total);
    }

    bool Session::consumeCompletionCue() {
        const bool pending = completionCuePending_;
        completionCuePending_ = false;
        return pending;
    }

    bool Session::shortSide(Orientation orientation) {
        return orientation == Orientation::ShortA || orientation == Orientation::ShortB;
    }

    Orientation Session::opposite(Orientation orientation) {
        return orientation == Orientation::ShortA ? Orientation::ShortB
             : orientation == Orientation::ShortB ? Orientation::ShortA
                                                  : Orientation::Unknown;
    }

    void Session::startPhase(Phase phase, uint32_t nowMs, Orientation orientation) {
        phase_ = phase;
        activeSide_ = orientation;
        startedMs_ = nowMs;
        durationMs_ = phase == Phase::Focus ? focusDurationMs_ : breakDurationMs_;
        pausedRemainingMs_ = 0;
        waitTarget_ = Orientation::Unknown;
        targetPresentAtWaitStart_ = false;
    }

    void Session::finishPhase(Orientation orientation) {
        if (phase_ == Phase::Focus && round_ >= rounds_) {
            phase_ = Phase::Complete;
            durationMs_ = 0;
            return;
        }
        if (phase_ == Phase::Break)
            ++round_;
        phase_ = phase_ == Phase::Focus ? Phase::WaitingBreak : Phase::WaitingFocus;
        waitTarget_ = opposite(activeSide_);
        targetPresentAtWaitStart_ = orientation == waitTarget_;
        durationMs_ = 0;
        pausedRemainingMs_ = 0;
    }

} // namespace focus
