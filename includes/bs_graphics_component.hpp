#ifndef __BS_GRAPHICS_COMPONENT_H_
#define __BS_GRAPHICS_COMPONENT_H_

#include <stdint.h>

#include "bs_entity.hpp"
#include "bs_types.hpp"
#include "glm/mat4x4.hpp"
#include "mesh.hpp"

namespace bs {
class GraphicsComponent {
 public:
  void update(float deltaTime, MusicPos musicPos);

 public:
  glm::mat4 _transform;
  Entity *_entity;
  Model *_model;
};
}  // namespace bs

#endif  // __BS_GRAPHICS_COMPONENT_H_
