#include "TimekeeperController.h"

#include <algorithm>
#include <stdlib.h>

void TimekeeperController::begin(uint32_t sessionSeed) {
  sessionSequence_ = sessionSeed;
  clearTiming();
}

void TimekeeperController::clearTiming() {
  state_ = TimekeeperState::Ready;
  startedAtMs_ = 0;
  accumulatedMs_ = 0;
  lastLapElapsedMs_ = 0;
  previousLapDurationMs_ = 0;
  bestLapDurationMs_ = 0;
  lapCount_ = 0;
  nextMilestoneMs_ = 60ULL * 1000ULL;
  countdownMilestoneMask_ = 0;
  pomodoroCycleIndex_ = 0;
  pomodoroPhase_ = TimekeeperPomodoroPhase::None;
  pomodoroMilestoneMask_ = 0;
}

void TimekeeperController::startNewSession(uint64_t nowMs) {
  ++sessionSequence_;
  if (sessionSequence_ == 0) {
    ++sessionSequence_;
  }
  accumulatedMs_ = 0;
  startedAtMs_ = nowMs;
  lastLapElapsedMs_ = 0;
  previousLapDurationMs_ = 0;
  bestLapDurationMs_ = 0;
  lapCount_ = 0;
  nextMilestoneMs_ = 60ULL * 1000ULL;
  countdownMilestoneMask_ = 0;
  if (activity_ == TimekeeperActivity::Pomodoro) {
    pomodoroActiveWorkDurationMs_ = pomodoroWorkDurationMs_;
    pomodoroActiveBreakDurationMs_ = pomodoroBreakDurationMs_;
    pomodoroActiveConfigRevision_ = pomodoroConfigRevision_;
    pomodoroActiveCycles_ = pomodoroCycles_;
    pomodoroCycleIndex_ = 1;
    pomodoroPhase_ = TimekeeperPomodoroPhase::Work;
    pomodoroMilestoneMask_ = 0;
  }
  state_ = TimekeeperState::Running;
}

bool TimekeeperController::selectActivity(TimekeeperActivity activity,
                                          uint64_t nowMs,
                                          TimekeeperEvent* resetEvent) {
  if (activity == activity_ || !canChangeActivity()) {
    return false;
  }
  if (resetEvent != nullptr && state_ != TimekeeperState::Ready) {
    *resetEvent = makeEvent(TimekeeperEventType::Reset, nowMs);
  }
  activity_ = activity;
  clearTiming();
  return true;
}

TimekeeperEvent TimekeeperController::toggle(uint64_t nowMs) {
  if (state_ == TimekeeperState::Running) {
    if (activity_ == TimekeeperActivity::TenSecondChallenge) {
      accumulatedMs_ = elapsedMs(nowMs);
      state_ = TimekeeperState::Finished;
      TimekeeperEvent event = makeEvent(TimekeeperEventType::Result, nowMs);
      event.signedErrorMs = static_cast<int64_t>(event.elapsedMs) -
                            static_cast<int64_t>(challengeTargetMs_);
      event.absoluteErrorMs = static_cast<uint64_t>(llabs(event.signedErrorMs));
      event.rank = challengeRank(event.absoluteErrorMs);
      return event;
    }
    accumulatedMs_ = elapsedMs(nowMs);
    state_ = TimekeeperState::Paused;
    return makeEvent(TimekeeperEventType::Paused, nowMs);
  }

  if (state_ == TimekeeperState::Paused) {
    startedAtMs_ = nowMs;
    state_ = TimekeeperState::Running;
    return makeEvent(TimekeeperEventType::Resumed, nowMs);
  }

  startNewSession(nowMs);
  return makeEvent(TimekeeperEventType::Started, nowMs);
}

TimekeeperEvent TimekeeperController::reset(uint64_t nowMs) {
  if (state_ == TimekeeperState::Running) {
    return TimekeeperEvent();
  }
  clearTiming();
  return makeEvent(TimekeeperEventType::Reset, nowMs);
}

TimekeeperEvent TimekeeperController::lap(uint64_t nowMs) {
  if (activity_ != TimekeeperActivity::Stopwatch || state_ != TimekeeperState::Running) {
    return TimekeeperEvent();
  }
  const uint64_t elapsed = elapsedMs(nowMs);
  const uint64_t duration = elapsed - lastLapElapsedMs_;
  ++lapCount_;

  TimekeeperEvent event = makeEvent(TimekeeperEventType::Lap, nowMs);
  event.elapsedMs = elapsed;
  event.lapIndex = lapCount_;
  event.lapDurationMs = duration;
  event.previousLapDurationMs = previousLapDurationMs_;
  event.lapDeltaMs = previousLapDurationMs_ == 0
                       ? 0
                       : static_cast<int64_t>(duration) -
                           static_cast<int64_t>(previousLapDurationMs_);
  event.isBestLap = bestLapDurationMs_ == 0 || duration < bestLapDurationMs_;

  if (event.isBestLap) {
    bestLapDurationMs_ = duration;
  }
  previousLapDurationMs_ = duration;
  lastLapElapsedMs_ = elapsed;
  return event;
}

TimekeeperEvent TimekeeperController::suspend(uint64_t nowMs, const char* reason) {
  if (state_ != TimekeeperState::Running) {
    return TimekeeperEvent();
  }
  accumulatedMs_ = elapsedMs(nowMs);
  if (activity_ == TimekeeperActivity::TenSecondChallenge) {
    state_ = TimekeeperState::Aborted;
    TimekeeperEvent event = makeEvent(TimekeeperEventType::Aborted, nowMs);
    event.reason = reason;
    return event;
  }
  state_ = TimekeeperState::Paused;
  TimekeeperEvent event = makeEvent(TimekeeperEventType::Paused, nowMs);
  event.reason = reason;
  return event;
}

TimekeeperEvent TimekeeperController::update(uint64_t nowMs) {
  if (state_ != TimekeeperState::Running) {
    return TimekeeperEvent();
  }

  if (activity_ == TimekeeperActivity::Countdown && remainingMs(nowMs) == 0) {
    accumulatedMs_ = countdownDurationMs_;
    state_ = TimekeeperState::Finished;
    return makeEvent(TimekeeperEventType::Finished, nowMs);
  }

  if (activity_ == TimekeeperActivity::Pomodoro && remainingMs(nowMs) == 0) {
    const uint64_t finishedPhaseDuration = pomodoroPhase_ == TimekeeperPomodoroPhase::Break
                                             ? pomodoroActiveBreakDurationMs_
                                             : pomodoroActiveWorkDurationMs_;
    const uint64_t phaseElapsed = elapsedMs(nowMs);
    const uint64_t overrunMs = phaseElapsed > finishedPhaseDuration
                                 ? phaseElapsed - finishedPhaseDuration
                                 : 0;
    if (pomodoroPhase_ == TimekeeperPomodoroPhase::Work &&
        pomodoroCycleIndex_ >= pomodoroActiveCycles_) {
      accumulatedMs_ = finishedPhaseDuration;
      startedAtMs_ = 0;
      pomodoroPhase_ = TimekeeperPomodoroPhase::None;
      state_ = TimekeeperState::Completed;
      return makeEvent(TimekeeperEventType::Completed, nowMs);
    }

    TimekeeperEventType eventType = TimekeeperEventType::Transition;
    TimekeeperTransition transition = TimekeeperTransition::None;
    if (pomodoroPhase_ == TimekeeperPomodoroPhase::Work) {
      pomodoroPhase_ = TimekeeperPomodoroPhase::Break;
      transition = TimekeeperTransition::WorkToBreak;
    } else {
      pomodoroPhase_ = TimekeeperPomodoroPhase::Work;
      ++pomodoroCycleIndex_;
      transition = TimekeeperTransition::BreakToWork;
    }
    accumulatedMs_ = overrunMs;
    startedAtMs_ = nowMs;
    pomodoroMilestoneMask_ = 0;
    TimekeeperEvent event = makeEvent(eventType, nowMs);
    event.transition = transition;
    return event;
  }

  if (activity_ == TimekeeperActivity::Pomodoro) {
    const uint64_t remaining = remainingMs(nowMs);
    const uint64_t phaseDuration = pomodoroPhase_ == TimekeeperPomodoroPhase::Break
                                     ? pomodoroActiveBreakDurationMs_
                                     : pomodoroActiveWorkDurationMs_;
    struct PomodoroMilestoneCandidate {
      uint64_t thresholdMs;
      TimekeeperMilestone milestone;
      uint8_t mask;
    };
    PomodoroMilestoneCandidate candidates[3];
    size_t candidateCount = 0;
    if (pomodoroPhase_ == TimekeeperPomodoroPhase::Work) {
      const uint64_t halfwayRemainingMs = phaseDuration / 2ULL;
      const bool halfwayDuplicatesSpecific =
        halfwayRemainingMs == 5ULL * 60ULL * 1000ULL ||
        halfwayRemainingMs == 60ULL * 1000ULL;
      candidates[candidateCount++] = {
        halfwayDuplicatesSpecific ? 0 : halfwayRemainingMs,
        TimekeeperMilestone::PomodoroWorkHalf,
        1U << 0,
      };
      candidates[candidateCount++] = {
        phaseDuration > 5ULL * 60ULL * 1000ULL ? 5ULL * 60ULL * 1000ULL : 0,
        TimekeeperMilestone::PomodoroWorkRemaining5Minutes,
        1U << 1,
      };
      candidates[candidateCount++] = {
        phaseDuration > 60ULL * 1000ULL ? 60ULL * 1000ULL : 0,
        TimekeeperMilestone::PomodoroWorkFinishingSoon,
        1U << 2,
      };
    } else {
      candidates[candidateCount++] = {
        phaseDuration > 60ULL * 1000ULL ? 60ULL * 1000ULL : 0,
        TimekeeperMilestone::PomodoroBreakRemaining1Minute,
        1U << 3,
      };
      candidates[candidateCount++] = {
        phaseDuration > 10ULL * 1000ULL ? 10ULL * 1000ULL : 0,
        TimekeeperMilestone::PomodoroBreakFinishingSoon,
        1U << 4,
      };
    }

    const PomodoroMilestoneCandidate* selected = nullptr;
    for (size_t index = 0; index < candidateCount; ++index) {
      const PomodoroMilestoneCandidate& candidate = candidates[index];
      if (candidate.thresholdMs == 0 ||
          (pomodoroMilestoneMask_ & candidate.mask) != 0 ||
          remaining > candidate.thresholdMs) {
        continue;
      }
      pomodoroMilestoneMask_ |= candidate.mask;
      selected = &candidate;
    }
    if (selected != nullptr) {
      TimekeeperEvent event = makeEvent(TimekeeperEventType::Milestone, nowMs);
      event.remainingMs = selected->thresholdMs;
      event.milestone = selected->milestone;
      return event;
    }
  }

  if (activity_ == TimekeeperActivity::Countdown) {
    const uint64_t remaining = remainingMs(nowMs);
    struct CountdownMilestoneCandidate {
      uint64_t thresholdMs;
      TimekeeperMilestone milestone;
      uint8_t mask;
    };
    const uint64_t halfwayMs = countdownDurationMs_ / 2ULL;
    const bool halfwayHasSpecificName =
      (halfwayMs == 5ULL * 60ULL * 1000ULL &&
       countdownDurationMs_ >= 15ULL * 60ULL * 1000ULL) ||
      halfwayMs == 60ULL * 1000ULL ||
      halfwayMs == 30ULL * 1000ULL ||
      halfwayMs == 10ULL * 1000ULL;
    const CountdownMilestoneCandidate candidates[] = {
      {halfwayHasSpecificName ? 0 : halfwayMs, TimekeeperMilestone::Halfway, 1U << 0},
      {countdownDurationMs_ >= 15ULL * 60ULL * 1000ULL
         ? 5ULL * 60ULL * 1000ULL : 0,
       TimekeeperMilestone::Remaining5Minutes,
       1U << 1},
      {60ULL * 1000ULL, TimekeeperMilestone::Remaining1Minute, 1U << 2},
      {30ULL * 1000ULL, TimekeeperMilestone::Remaining30Seconds, 1U << 3},
      {10ULL * 1000ULL, TimekeeperMilestone::Remaining10Seconds, 1U << 4},
    };

    const CountdownMilestoneCandidate* selected = nullptr;
    for (const CountdownMilestoneCandidate& candidate : candidates) {
      if (candidate.thresholdMs == 0 || candidate.thresholdMs >= countdownDurationMs_ ||
          (countdownMilestoneMask_ & candidate.mask) != 0 ||
          remaining > candidate.thresholdMs) {
        continue;
      }
      countdownMilestoneMask_ |= candidate.mask;
      // Candidates are ordered from earlier to later. If several thresholds
      // were crossed between loop iterations, announce only the most recent
      // one instead of speaking several stale milestones back-to-back.
      selected = &candidate;
    }
    if (selected != nullptr) {
      TimekeeperEvent event = makeEvent(TimekeeperEventType::Milestone, nowMs);
      event.remainingMs = selected->thresholdMs;
      event.milestone = selected->milestone;
      return event;
    }
  }

  if (activity_ == TimekeeperActivity::Stopwatch) {
    const uint64_t elapsed = elapsedMs(nowMs);
    if (elapsed >= nextMilestoneMs_) {
      TimekeeperEvent event = makeEvent(TimekeeperEventType::Milestone, nowMs);
      event.elapsedMs = elapsed;
      nextMilestoneMs_ = elapsed < 5ULL * 60ULL * 1000ULL
                           ? 5ULL * 60ULL * 1000ULL
                           : ((elapsed / (5ULL * 60ULL * 1000ULL)) + 1ULL) *
                               (5ULL * 60ULL * 1000ULL);
      return event;
    }
  }
  return TimekeeperEvent();
}

uint64_t TimekeeperController::elapsedMs(uint64_t nowMs) const {
  if (state_ != TimekeeperState::Running || startedAtMs_ == 0) {
    return accumulatedMs_;
  }
  return accumulatedMs_ + (nowMs - startedAtMs_);
}

uint64_t TimekeeperController::remainingMs(uint64_t nowMs) const {
  const uint64_t elapsed = elapsedMs(nowMs);
  uint64_t durationMs = countdownDurationMs_;
  if (activity_ == TimekeeperActivity::Pomodoro) {
    if (pomodoroPhase_ == TimekeeperPomodoroPhase::Break) {
      durationMs = pomodoroActiveBreakDurationMs_;
    } else if (pomodoroPhase_ == TimekeeperPomodoroPhase::Work ||
               state_ != TimekeeperState::Ready) {
      durationMs = pomodoroActiveWorkDurationMs_;
    } else {
      durationMs = pomodoroWorkDurationMs_;
    }
  }
  return elapsed >= durationMs ? 0 : durationMs - elapsed;
}

uint64_t TimekeeperController::displayMs(uint64_t nowMs) const {
  if (activity_ == TimekeeperActivity::Countdown ||
      activity_ == TimekeeperActivity::Pomodoro) {
    return remainingMs(nowMs);
  }
  return elapsedMs(nowMs);
}

bool TimekeeperController::adjustCountdownMinutes(int deltaMinutes, uint64_t nowMs) {
  if (activity_ != TimekeeperActivity::Countdown || state_ == TimekeeperState::Running ||
      deltaMinutes == 0) {
    return false;
  }
  const int64_t adjusted = static_cast<int64_t>(countdownDurationMs_) +
                           static_cast<int64_t>(deltaMinutes) *
                             static_cast<int64_t>(kCountdownStepMs);
  countdownDurationMs_ = static_cast<uint64_t>(std::max<int64_t>(
    static_cast<int64_t>(kMinCountdownDurationMs),
    std::min<int64_t>(static_cast<int64_t>(kMaxCountdownDurationMs), adjusted)));
  (void)nowMs;
  clearTiming();
  return true;
}

bool TimekeeperController::setCountdownMinutes(uint16_t minutes, uint64_t nowMs) {
  if (activity_ != TimekeeperActivity::Countdown || state_ == TimekeeperState::Running) {
    return false;
  }
  const uint64_t requestedMs = static_cast<uint64_t>(minutes) * 60ULL * 1000ULL;
  countdownDurationMs_ = std::max<uint64_t>(
    kMinCountdownDurationMs,
    std::min<uint64_t>(kMaxCountdownDurationMs, requestedMs));
  (void)nowMs;
  clearTiming();
  return true;
}

bool TimekeeperController::setChallengeTargetSeconds(uint16_t seconds, uint64_t nowMs) {
  if (activity_ != TimekeeperActivity::TenSecondChallenge ||
      state_ == TimekeeperState::Running ||
      (seconds != 10 && seconds != 30 && seconds != 60)) {
    return false;
  }
  challengeTargetMs_ = static_cast<uint64_t>(seconds) * 1000ULL;
  (void)nowMs;
  clearTiming();
  return true;
}

bool TimekeeperController::cycleChallengeDifficulty(uint64_t nowMs) {
  if (activity_ != TimekeeperActivity::TenSecondChallenge ||
      state_ == TimekeeperState::Running) {
    return false;
  }
  switch (challengeDifficulty_) {
    case TimekeeperChallengeDifficulty::Low:
      challengeDifficulty_ = TimekeeperChallengeDifficulty::Medium;
      break;
    case TimekeeperChallengeDifficulty::Medium:
      challengeDifficulty_ = TimekeeperChallengeDifficulty::High;
      break;
    case TimekeeperChallengeDifficulty::High:
      challengeDifficulty_ = TimekeeperChallengeDifficulty::Low;
      break;
  }
  (void)nowMs;
  clearTiming();
  return true;
}

bool TimekeeperController::configurePomodoro(uint64_t workDurationMs,
                                             uint64_t breakDurationMs,
                                             uint32_t configRevision) {
  if (workDurationMs < kMinPomodoroWorkDurationMs ||
      workDurationMs > kMaxPomodoroWorkDurationMs ||
      breakDurationMs < kMinPomodoroBreakDurationMs ||
      breakDurationMs > kMaxPomodoroBreakDurationMs ||
      configRevision == 0) {
    return false;
  }
  pomodoroWorkDurationMs_ = workDurationMs;
  pomodoroBreakDurationMs_ = breakDurationMs;
  pomodoroConfigRevision_ = configRevision;
  return true;
}

bool TimekeeperController::adjustPomodoroCycles(int deltaCycles, uint64_t nowMs) {
  if (activity_ != TimekeeperActivity::Pomodoro ||
      state_ != TimekeeperState::Ready || deltaCycles == 0) {
    return false;
  }
  const int adjusted = std::max<int>(
    kMinPomodoroCycles,
    std::min<int>(kMaxPomodoroCycles,
                  static_cast<int>(pomodoroCycles_) + deltaCycles));
  if (adjusted == pomodoroCycles_) {
    return false;
  }
  pomodoroCycles_ = static_cast<uint8_t>(adjusted);
  (void)nowMs;
  return true;
}

bool TimekeeperController::setPomodoroCycles(uint8_t cycles, uint64_t nowMs) {
  if (state_ == TimekeeperState::Running || state_ == TimekeeperState::Paused ||
      cycles < kMinPomodoroCycles || cycles > kMaxPomodoroCycles) {
    return false;
  }
  pomodoroCycles_ = cycles;
  (void)nowMs;
  return true;
}

TimekeeperEvent TimekeeperController::makeEvent(TimekeeperEventType type,
                                                uint64_t nowMs) const {
  TimekeeperEvent event;
  event.type = type;
  event.activity = activity_;
  event.state = state_;
  event.sessionSequence = sessionSequence_;
  event.createdAtMs = nowMs;
  event.elapsedMs = elapsedMs(nowMs);
  event.remainingMs = activity_ == TimekeeperActivity::Countdown ? remainingMs(nowMs) : 0;
  event.durationMs = activity_ == TimekeeperActivity::Countdown
                       ? countdownDurationMs_
                       : (activity_ == TimekeeperActivity::TenSecondChallenge
                            ? challengeTargetMs_
                            : 0);
  event.challengeDifficulty = challengeDifficulty_;
  if (activity_ == TimekeeperActivity::Pomodoro) {
    const bool useSavedConfig = state_ == TimekeeperState::Ready;
    const uint8_t eventTotalCycles = useSavedConfig
                                       ? pomodoroCycles_
                                       : pomodoroActiveCycles_;
    event.elapsedMs = elapsedMs(nowMs);
    event.remainingMs = type == TimekeeperEventType::Completed ? 0 : remainingMs(nowMs);
    event.pomodoroPhase = pomodoroPhase_;
    event.cycleIndex = pomodoroCycleIndex_;
    event.totalCycles = eventTotalCycles;
    event.remainingCycles = pomodoroCycleIndex_ >= eventTotalCycles
                              ? 0
                              : eventTotalCycles - pomodoroCycleIndex_;
    event.isFinalCycle = pomodoroPhase_ == TimekeeperPomodoroPhase::Work &&
                         pomodoroCycleIndex_ == eventTotalCycles;
    event.workDurationMs = useSavedConfig
                             ? pomodoroWorkDurationMs_
                             : pomodoroActiveWorkDurationMs_;
    event.breakDurationMs = useSavedConfig
                              ? pomodoroBreakDurationMs_
                              : pomodoroActiveBreakDurationMs_;
    event.phaseDurationMs = pomodoroPhase_ == TimekeeperPomodoroPhase::Work
                              ? pomodoroActiveWorkDurationMs_
                              : (pomodoroPhase_ == TimekeeperPomodoroPhase::Break
                                   ? pomodoroActiveBreakDurationMs_
                                   : 0);
    event.configRevision = useSavedConfig
                             ? pomodoroConfigRevision_
                             : pomodoroActiveConfigRevision_;
  }
  return event;
}

const char* TimekeeperController::challengeRank(uint64_t absoluteErrorMs) const {
  if (absoluteErrorMs == 0) {
    return "perfect";
  }
  if (absoluteErrorMs <= 50) {
    return "amazing";
  }
  if (absoluteErrorMs <= 200) {
    return "excellent";
  }
  if (absoluteErrorMs <= 500) {
    return "close";
  }
  if (absoluteErrorMs <= 1000) {
    return "near";
  }
  return "try_again";
}
