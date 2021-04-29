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
      _lastFrameTime{0.0f},
      _allocator{1000000 * 10}  // 10mb
{
  initGlfw();

  _renderer.init(_window, _allocator);

  initScene();

  // TODO: Some kind of resource manager and stuf
  _renderer.setupDrawables(_graphicsComponents, _nGraphicsComponents);

  // _audioEngine.load("../audio/b2.mp3", 84.5);
}

Bolster::~Bolster() {
  glfwDestroyWindow(_window);
  glfwTerminate();
}

void Bolster::initScene() {
  _nEntities = 1;
  _nGraphicsComponents = 1;

  _entities = _allocator.alloc<bs::Entity, StackDirection::Bottom>(
      sizeof(bs::Entity) * _nEntities);
  _graphicsComponents =
      _allocator.alloc<bs::GraphicsComponent, StackDirection::Bottom>(
          sizeof(bs::GraphicsComponent) * _nGraphicsComponents);

  // Adam Head
  _entities[0] = bs::Entity{};
  bs::GraphicsComponent gComp{glm::mat4{}, &_entities[0], &_renderer._drawable};
  _graphicsComponents[0] = gComp;

  // Bunny
  // _entities[1] = bs::Entity{{0.0f, 1.0f, 0.0f}};
  // bs::GraphicsComponent bComp{glm::mat4{}, 1, &_entities[1],
  //&_renderer._meshes[0]};
  //_graphicsComponents[1] = bComp;

  // // Cube
  // _entities[2] = bs::Entity{};
  // bs::GraphicsComponent cComp{glm::mat4{}, 1, &_entities[2],
  //                             &_renderer._meshes[1]};
  // _entities[2]._pos = glm::vec3{2.f, 1.4f, 0.4f};

  // _graphicsComponents[2] = cComp;

  // for (size_t i = 3; i < _nEntities; i++) {
  //   bs::Entity entity{{5.9f * std::sin(i), 4.0f, 5.9f * std::cos(i)}};
  //   _entities[i] = entity;

  //   bs::GraphicsComponent gComp{glm::mat4{}, 0, &_entities[i],
  //                               &_renderer._meshes[0]};

  //   _graphicsComponents[i] = gComp;
  // }
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

  // _audioEngine.playBackground();

  while (!glfwWindowShouldClose(_window)) {
    auto currentFrame = glfwGetTime();
    _deltaTime = currentFrame - _lastFrameTime;
    _lastFrameTime = currentFrame;

    // auto musicPos = _audioEngine.update(_deltaTime);
    // if (lastMusicPos != musicPos) {
    // lastMusicPos = musicPos;
    // std::cout << musicPos.period << ", " << musicPos.barRel << ", "
    // << musicPos.beatRel << ", " << musicPos.beat << std::endl;

    // _graphicsComponents[0]._entity->_pos.x += 0.3 *
    // std::sin(musicPos.beat); _graphicsComponents[0]._entity->_pos.z -= 0.3
    // * std::cos(musicPos.beat);
    //}

    // Test light pos
    // _graphicsComponents[1]._entity->_pos.x =
    //     (std::sin(currentFrame * 0.2) + 1) * 5.0f;
    // _graphicsComponents[1]._entity->_pos.y = 4.f;
    // _graphicsComponents[1]._entity->_pos.z =
    //     (std::cos(currentFrame * 0.2) + 1) * 5.0f;

    //_graphicsComponents[3]._entity->_pos.x =
    //(std::cos(currentFrame * 1.2) + 1) * 15.0f;
    //_graphicsComponents[3]._entity->_pos.y = 3.f;
    //_graphicsComponents[3]._entity->_pos.z =
    //(std::sin(currentFrame * 1.2) + 1) * 15.0f;
    // Input
    glfwPollEvents();
    processInput(_window);

    // Game updates
    dialogueComponent.update(_deltaTime, {0, 0, 0}, _buttonsPressed);

    for (size_t i{}; i < _nGraphicsComponents; i++) {
      _graphicsComponents[i].update(_deltaTime, {0, 0, 0});
    }
    camera.update(_deltaTime);

    // Render
    _renderer.draw(_graphicsComponents, _nGraphicsComponents, camera,
                   currentFrame, _deltaTime);

    _allocator.clearTop();
  }
}

int main() {
  Bolster bolster{};
  bolster.run();

  return 0;
}
