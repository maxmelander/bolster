#include "end_state.hpp"

#include <iostream>

#include "game_state.hpp"
#include "game_state_manager.hpp"

EndState::EndState(GameStateManager &gameStateManager)
    : GameState{gameStateManager} {};

void EndState::onEnter() { std::cout << "The end." << std::endl; };

void EndState::onExit(){};

void EndState::update(float dt, const MusicPos &mp,
                      const GamepadState &gamepadState,
                      FrameEvents &frameEvents) {
  for (size_t i{}; i < gamepadState.size(); i++) {
    if (gamepadState[i]) {
      _gameStateManager.restartGame();
    }
  }
};

void EndState::rUpdate(const MusicPos &mp, const GamepadState &gamepadState,
                       FrameEvents &frameEvents){};
