#ifndef __MAIN_H_
#define __MAIN_H_

#include "GLFW/glfw3.h"
#include "vk_engine.hpp"

class Bolster {
 public:
  Bolster();
  ~Bolster();
  void run();

 private:
  void initGlfw();
  void processKeyboard(GLFWwindow *);
  static void processMouse(GLFWwindow *, double, double);

 public:
  GLFWwindow *_window;

 private:
  const char *_windowTitle;
  uint32_t _windowWidth, _windowHeight;

  float _deltaTime;
  float _lastFrameTime;

  VulkanEngine _renderer;
};

#endif  // __MAIN_H_
