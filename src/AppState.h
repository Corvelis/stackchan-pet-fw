#pragma once

enum class ChanState {
  Idle,
  Listening,
  Speaking
};

struct Pose {
  int pan;
  int tilt;
};
