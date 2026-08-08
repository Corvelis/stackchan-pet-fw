#include <assert.h>
#include <string.h>

#include "TimekeeperController.h"

namespace {

void testStopwatchAndLap() {
  TimekeeperController controller;
  controller.begin();
  TimekeeperEvent event = controller.toggle(1000);
  assert(event.type == TimekeeperEventType::Started);
  assert(controller.isRunning());

  event = controller.update(61000);
  assert(event.type == TimekeeperEventType::Milestone);
  assert(event.elapsedMs == 60000);

  event = controller.lap(62000);
  assert(event.type == TimekeeperEventType::Lap);
  assert(event.lapIndex == 1);
  assert(event.lapDurationMs == 61000);
  assert(event.isBestLap);

  event = controller.toggle(63000);
  assert(event.type == TimekeeperEventType::Paused);
  event = controller.reset(63000);
  assert(event.type == TimekeeperEventType::Reset);
  assert(controller.state() == TimekeeperState::Ready);
}

void testCountdown() {
  TimekeeperController controller;
  controller.begin();
  assert(controller.selectActivity(TimekeeperActivity::Countdown, 1000));
  assert(controller.setCountdownMinutes(1, 1000));
  assert(controller.toggle(1000).type == TimekeeperEventType::Started);

  TimekeeperEvent event = controller.update(61000);
  assert(event.type == TimekeeperEventType::Finished);
  assert(event.remainingMs == 0);
  assert(event.durationMs == 60000);
}

void testChallengeResultAndSuspend() {
  TimekeeperController controller;
  controller.begin();
  assert(controller.selectActivity(TimekeeperActivity::TenSecondChallenge, 1000));
  assert(controller.setChallengeTargetSeconds(10, 1000));
  assert(controller.toggle(1000).type == TimekeeperEventType::Started);

  TimekeeperEvent event = controller.toggle(11042);
  assert(event.type == TimekeeperEventType::Result);
  assert(event.signedErrorMs == 42);
  assert(event.absoluteErrorMs == 42);
  assert(strcmp(event.rank, "amazing") == 0);

  assert(controller.reset(11042).type == TimekeeperEventType::Reset);
  assert(controller.toggle(12000).type == TimekeeperEventType::Started);
  event = controller.suspend(13000, "display_off");
  assert(event.type == TimekeeperEventType::Aborted);
  assert(event.state == TimekeeperState::Aborted);
  assert(strcmp(event.reason, "display_off") == 0);
}

void testPomodoroTransitions() {
  TimekeeperController controller;
  controller.begin();
  assert(controller.configurePomodoro(60000, 60000, 2));
  assert(!controller.configurePomodoro(59999, 60000, 3));
  assert(controller.selectActivity(TimekeeperActivity::Pomodoro, 1000));
  assert(controller.setPomodoroCycles(2, 1000));
  assert(controller.toggle(1000).type == TimekeeperEventType::Started);

  TimekeeperEvent event = controller.update(61000);
  assert(event.type == TimekeeperEventType::Transition);
  assert(event.transition == TimekeeperTransition::WorkToBreak);
  assert(event.pomodoroPhase == TimekeeperPomodoroPhase::Break);
  assert(event.cycleIndex == 1);

  event = controller.update(121000);
  assert(event.type == TimekeeperEventType::Transition);
  assert(event.transition == TimekeeperTransition::BreakToWork);
  assert(event.pomodoroPhase == TimekeeperPomodoroPhase::Work);
  assert(event.cycleIndex == 2);

  event = controller.update(181000);
  assert(event.type == TimekeeperEventType::Completed);
  assert(event.state == TimekeeperState::Completed);
  assert(event.remainingMs == 0);
  assert(event.totalCycles == 2);
}

}  // namespace

int main() {
  testStopwatchAndLap();
  testCountdown();
  testChallengeResultAndSuspend();
  testPomodoroTransitions();
  return 0;
}
