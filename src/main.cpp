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
#include "movement_component.hpp"
#include "rhythmic_state.hpp"

static float lastMouseX = 400, lastMouseY = 300;
static Camera camera{glm::vec3{0.0f, 0.0f, 3.5f}};

Bolster::Bolster()
    : _windowTitle{"Bolster"},
      _windowWidth{1200},
      _windowHeight{900},
      _allocator{1000000 * 100},  // 100mb
      _deltaTime{0.0f},
      _lastFrameTime{0.0f},
      _gameStateManager{_allocator},
      _entityManager{_allocator} {
  initGlfw();

  _renderer.init(_window, _allocator);
  //

  initScene();

  // TODO: Some kind of resource manager and stuff
  _renderer.setupDrawables(_entityManager._graphicsComponents,
                           _entityManager._nGraphicsComponents);
}

Bolster::~Bolster() {
  glfwDestroyWindow(_window);
  glfwTerminate();
}

void Bolster::initScene() {
  auto sf = _entityManager.createEntity();
  _entityManager.addComponent(
      sf->_handle, bs::GraphicsComponent{._transform = glm::mat4{},
                                         ._model = &_renderer._drawable});
  // _entityManager.addComponent(sfHandle, MovementComponent{});
  // SF

  auto enemy1 = _entityManager.createEntity();
  enemy1->_pos = glm::vec3{-5.0f, 0.0f, -20.0f};

  _entityManager.addComponent(
      enemy1->_handle, bs::GraphicsComponent{._transform = glm::mat4{},
                                             ._model = &_renderer._drawable});

  _entityManager.addComponent(
      enemy1->_handle,
      TargetingComponent{enemy1, sf, 3.f, 5.f, glm::vec3{-4.0f, 3.0f, 0.0f}});

  auto enemy2 = _entityManager.createEntity();
  enemy2->_pos = glm::vec3{5.0f, 0.0f, -20.0f};

  _entityManager.addComponent(
      enemy2->_handle, bs::GraphicsComponent{._transform = glm::mat4{},
                                             ._model = &_renderer._drawable});
  _entityManager.addComponent(
      enemy2->_handle,
      TargetingComponent{enemy2, sf, 3.f, 6.f, glm::vec3{4.0f, 3.0f, 0.0f}});
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

  // camera.yaw += xOffset;
  // camera.pitch += yOffset;
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

  while (!glfwWindowShouldClose(_window)) {
    glfwPollEvents();

    auto currentTime = glfwGetTime();
    _deltaTime = currentTime - _lastFrameTime;
    _lastFrameTime = currentTime;

    // NOTE: There is a problem where we use the audio engine's music pos
    // to generate events in the game state's. But sometimes, those events
    // should trigger audio things to happen.
    //
    // So, we need to separate thos two things, and first get the music pos,
    // then after the game state update, we do a processEvents type call to
    // the audio engine
    MusicPos musicPos = _audioEngine.update(_deltaTime);

    GamepadState gamepadState = processInput(_window);

    FrameEvents frameEvents{
        .events = _allocator.alloc<FrameEvent, StackDirection::Top>(
            sizeof(FrameEvent) * MAX_FRAME_EVENTS)};

    // Game logic update
    _gameStateManager.update(_deltaTime, musicPos, gamepadState, frameEvents);

    // Rhythmic game logic update
    if (lastMusicPos != musicPos) {
      _gameStateManager.rUpdate(musicPos, gamepadState, frameEvents);
      lastMusicPos = musicPos;
    }

    _entityManager.update(_deltaTime, musicPos, frameEvents);

    _audioEngine.processEvents(frameEvents);

    // // TODO: Move to SF AI component or something like that
    // for (size_t i{}; i < _nMovementComponents; i++) {
    //   for (size_t f{}; f < frameEvents.nEvents; f++) {
    //     switch (frameEvents.events[f].type) {
    //       case EventType::RHYTHM_UP:
    //         _movementComponents[i].moveTo(
    //             glm::vec3{_entities[i]._pos.x, _entities[i]._pos.y + 0.5,
    //                       _entities[i]._pos.z},
    //             15.f, nullptr);
    //         break;
    //       case EventType::RHYTHM_DOWN:
    //         _movementComponents[i].moveTo(
    //             glm::vec3{_entities[i]._pos.x, _entities[i]._pos.y - 0.5,
    //                       _entities[i]._pos.z},
    //             15.f, [&, i]() {
    //               _movementComponents[i].moveTo(
    //                   glm::vec3{_entities[i]._pos.x, _entities[i]._pos.y +
    //                   0.5,
    //                             _entities[i]._pos.z},
    //                   15.f, nullptr);
    //             });
    //         break;
    //       case EventType::RHYTHM_LEFT:
    //         _movementComponents[i].moveTo(
    //             glm::vec3{_entities[i]._pos.x - 0.5, _entities[i]._pos.y,
    //                       _entities[i]._pos.z},
    //             15.f, nullptr);
    //         break;
    //       case EventType::RHYTHM_RIGHT:
    //         _movementComponents[i].moveTo(
    //             glm::vec3{_entities[i]._pos.x + 0.5, _entities[i]._pos.y,
    //                       _entities[i]._pos.z},
    //             15.f, [&, i]() {
    //               _movementComponents[i].moveTo(
    //                   glm::vec3{_entities[i]._pos.x - 0.5,
    //                   _entities[i]._pos.y,
    //                             _entities[i]._pos.z},
    //                   15.f, nullptr);
    //             });
    //         break;
    //       default:
    //         break;
    //     }
    //   }
    // }

    // Batched component updates

    // Game updates
    // dialogueComponent.update(_deltaTime, {0, 0, 0}, _buttonsPressed);

    camera.update(_deltaTime);

    // Render
    _renderer.draw(_entityManager._graphicsComponents,
                   _entityManager._nGraphicsComponents, camera, currentTime,
                   _deltaTime);

    // Delete stuff that needs to be deleted
    for (size_t i{}; i < frameEvents.nEvents; i++) {
      auto& event = frameEvents.events[i];
      if (event.type == EventType::DESTROY) {
        // std::cout << "DESTRYOUUIUIUO" << std::endl;
        _entityManager.deleteEntity(event.entityHandle);
        _renderer.setupDrawables(_entityManager._graphicsComponents,
                                 _entityManager._nGraphicsComponents);
      }
    }

    _allocator.clearTop();
  }
}

int main() {
  Bolster bolster{};
  bolster.run();

  return 0;
}
