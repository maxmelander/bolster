#ifndef __MOVEMENT_COMPONENT_H_
#define __MOVEMENT_COMPONENT_H_

#include <functional>

#include "bs_entity.hpp"
#include "glm/vec3.hpp"

class MovementComponent {
 public:
  MovementComponent(bs::Entity *entity);
  void update(float deltaTime);
  void moveTo(glm::vec3 pos, float velocity, std::function<void()> &&callback);

 public:
  bs::Entity *_entity;
  std::function<void()> _callback;
  static constexpr float TOLERANCE = 0.1;

 private:
  bool _isMoving;
  float _velocity;
  glm::vec3 _direction;
  glm::vec3 _target;
};

#endif  // __MOVEMENT_COMPONENT_H_
