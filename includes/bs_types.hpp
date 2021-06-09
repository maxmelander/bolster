#ifndef __BS_TYPES_H_
#define __BS_TYPES_H_

#include <stdint.h>

#include <array>

constexpr size_t MAX_ENTITIES = 1000;
constexpr size_t MAX_FRAME_EVENTS = 10;

struct RhythmEvent {
  uint32_t beat;
  size_t gamepadButton;
};

struct RhythmBar {
  size_t nEvents;
  RhythmEvent *rhythmEvents;
};

struct MusicPos {
  uint32_t period;
  uint32_t barRel;
  uint32_t beatRel;
  uint32_t beat;

  bool operator==(const MusicPos &other) {
    return period == other.period && barRel == other.barRel &&
           beatRel == other.beatRel && beat == other.beat;
  }

  bool operator!=(const MusicPos &other) {
    return period != other.period || barRel != other.barRel ||
           beatRel != other.beatRel || beat != other.beat;
  }
};

enum class EventType {
  PLAYER_BAD,
  PLAYER_OK,
  PLAYER_PERFECT,
  PLAYER_FAIL,
  PLAYER_DEATH,
};

struct FrameEvents {
  uint32_t nEvents = 0;
  EventType *events;

  void addEvent(EventType eventType) {
    if (nEvents < MAX_FRAME_EVENTS) {
      events[nEvents] = eventType;
      nEvents++;
    }
  }
};

constexpr size_t GAMEPAD_A = 0;
constexpr size_t GAMEPAD_B = 1;
constexpr size_t GAMEPAD_X = 2;
constexpr size_t GAMEPAD_Y = 3;
constexpr size_t GAMEPAD_UP = 4;
constexpr size_t GAMEPAD_DOWN = 5;
constexpr size_t GAMEPAD_LEFT = 6;
constexpr size_t GAMEPAD_RIGHT = 7;
constexpr size_t GAMEPAD_NONE = 99;

typedef std::array<bool, 8> GamepadState;

#endif  // __BS_TYPES_H_
