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

  newState[GAMEPAD_A] = (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A] &&
                         !_lastGamepadState[GAMEPAD_A]);
  newState[GAMEPAD_B] = (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_B] &&
                         !_lastGamepadState[GAMEPAD_B]);
  newState[GAMEPAD_X] = (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_X] &&
                         !_lastGamepadState[GAMEPAD_X]);
  newState[GAMEPAD_Y] = (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_Y] &&
                         !_lastGamepadState[GAMEPAD_Y]);
  newState[GAMEPAD_UP] = (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] &&
                          !_lastGamepadState[GAMEPAD_UP]);
  newState[GAMEPAD_DOWN] =
      (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] &&
       !_lastGamepadState[GAMEPAD_DOWN]);
  newState[GAMEPAD_LEFT] =
      (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] &&
       !_lastGamepadState[GAMEPAD_LEFT]);
  newState[GAMEPAD_RIGHT] =
      (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] &&
       !_lastGamepadState[GAMEPAD_RIGHT]);

  _lastGamepadState = newState;
  return newState;
}

void Bolster::run() {
  bool firstDown = false;
  bs::DialogueComponent dialogueComponent{};

  MusicPos lastMusicPos{999, 999, 999, 999};

  // _audioEngine.playBackground();

  while (!glfwWindowShouldClose(_window)) {
    glfwPollEvents();

    auto currentTime = glfwGetTime();
    _deltaTime = currentTime - _lastFrameTime;
    _lastFrameTime = currentTime;

    // MusicPos musicPos = _audioEngine.update(_deltaTime);

    GamepadState gamepadState = processInput(_window);

    FrameEvents frameEvents{
        .nEvents = 0,
        .events = _allocator.alloc<EventType, StackDirection::Top>(
            sizeof(EventType) * MAX_FRAME_EVENTS)};

    _gameStateManager.update(_deltaTime, MusicPos{}, gamepadState, frameEvents);

    // Batched component updates
    for (size_t i{}; i < _nGraphicsComponents; i++) {
      _graphicsComponents[i].update(_deltaTime, {0, 0, 0});
    }

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
