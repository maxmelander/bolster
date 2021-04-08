#include "main.hpp"

#include <stdint.h>

#include "GLFW/glfw3.h"
#include "camera.hpp"
#include "dstack.hpp"

static float lastMouseX = 400, lastMouseY = 300;
static Camera camera{glm::vec3{2.0f, 2.0f, 2.0f}};

Bolster::Bolster()
    : _windowTitle{"Bolster"},
      _windowWidth{800},
      _windowHeight{600},
      _deltaTime{0.0f},
      _lastFrameTime{0.0f} {
  initGlfw();
  _renderer.init(_window);
}

Bolster::~Bolster() {
  glfwDestroyWindow(_window);
  glfwTerminate();
}

void Bolster::initGlfw() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  _window = glfwCreateWindow(_windowWidth, _windowHeight, _windowTitle, nullptr,
                             nullptr);
  glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(_window, processMouse);
  glfwSetWindowUserPointer(_window, this);
  // glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
}

void Bolster::processMouse(GLFWwindow *window, double xpos, double ypos) {
  float xOffset = xpos - lastMouseX;
  float yOffset = lastMouseY - ypos;

  lastMouseX = xpos;
  lastMouseY = ypos;

  const float sensitivity = 0.1f;
  xOffset *= sensitivity;
  yOffset *= sensitivity;

  camera.yaw += xOffset;
  camera.pitch += yOffset;
}

void Bolster::processKeyboard(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.setAcceleration(0.5f);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.setAcceleration(-0.5f);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.setStrafeAcceleration(-0.5f);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.setStrafeAcceleration(0.5f);
}

void Bolster::run() {
  while (!glfwWindowShouldClose(_window)) {
    auto currentFrame = glfwGetTime();
    _deltaTime = currentFrame - _lastFrameTime;
    _lastFrameTime = currentFrame;

    glfwPollEvents();
    processKeyboard(_window);
    camera.update(_deltaTime);

    _renderer.draw(camera, currentFrame, _deltaTime);
  }
}

int main() {
  Bolster bolster{};
  bolster.run();

  return 0;
}
