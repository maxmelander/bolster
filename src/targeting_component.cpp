#include "targeting_component.hpp"

#include <cmath>
#include <iostream>
#include <ostream>

#include "bs_entity.hpp"
#include "bs_types.hpp"
#include "glm/ext/quaternion_geometric.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"

TargetingComponent::TargetingComponent(bs::Entity *entity, bs::Entity *target,
                                       float targetTime, float delay,
                                       glm::vec3 archNormal)
    : _entity{entity},
      _target{target},
      _currentTime{-delay},
      _targetTime{targetTime},
      _archNormal{archNormal},
      _startingPos{_entity->_pos} {};

void TargetingComponent::update(float deltaTime, MusicPos mp,
                                FrameEvents &frameEvents) {
  if (_currentTime >= 0.f) {
    float progress = _currentTime / _targetTime;
    glm::vec3 direction = _target->_pos - _startingPos;
    glm::vec3 newPos = _startingPos + (direction * progress);

    float heightScalar = std::sin(progress * (3.1415f));
    newPos += _archNormal * heightScalar;
    _entity->_pos = newPos;

    if (progress >= 1.0f) {
      // TODO: Entity index
      frameEvents.addEvent(
          FrameEvent{.type = EventType::DESTROY, .entityIndex = 2});
      std::cout << "ey" << std::endl;
    }
  }

  _currentTime += deltaTime;
}
