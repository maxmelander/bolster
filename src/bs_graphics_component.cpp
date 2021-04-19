#include "bs_graphics_component.hpp"

namespace bs {
// TODO: We only really need to update the transform matrix IF
// the entity has moved.
void GraphicsComponent::update(float deltaTime, MusicPos musicPos) {
  // NOTE: Not the best to follow a pointer here probably
  _transform = glm::translate(glm::mat4{1.0}, _entity->_pos);
}
}  // namespace bs
