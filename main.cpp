#include "main.hpp"

#include <stdint.h>

#include "GLFW/glfw3.h"
#include "camera.hpp"
#include "dstack.hpp"

Bolster::Bolster() {}
Bolster::~Bolster() {
  glfwDestroyWindow(_window);
  glfwTerminate();
}
void Bolster::init() {
  _windowTitle = "Bolster";
  _windowWidth = 800;
  _windowHeight = 600;
  _camera = Camera{glm::vec3{2.0f, 2.0f, 2.0f}};
  _deltaTime = 0.0f;
  _lastFrameTime = 0.0f;
  _lastMouseX = 400;
  _lastMouseY = 300;

  initGlfw();

  _renderer.init(_window);
}

void Bolster::initGlfw() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  _window = glfwCreateWindow(_windowWidth, _windowHeight, _windowTitle, nullptr,
                             nullptr);
  glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  // glfwSetCursorPosCallback(_window, mouseCallback);
  glfwSetWindowUserPointer(_window, this);
  // glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
}

void Bolster::processMouse(GLFWwindow *window, double xpos, double ypos) {
  float xOffset = xpos - _lastMouseX;
  float yOffset = _lastMouseY - ypos;

  _lastMouseX = xpos;
  _lastMouseY = ypos;

  const float sensitivity = 0.1f;
  xOffset *= sensitivity;
  yOffset *= sensitivity;

  _camera.yaw += xOffset;
  _camera.pitch += yOffset;
}

void Bolster::processKeyboard(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    _camera.setAcceleration(0.5f);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    _camera.setAcceleration(-0.5f);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    _camera.setStrafeAcceleration(-0.5f);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    _camera.setStrafeAcceleration(0.5f);
}

void Bolster::run() {
  while (!glfwWindowShouldClose(_window)) {
    auto currentFrame = glfwGetTime();
    _deltaTime = currentFrame - _lastFrameTime;
    _lastFrameTime = currentFrame;

    glfwPollEvents();
    processKeyboard(_window);
    _camera.update(_deltaTime);

    _renderer.draw(_camera, _deltaTime);
  }
}

int main() {
  Bolster bolster{};
  bolster.init();
  bolster.run();

  return 0;
}
