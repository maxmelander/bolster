
#include "start_state.hpp"

#include <iostream>

#include "bs_types.hpp"
#include "game_state.hpp"
#include "game_state_manager.hpp"

StartState::StartState(GameStateManager &gameStateManager)
    : GameState{gameStateManager} {};

void StartState::onEnter() {
  std::cout << "Press any button to start" << std::endl;
};
void StartState::onExit(){};

// Continue to the next state if any button is pressed
void StartState::update(float dt, const MusicPos &mp,
                        const GamepadState &gamepadState,
                        FrameEvents &frameEvents) {
  for (size_t i{}; i < gamepadState.size(); i++) {
    if (gamepadState[i]) {
      frameEvents.addEvent(FrameEvent{.type = EventType::GAME_START});
      _gameStateManager.nextState();
    }
  }
};

void StartState::rUpdate(const MusicPos &mp, const GamepadState &gamepadState,
                         FrameEvents &frameEvents){};
