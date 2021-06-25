#ifndef __BS_ENTITY_H_
#define __BS_ENTITY_H_

#include <array>
#include <optional>

#include "bs_types.hpp"
#include "glm/vec3.hpp"

namespace bs {

struct Entity {
  uint8_t _handle;
  std::optional<uint8_t> _graphicsComponent{std::nullopt};
  std::optional<uint8_t> _movementComponent{std::nullopt};
  std::optional<uint8_t> _targetingComponent{std::nullopt};
  glm::vec3 _pos;
};

}  // namespace bs

#endif  // __BS_ENTITY_H_
