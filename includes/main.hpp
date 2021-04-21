#ifndef __MAIN_H_
#define __MAIN_H_

#include <stdint.h>

#include "GLFW/glfw3.h"
#include "audio.hpp"
#include "bs_entity.hpp"
#include "dstack.hpp"
#include "soloud.h"
#include "soloud_wavstream.h"
#include "vk_engine.hpp"

class Bolster {
 public:
  Bolster();
  ~Bolster();
  void run();

 private:
  void initGlfw();
  void initScene();
  void processInput(GLFWwindow *);
  static void processMouse(GLFWwindow *, double, double);

 public:
  GLFWwindow *_window;

 private:
  const char *_windowTitle;
  uint32_t _windowWidth, _windowHeight;

  bool _lastButtonsPressed[4];
  bool _buttonsPressed[4];

  float _deltaTime;
  float _lastFrameTime;

  size_t _nEntities;
  bs::Entity *_entities;

  size_t _nGraphicsComponents;
  bs::GraphicsComponent *_graphicsComponents;

  DStack _allocator;
  AudioEngine _audioEngine;
  VulkanEngine _renderer;
};

#endif  // __MAIN_H_
