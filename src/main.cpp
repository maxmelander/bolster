#include "main.hpp"

#include <stdint.h>

#include <fstream>
#include <iostream>
#include <string>

#include "GLFW/glfw3.h"
// #include "bs_dialogue_component.hpp"
#include "bs_entity.hpp"
#include "bs_graphics_component.hpp"
#include "bs_types.hpp"
#include "camera.hpp"
#include "dstack.hpp"
#include "glm/vec3.hpp"
#include "rhythmic_state.hpp"

static float lastMouseX = 400, lastMouseY = 300;
static Camera camera{glm::vec3{2.0f, 2.0f, 2.0f}};

Bolster::Bolster()
    : _windowTitle{"Bolster"},
      _windowWidth{1200},
      _windowHeight{900},
      _allocator{1000000 * 100},  // 100mb
      _deltaTime{0.0f},
      _lastFrameTime{0.0f},
      _gameStateManager{_allocator} {
  initGlfw();

  //_renderer.init(_window, _allocator);
  //

  initScene();

  // TODO: Some kind of resource manager and stuff
  // _renderer.setupDrawables(_graphicsComponents, _nGraphicsComponents);

  _audioEngine.load("../audio/b2.mp3", 84.5);
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

GamepadState Bolster::processInput(GLFWwindow* window) {
  GLFWgamepadstate gamepadState;
  glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState);

  GamepadState newState{};

  if (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A] ||
      glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    if (!_gamepadState[GAMEPAD_A]) {
      newState[GAMEPAD_A] = true;
      _gamepadState[GAMEPAD_A] = true;
    }
  } else {
    _gamepadState[GAMEPAD_A] = false;
  }

  if (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_B] ||
      glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    if (!_gamepadState[GAMEPAD_B]) {
      newState[GAMEPAD_B] = true;
      _gamepadState[GAMEPAD_B] = true;
    }
  } else {
    _gamepadState[GAMEPAD_B] = false;
  }

  if (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_X] ||
      glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    if (!_gamepadState[GAMEPAD_X]) {
      newState[GAMEPAD_X] = true;
      _gamepadState[GAMEPAD_X] = true;
    }
  } else {
    _gamepadState[GAMEPAD_X] = false;
  }

  if (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_Y] ||
      glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    if (!_gamepadState[GAMEPAD_Y]) {
      newState[GAMEPAD_Y] = true;
      _gamepadState[GAMEPAD_Y] = true;
    }
  } else {
    _gamepadState[GAMEPAD_Y] = false;
  }

  newState[GAMEPAD_DOWN] =
      (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] &&
       !_gamepadState[GAMEPAD_DOWN]);
  newState[GAMEPAD_LEFT] =
      (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] &&
       !_gamepadState[GAMEPAD_LEFT]);
  newState[GAMEPAD_RIGHT] =
      (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] &&
       !_gamepadState[GAMEPAD_RIGHT]);

  return newState;
}

void Bolster::run() {
  // bs::DialogueComponent dialogueComponent{};

  MusicPos lastMusicPos{999, 999, 999, 999};

  _audioEngine.playBackground();

  while (!glfwWindowShouldClose(_window)) {
    glfwPollEvents();

    auto currentTime = glfwGetTime();
    _deltaTime = currentTime - _lastFrameTime;
    _lastFrameTime = currentTime;

    MusicPos musicPos = _audioEngine.update(_deltaTime);

    GamepadState gamepadState = processInput(_window);

    FrameEvents frameEvents{
        .events = _allocator.alloc<EventType, StackDirection::Top>(
            sizeof(EventType) * MAX_FRAME_EVENTS)};

    // Game logic update
    _gameStateManager.update(_deltaTime, musicPos, gamepadState, frameEvents);

    // Rhythmic game logic update
    if (lastMusicPos != musicPos) {
      _gameStateManager.rUpdate(musicPos, gamepadState, frameEvents);
      lastMusicPos = musicPos;
    }

    // Batched component updates
    for (size_t i{}; i < _nGraphicsComponents; i++) {
      _graphicsComponents[i].update(_deltaTime, {0, 0, 0});
    }

    for (size_t i{}; i < frameEvents.nEvents; i++) {
      const auto& e = frameEvents.events[i];
      if (e == EventType::PLAYER_DEATH) {
        return;
      }
    }

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

    // Game updates
    // dialogueComponent.update(_deltaTime, {0, 0, 0}, _buttonsPressed);

    camera.update(_deltaTime);

    // Render
    // _renderer.draw(_graphicsComponents, _nGraphicsComponents, camera,
    //                currentFrame, _deltaTime);

    _allocator.clearTop();
  }
}

int main() {
  Bolster bolster{};
  bolster.run();

  return 0;
}
