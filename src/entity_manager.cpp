#include "entity_manager.hpp"

#include <iostream>

#include "bs_entity.hpp"
#include "bs_types.hpp"

EntityManager::EntityManager(DStack &allocator)
    : _allocator{allocator},
      _nGraphicsComponents{0},
      _nMovementComponents{0},
      _nTargetingComponents{0} {
  for (uint8_t i{}; i < MAX_ENTITIES; i++) _freeHandles.push_back(i);

  // Allocate little memory pools for each type of component
  _entities = _allocator.alloc<bs::Entity, StackDirection::Bottom>(
      sizeof(bs::Entity) * MAX_ENTITIES);
  _graphicsComponents =
      _allocator.alloc<bs::GraphicsComponent, StackDirection::Bottom>(
          sizeof(bs::GraphicsComponent) * MAX_ENTITIES);
  _movementComponents =
      _allocator.alloc<MovementComponent, StackDirection::Bottom>(
          sizeof(MovementComponent) * MAX_ENTITIES);
  _targetingComponents =
      _allocator.alloc<TargetingComponent, StackDirection::Bottom>(
          sizeof(TargetingComponent) * MAX_ENTITIES);
}

// TODO: Smart pointers, probably
bs::Entity *EntityManager::createEntity() noexcept {
  auto handle = _freeHandles.front();
  _freeHandles.pop_front();

  _entities[handle] = bs::Entity{._handle = handle};

  return &_entities[handle];
}

void EntityManager::deleteEntity(uint8_t handle) noexcept {
  // Deleta all the entity components
  // and repack the arrays
  auto &entity = _entities[handle];

  if (entity._graphicsComponent.has_value()) {
    auto componentIndex = entity._graphicsComponent.value();
    // TODO: Call the destructor
    // _graphicsComponents[indices.graphicsComponent] = {};

    // Plug the hole with the last component
    _graphicsComponents[componentIndex] =
        _graphicsComponents[_nGraphicsComponents - 1];
    // Update the moved entity's component index
    _graphicsComponents[componentIndex]._entity->_graphicsComponent =
        componentIndex;

    _nGraphicsComponents--;
  }

  if (entity._movementComponent.has_value()) {
    auto componentIndex = entity._movementComponent.value();
    // TODO: Call the destructor
    _movementComponents[componentIndex] =
        _movementComponents[_nMovementComponents - 1];
    // Update the moved entity's component index
    _movementComponents[componentIndex]._entity->_movementComponent =
        componentIndex;
    _nMovementComponents--;
  }

  if (entity._targetingComponent.has_value()) {
    auto componentIndex = entity._targetingComponent.value();
    // TODO: Call the destructor
    _targetingComponents[componentIndex] =
        _targetingComponents[_nTargetingComponents - 1];
    // Update the moved entity's component index
    _targetingComponents[componentIndex]._entity->_targetingComponent =
        componentIndex;
    _nTargetingComponents--;
  }

  // NOTE: The entity itself doesn't need to be reset here.
  // Since we deleted all of its components it won't do anything anyway
  //
  // Later we might need some way of signaling that an entity is deleted
  // if some part of the code looks up the wrong entity or something idk.
  _freeHandles.push_back(handle);
}

bs::Entity *EntityManager::getEntityPtr(uint8_t handle) {
  return &_entities[handle];
}

// TODO: Consistent way of doing entity hookup
void EntityManager::addComponent(uint8_t handle,
                                 bs::GraphicsComponent component) noexcept {
  uint8_t componentIndex = _nGraphicsComponents;
  component._entity = &_entities[handle];
  _graphicsComponents[componentIndex] = component;
  _entities[handle]._graphicsComponent = componentIndex;
  _nGraphicsComponents++;
}
void EntityManager::addComponent(uint8_t handle,
                                 MovementComponent component) noexcept {
  uint8_t componentIndex = _nMovementComponents;
  component._entity = &_entities[handle];
  _movementComponents[componentIndex] = component;
  _entities[handle]._movementComponent = componentIndex;
  _nMovementComponents++;
}
void EntityManager::addComponent(uint8_t handle,
                                 TargetingComponent component) noexcept {
  uint8_t componentIndex = _nTargetingComponents;
  // component._entity = &_entities[handle];
  _targetingComponents[componentIndex] = component;
  _entities[handle]._targetingComponent = componentIndex;
  _nTargetingComponents++;
}

void EntityManager::update(float deltaTime, MusicPos mp,
                           FrameEvents &frameEvents) {
  // Batched component update
  //
  // NOTE: In the future, I might want to create component managers which are
  // structs of arrays. If I want to batch update a single field at a time, for
  // caching reasons.
  //
  // But I'll look at that later, when I know my access patterns and stuff like
  // that Maybe I don't need the complexity at all, since my game is so simple
  for (size_t i{}; i < _nGraphicsComponents; i++) {
    _graphicsComponents[i].update(deltaTime, {0, 0, 0});
  }

  for (size_t i{}; i < _nMovementComponents; i++) {
    _movementComponents[i].update(deltaTime);
  }

  for (size_t i{}; i < _nTargetingComponents; i++) {
    _targetingComponents[i].update(deltaTime, mp, frameEvents);
  }
}
