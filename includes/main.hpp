#ifndef __MAIN_H_
#define __MAIN_H_

#include <stdint.h>

#include "audio.hpp"
#include "bs_entity.hpp"
#include "dstack.hpp"
#include "game_state_manager.hpp"
#include "movement_component.hpp"
#include "soloud.h"
#include "soloud_wavstream.h"
#include "targeting_component.hpp"
#include "vk_engine.hpp"

class GLFWwindow;
class GLFWgamepadstate;

class Bolster {
 public:
  Bolster();
  ~Bolster();
  void run();

 private:
  void initGlfw();
  void initScene();
  GamepadState processInput(GLFWwindow *);
  static void processMouse(GLFWwindow *, double, double);

 public:
  GLFWwindow *_window;

 private:
  const char *_windowTitle;
  uint32_t _windowWidth, _windowHeight;

  DStack _allocator;

  float _deltaTime;
  float _lastFrameTime;

  GamepadState _gamepadState;

  size_t _nEntities;
  bs::Entity *_entities;

  size_t _nGraphicsComponents;
  bs::GraphicsComponent *_graphicsComponents;

  size_t _nMovementComponents;
  MovementComponent *_movementComponents;

  size_t _nTargetingComponents;
  TargetingComponent *_targetingComponents;

  GameStateManager _gameStateManager;

  AudioEngine _audioEngine;
  VulkanEngine _renderer;
};

#endif  // __MAIN_H_
