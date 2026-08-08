#pragma once

#include <stdint.h>

enum class TimekeeperActivity : uint8_t {
  Stopwatch = 0,
  Countdown = 1,
  TenSecondChallenge = 2,
  Pomodoro = 3,
};

enum class TimekeeperState : uint8_t {
  Ready = 0,
  Running = 1,
  Paused = 2,
  Finished = 3,
  Aborted = 4,
  Completed = 5,
};

enum class TimekeeperPomodoroPhase : uint8_t {
  None = 0,
  Work = 1,
  Break = 2,
};

enum class TimekeeperTransition : uint8_t {
  None = 0,
  WorkToBreak = 1,
  BreakToWork = 2,
};

enum class TimekeeperChallengeDifficulty : uint8_t {
  Low = 0,
  Medium = 1,
  High = 2,
};

enum class TimekeeperEventType : uint8_t {
  None = 0,
  Started,
  Paused,
  Resumed,
  Reset,
  Lap,
  Milestone,
  Finished,
  Result,
  Aborted,
  Transition,
  Completed,
};

enum class TimekeeperMilestone : uint8_t {
  None = 0,
  Halfway,
  Remaining5Minutes,
  Remaining1Minute,
  Remaining30Seconds,
  Remaining10Seconds,
  PomodoroWorkHalf,
  PomodoroWorkRemaining5Minutes,
  PomodoroWorkFinishingSoon,
  PomodoroBreakRemaining1Minute,
  PomodoroBreakFinishingSoon,
};

struct TimekeeperEvent {
  TimekeeperEventType type = TimekeeperEventType::None;
  TimekeeperActivity activity = TimekeeperActivity::Stopwatch;
  TimekeeperState state = TimekeeperState::Ready;
  uint32_t sessionSequence = 0;
  uint64_t createdAtMs = 0;
  uint64_t elapsedMs = 0;
  uint64_t remainingMs = 0;
  uint64_t durationMs = 0;
  uint32_t lapIndex = 0;
  uint64_t lapDurationMs = 0;
  uint64_t previousLapDurationMs = 0;
  int64_t lapDeltaMs = 0;
  bool isBestLap = false;
  int64_t signedErrorMs = 0;
  uint64_t absoluteErrorMs = 0;
  TimekeeperChallengeDifficulty challengeDifficulty =
    TimekeeperChallengeDifficulty::Medium;
  TimekeeperMilestone milestone = TimekeeperMilestone::None;
  TimekeeperPomodoroPhase pomodoroPhase = TimekeeperPomodoroPhase::None;
  TimekeeperTransition transition = TimekeeperTransition::None;
  uint32_t cycleIndex = 0;
  uint32_t totalCycles = 0;
  uint32_t remainingCycles = 0;
  bool isFinalCycle = false;
  uint64_t workDurationMs = 0;
  uint64_t breakDurationMs = 0;
  uint64_t phaseDurationMs = 0;
  uint32_t configRevision = 0;
  const char* rank = nullptr;
  const char* reason = nullptr;

  bool valid() const { return type != TimekeeperEventType::None; }
};

class TimekeeperController {
public:
  static constexpr uint64_t kDefaultCountdownDurationMs = 5ULL * 60ULL * 1000ULL;
  static constexpr uint64_t kCountdownStepMs = 60ULL * 1000ULL;
  static constexpr uint64_t kMinCountdownDurationMs = 10ULL * 1000ULL;
  static constexpr uint64_t kMaxCountdownDurationMs = 120ULL * 60ULL * 1000ULL;
  static constexpr uint64_t kDefaultChallengeTargetMs = 10ULL * 1000ULL;
  static constexpr uint64_t kDefaultPomodoroWorkDurationMs = 25ULL * 60ULL * 1000ULL;
  static constexpr uint64_t kDefaultPomodoroBreakDurationMs = 5ULL * 60ULL * 1000ULL;
  static constexpr uint64_t kMinPomodoroWorkDurationMs = 60ULL * 1000ULL;
  static constexpr uint64_t kMaxPomodoroWorkDurationMs = 120ULL * 60ULL * 1000ULL;
  static constexpr uint64_t kMinPomodoroBreakDurationMs = 60ULL * 1000ULL;
  static constexpr uint64_t kMaxPomodoroBreakDurationMs = 60ULL * 60ULL * 1000ULL;
  static constexpr uint8_t kDefaultPomodoroCycles = 4;
  static constexpr uint8_t kMinPomodoroCycles = 1;
  static constexpr uint8_t kMaxPomodoroCycles = 12;

  void begin(uint32_t sessionSeed = 0);
  TimekeeperActivity activity() const { return activity_; }
  TimekeeperState state() const { return state_; }
  bool isRunning() const { return state_ == TimekeeperState::Running; }
  bool canChangeActivity() const { return state_ != TimekeeperState::Running; }

  bool selectActivity(TimekeeperActivity activity, uint64_t nowMs, TimekeeperEvent* resetEvent = nullptr);
  TimekeeperEvent toggle(uint64_t nowMs);
  TimekeeperEvent reset(uint64_t nowMs);
  TimekeeperEvent lap(uint64_t nowMs);
  TimekeeperEvent suspend(uint64_t nowMs, const char* reason);
  TimekeeperEvent update(uint64_t nowMs);

  uint64_t elapsedMs(uint64_t nowMs) const;
  uint64_t remainingMs(uint64_t nowMs) const;
  uint64_t displayMs(uint64_t nowMs) const;
  uint64_t countdownDurationMs() const { return countdownDurationMs_; }
  bool adjustCountdownMinutes(int deltaMinutes, uint64_t nowMs);
  bool setCountdownMinutes(uint16_t minutes, uint64_t nowMs);
  uint64_t challengeTargetMs() const { return challengeTargetMs_; }
  TimekeeperChallengeDifficulty challengeDifficulty() const {
    return challengeDifficulty_;
  }
  bool setChallengeTargetSeconds(uint16_t seconds, uint64_t nowMs);
  bool cycleChallengeDifficulty(uint64_t nowMs);
  bool configurePomodoro(uint64_t workDurationMs,
                         uint64_t breakDurationMs,
                         uint32_t configRevision);
  bool adjustPomodoroCycles(int deltaCycles, uint64_t nowMs);
  bool setPomodoroCycles(uint8_t cycles, uint64_t nowMs);
  uint64_t pomodoroWorkDurationMs() const { return pomodoroWorkDurationMs_; }
  uint64_t pomodoroBreakDurationMs() const { return pomodoroBreakDurationMs_; }
  uint32_t pomodoroConfigRevision() const { return pomodoroConfigRevision_; }
  uint8_t pomodoroCycles() const { return pomodoroCycles_; }
  TimekeeperPomodoroPhase pomodoroPhase() const { return pomodoroPhase_; }
  uint8_t pomodoroCycleIndex() const { return pomodoroCycleIndex_; }
  uint64_t pomodoroActiveWorkDurationMs() const { return pomodoroActiveWorkDurationMs_; }
  uint64_t pomodoroActiveBreakDurationMs() const { return pomodoroActiveBreakDurationMs_; }
  uint32_t pomodoroActiveConfigRevision() const { return pomodoroActiveConfigRevision_; }
  uint32_t sessionSequence() const { return sessionSequence_; }
  uint32_t lapCount() const { return lapCount_; }
  uint64_t lastLapDurationMs() const { return previousLapDurationMs_; }

private:
  TimekeeperEvent makeEvent(TimekeeperEventType type, uint64_t nowMs) const;
  void startNewSession(uint64_t nowMs);
  void clearTiming();
  const char* challengeRank(uint64_t absoluteErrorMs) const;

  TimekeeperActivity activity_ = TimekeeperActivity::Stopwatch;
  TimekeeperState state_ = TimekeeperState::Ready;
  uint32_t sessionSequence_ = 0;
  uint64_t startedAtMs_ = 0;
  uint64_t accumulatedMs_ = 0;
  uint64_t countdownDurationMs_ = kDefaultCountdownDurationMs;
  uint64_t lastLapElapsedMs_ = 0;
  uint64_t previousLapDurationMs_ = 0;
  uint64_t bestLapDurationMs_ = 0;
  uint32_t lapCount_ = 0;
  uint64_t nextMilestoneMs_ = 60ULL * 1000ULL;
  uint8_t countdownMilestoneMask_ = 0;
  uint64_t challengeTargetMs_ = kDefaultChallengeTargetMs;
  TimekeeperChallengeDifficulty challengeDifficulty_ =
    TimekeeperChallengeDifficulty::Medium;
  uint64_t pomodoroWorkDurationMs_ = kDefaultPomodoroWorkDurationMs;
  uint64_t pomodoroBreakDurationMs_ = kDefaultPomodoroBreakDurationMs;
  uint32_t pomodoroConfigRevision_ = 1;
  uint8_t pomodoroCycles_ = kDefaultPomodoroCycles;
  uint64_t pomodoroActiveWorkDurationMs_ = kDefaultPomodoroWorkDurationMs;
  uint64_t pomodoroActiveBreakDurationMs_ = kDefaultPomodoroBreakDurationMs;
  uint32_t pomodoroActiveConfigRevision_ = 1;
  uint8_t pomodoroActiveCycles_ = kDefaultPomodoroCycles;
  uint8_t pomodoroCycleIndex_ = 0;
  TimekeeperPomodoroPhase pomodoroPhase_ = TimekeeperPomodoroPhase::None;
  uint8_t pomodoroMilestoneMask_ = 0;
};
