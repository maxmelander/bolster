#ifndef __ENTITY_MANAGER_H_
#define __ENTITY_MANAGER_H_

#include <stdint.h>

#include <deque>

#include "bs_entity.hpp"
#include "bs_graphics_component.hpp"
#include "bs_types.hpp"
#include "dstack.hpp"
#include "movement_component.hpp"
#include "targeting_component.hpp"

class EntityManager {
 public:
  EntityManager(DStack &allocator);
  // ~EntityManager();

  // Returns a handle to this entity
  bs::Entity *createEntity() noexcept;
  void deleteEntity(uint8_t handle) noexcept;

  bs::Entity *getEntityPtr(uint8_t handle);

  void addComponent(uint8_t handle, bs::GraphicsComponent component) noexcept;
  void addComponent(uint8_t handle, MovementComponent component) noexcept;
  void addComponent(uint8_t handle, TargetingComponent component) noexcept;

  void update(float delta, MusicPos mp, FrameEvents &frameEvents);

 private:
  DStack &_allocator;

  std::deque<uint8_t> _freeHandles;

  bs::Entity *_entities;

  size_t _nMovementComponents;
  MovementComponent *_movementComponents;

  size_t _nTargetingComponents;
  TargetingComponent *_targetingComponents;

  // TODO: Accessor type pattern for these badboys?
 public:
  size_t _nGraphicsComponents;
  bs::GraphicsComponent *_graphicsComponents;
};

#endif  // __ENTITY_MANAGER_H_
