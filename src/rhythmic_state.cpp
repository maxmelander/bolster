#include "rhythmic_state.hpp"

#include <array>
#include <iostream>
#include <string>

#include "bs_types.hpp"
#include "game_state_manager.hpp"

RhythmicState::RhythmicState(const std::string &name,
                             GameStateManager &gameStateManager)
    : GameState{gameStateManager}, _name{name}, _playerHealth{3} {
  std::cout << "init " << _name << std::endl;
}

void RhythmicState::onEnter() {
  // NOTE: This is where we would show some kind of start game message
  std::cout << _name << "on enter" << std::endl;
}

void RhythmicState::onExit() { std::cout << _name << "on exit" << std::endl; }

void RhythmicState::onObscure() {
  std::cout << _name << "on obscure" << std::endl;
}

void RhythmicState::onReveal() {
  // NOTE: If this state gets revealed, we know that the player lost,
  // so we restart the game.
  std::cout << _name << "on reveal" << std::endl;
}

void RhythmicState::processInput(const GamepadState &gamepadState,
                                 const MusicPos &mp, FrameEvents frameEvents) {
  std::array<size_t, 8> challenge{GAMEPAD_A,    GAMEPAD_A,   GAMEPAD_NONE,
                                  GAMEPAD_NONE, GAMEPAD_X,   GAMEPAD_X,
                                  GAMEPAD_NONE, GAMEPAD_NONE};

  size_t target = challenge[mp.beatRel];

  // If pressed any incorrect button
  for (size_t i{}; i < gamepadState.size(); i++) {
    if (i != target && gamepadState[i]) {
      frameEvents.addEvent(EventType::PLAYER_FAIL);
      return;
    }
  }

  // If pressed the correct button
  if (target != GAMEPAD_NONE && gamepadState[target]) {
    frameEvents.addEvent(EventType::PLAYER_SUCCESS);
    return;
  }
}

void RhythmicState::update(float dt, const MusicPos &mp,
                           const GamepadState &gamepadState,
                           FrameEvents &frameEvents) {
  processInput(gamepadState, mp);

  // Based on what happened in the process input,
  // its time to change the state, I guess.
}
