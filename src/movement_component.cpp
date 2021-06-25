#include "movement_component.hpp"

#include "bs_entity.hpp"
#include "glm/geometric.hpp"

MovementComponent::MovementComponent() : _isMoving{false} {}

void MovementComponent::update(float deltaTime) {
  if (_isMoving) {
    _entity->_pos += _direction * _velocity * deltaTime;

    if (glm::distance(_entity->_pos, _target) < TOLERANCE) {
      _entity->_pos = _target;
      _isMoving = false;
      if (_callback) {
        _callback();
        _callback = nullptr;
      }
    }
  }
}

void MovementComponent::moveTo(glm::vec3 pos, float velocity,
                               std::function<void()> &&callback) {
  _target = pos;
  _direction = glm::normalize(pos - _entity->_pos);
  _velocity = velocity;
  _isMoving = true;
  _callback = callback;
}

/*
** First, i need to get the direction vector to the target.
** Next, for every tic, I need to move in that direction by the velocity
*/
