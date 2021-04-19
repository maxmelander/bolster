#ifndef __BS_TYPES_H_
#define __BS_TYPES_H_

#include <stdint.h>
namespace bs {

constexpr size_t MAX_ENTITIES = 100;
constexpr size_t MAX_COMPONENTS = 20;

constexpr size_t GAMEPAD_A = 0;
constexpr size_t GAMEPAD_B = 1;
constexpr size_t GAMEPAD_X = 2;
constexpr size_t GAMEPAD_Y = 3;
constexpr size_t GAMEPAD_NONE = 4;

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

}  // namespace bs

#endif  // __BS_TYPES_H_
