#include "main.hpp"

#include <stdint.h>

#include <iostream>
#include <string>

#include "GLFW/glfw3.h"
#include "bs_dialogue_component.hpp"
#include "bs_entity.hpp"
#include "bs_graphics_component.hpp"
#include "bs_types.hpp"
#include "camera.hpp"
#include "dstack.hpp"
#include "glm/vec3.hpp"

static float lastMouseX = 400, lastMouseY = 300;
static Camera camera{glm::vec3{2.0f, 2.0f, 2.0f}};

Bolster::Bolster()
    : _windowTitle{"Bolster"},
      _windowWidth{1200},
      _windowHeight{900},
      _lastButtonsPressed{false, false, false, false},
      _buttonsPressed{false, false, false, false},
      _deltaTime{0.0f},
      _lastFrameTime{0.0f} {
  initGlfw();

  _renderer.init(_window);

  initScene();

  // TODO: Some kind of resource manager and stuf
  _renderer.setupDrawables(_graphicsComponents.data(),
                           _graphicsComponents.size());

  _audioEngine.load("../audio/b2.mp3", 84.5);
}

Bolster::~Bolster() {
  glfwDestroyWindow(_window);
  glfwTerminate();
}

void Bolster::initScene() {
  for (size_t i{}; i < bs::MAX_ENTITIES; i++) {
    size_t y = i % 10;
    size_t x = i / 10;
    bs::Entity entity{{x * 2.2f, 0.0f, y * 2.2f}};
    _entities[i] = entity;

    bs::GraphicsComponent gComp{glm::mat4{}, static_cast<uint32_t>(i) % 2,
                                &_entities[i], &_renderer._meshes[i % 2]};

    _graphicsComponents[i] = gComp;
  }
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

void Bolster::processMouse(GLFWwindow* window, double xpos, double ypos) {
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

// NOTE: We might only want a button press to count for one update,
// then it shouldn't count anymore?
void Bolster::processInput(GLFWwindow* window) {
  GLFWgamepadstate gamepadState;
  if (glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState)) {
    if (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A]) {
      if (!_lastButtonsPressed[bs::GAMEPAD_A]) {
        _buttonsPressed[bs::GAMEPAD_A] = true;
      } else {
        _buttonsPressed[bs::GAMEPAD_A] = false;
      }
      _lastButtonsPressed[bs::GAMEPAD_A] = true;
    } else {
      _lastButtonsPressed[bs::GAMEPAD_A] = false;
    }

    if (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_B]) {
      if (!_lastButtonsPressed[bs::GAMEPAD_B]) {
        _buttonsPressed[bs::GAMEPAD_B] = true;
      } else {
        _buttonsPressed[bs::GAMEPAD_B] = false;
      }
      _lastButtonsPressed[bs::GAMEPAD_B] = true;
    } else {
      _lastButtonsPressed[bs::GAMEPAD_B] = false;
    }

    if (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_X]) {
      if (!_lastButtonsPressed[bs::GAMEPAD_X]) {
        _buttonsPressed[bs::GAMEPAD_X] = true;
      } else {
        _buttonsPressed[bs::GAMEPAD_X] = false;
      }
      _lastButtonsPressed[bs::GAMEPAD_X] = true;
    } else {
      _lastButtonsPressed[bs::GAMEPAD_X] = false;
    }

    if (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_Y]) {
      if (!_lastButtonsPressed[bs::GAMEPAD_Y]) {
        _buttonsPressed[bs::GAMEPAD_Y] = true;
      } else {
        _buttonsPressed[bs::GAMEPAD_Y] = false;
      }
      _lastButtonsPressed[bs::GAMEPAD_Y] = true;
    } else {
      _lastButtonsPressed[bs::GAMEPAD_Y] = false;
    }
  }
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
  bool firstDown = false;
  bs::DialogueComponent dialogueComponent{};

  bs::MusicPos lastMusicPos{999, 999, 999, 999};

  _audioEngine.playBackground();

  while (!glfwWindowShouldClose(_window)) {
    auto currentFrame = glfwGetTime();
    _deltaTime = currentFrame - _lastFrameTime;
    _lastFrameTime = currentFrame;

    auto musicPos = _audioEngine.update(_deltaTime);
    if (lastMusicPos != musicPos) {
      lastMusicPos = musicPos;
      std::cout << musicPos.period << ", " << musicPos.barRel << ", "
                << musicPos.beatRel << ", " << musicPos.beat << std::endl;

      _graphicsComponents[0]._entity->_pos.x += 0.3 * std::sin(musicPos.beat);
      _graphicsComponents[0]._entity->_pos.z -= 0.3 * std::cos(musicPos.beat);
    }

    // Input
    glfwPollEvents();
    processInput(_window);

    // Game updates
    dialogueComponent.update(_deltaTime, {0, 0, 0}, _buttonsPressed);
    size_t i{};
    for (bs::GraphicsComponent& gc : _graphicsComponents) {
      gc.update(_deltaTime, {0, 0, 0});
      i++;
    }
    camera.update(_deltaTime);

    // Render
    _renderer.draw(_graphicsComponents.data(), _graphicsComponents.size(),
                   camera, currentFrame, _deltaTime);
  }
}

int main() {
  Bolster bolster{};
  bolster.run();

  return 0;
}
