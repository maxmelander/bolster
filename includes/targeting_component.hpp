#ifndef __TARGETING_COMPONENT_H_
#define __TARGETING_COMPONENT_H_

#include "bs_entity.hpp"
#include "bs_types.hpp"
#include "glm/ext/vector_float3.hpp"
class TargetingComponent {
 public:
  TargetingComponent(bs::Entity *entity, bs::Entity *target, float targetTime,
                     float delay, glm::vec3 archNormal);
  void update(float deltaTime, MusicPos mp, FrameEvents &frameEvents);

 public:
  bs::Entity *_entity;
  bs::Entity *_target;

 private:
  float _currentTime;
  float _targetTime;
  glm::vec3 _archNormal;
  glm::vec3 _startingPos;
};

#endif  // __TARGETING_COMPONENT_H_
