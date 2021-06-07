#include "rhythmic_state.hpp"

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

#include "bs_types.hpp"
#include "game_state_manager.hpp"
#include "json.hpp"

RhythmicState::RhythmicState(uint32_t level, GameStateManager &gameStateManager)
    : GameState{gameStateManager},
      _talking{true},
      _playerHealth{3},
      _rhythmEventIndex{0} {
  loadData(level);
}

void RhythmicState::loadData(uint32_t level) {
  using json = nlohmann::json;
  std::ifstream i("../data/level1.json");
  json j;
  i >> j;

  auto index = 0;
  for (const auto &e : j["events"]) {
    RhythmEvent re{.beat = e["beat"].get<uint32_t>(),
                   .gamepadButton = e["gamepadButton"].get<size_t>()};

    _rhythmEvents[index] = re;
    index++;
  }
}

void RhythmicState::onEnter() {
  // NOTE: This is where we would show some kind of start game message
}

void RhythmicState::onExit() {}

void RhythmicState::onObscure() {}

void RhythmicState::onReveal() {
  // NOTE: If this state gets revealed, we know that the player lost,
  // so we restart the game.
}

void RhythmicState::processInput(const GamepadState &gamepadState,
                                 const MusicPos &mp, FrameEvents &frameEvents) {
  if (_rhythmEventIndex >= _rhythmEvents.size()) {
    return;
  }

  auto rhythmEvent = _rhythmEvents[_rhythmEventIndex];

  int32_t distance = mp.beatRel - rhythmEvent.beat;

  // If the player was too late to hit the target
  if (distance > BEAT_WINDOW) {
    _playerHealth--;
    _rhythmEventIndex++;

    if (_playerHealth <= 0) {
      std::cout << "DEAD" << std::endl;
      frameEvents.addEvent(EventType::PLAYER_DEATH);
      return;
    }

    frameEvents.addEvent(EventType::PLAYER_FAIL);
    return;
  }

  // If pressed any incorrect button
  for (size_t i{}; i < gamepadState.size(); i++) {
    if (i != rhythmEvent.gamepadButton && gamepadState[i]) {
      _playerHealth--;
      _rhythmEventIndex++;

      if (_playerHealth <= 0) {
        frameEvents.addEvent(EventType::PLAYER_DEATH);
        return;
      }

      frameEvents.addEvent(EventType::PLAYER_FAIL);
      return;
    }
  }

  // If pressed the correct button
  if (rhythmEvent.gamepadButton != GAMEPAD_NONE &&
      gamepadState[rhythmEvent.gamepadButton]) {
    frameEvents.addEvent(EventType::PLAYER_SUCCESS);
    _rhythmEventIndex++;
    return;
  }
}

void RhythmicState::update(float dt, const MusicPos &mp,
                           const GamepadState &gamepadState,
                           FrameEvents &frameEvents) {
  if (!_talking) {
    processInput(gamepadState, mp, frameEvents);
  }
}

void RhythmicState::rUpdate(const MusicPos &mp,
                            const GamepadState &gamepadState,
                            FrameEvents &frameEvents) {
  // For every new bar, switch between talking an listening
  if (mp.beatRel == 0) {
    _rhythmEventIndex = 0;
    _talking = !_talking;
  }

  if (_talking) {
    if (_rhythmEventIndex < _rhythmEvents.size()) {
      auto rhythmEvent = _rhythmEvents[_rhythmEventIndex];
      if (rhythmEvent.beat % 16 == mp.beatRel) {
        _rhythmEventIndex++;
      }
    }
  }
}
