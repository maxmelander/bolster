#ifndef __MAIN_H_
#define __MAIN_H_

#include "GLFW/glfw3.h"
#include "camera.hpp"
#include "vk_engine.hpp"

class Bolster {
 public:
  Bolster();
  ~Bolster();
  void init();
  void run();

 private:
  void initGlfw();
  void processKeyboard(GLFWwindow *);
  void processMouse(GLFWwindow *, double, double);

 public:
  GLFWwindow *_window;
  Camera _camera;

 private:
  const char *_windowTitle;
  uint32_t _windowWidth, _windowHeight;

  float _deltaTime;
  float _lastFrameTime;
  float _lastMouseX, _lastMouseY;

  VulkanEngine _renderer;
};

#endif  // __MAIN_H_
