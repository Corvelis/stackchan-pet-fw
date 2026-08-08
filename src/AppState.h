#pragma once

#include <stdint.h>

enum class ChanState {
  Idle,
  Listening,
  Speaking
};

// Top-level user experience.  This is intentionally separate from ChanState:
// a character can be Speaking while the Stopwatch experience remains active.
enum class ExperienceMode : uint8_t {
  Conversation = 0,
  Guruguru = 1,
  Timekeeper = 2,
  Travel = 3,
};

struct Pose {
  int pan;
  int tilt;
};
