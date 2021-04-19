#ifndef __MAIN_H_
#define __MAIN_H_

#include <stdint.h>

#include <array>

#include "GLFW/glfw3.h"
#include "audio.hpp"
#include "bs_entity.hpp"
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

  std::array<bs::Entity, bs::MAX_ENTITIES> _entities;
  std::array<bs::GraphicsComponent, bs::MAX_ENTITIES> _graphicsComponents;

  AudioEngine _audioEngine;
  VulkanEngine _renderer;
};

#endif  // __MAIN_H_
