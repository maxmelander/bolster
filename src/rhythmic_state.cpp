#include "rhythmic_state.hpp"

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

#include "bs_types.hpp"
#include "dstack.hpp"
#include "game_state_manager.hpp"
#include "json.hpp"

RhythmicState::RhythmicState(uint32_t level, GameStateManager &gameStateManager,
                             DStack &allocator)
    : GameState{gameStateManager},
      _talking{false},
      _playerHealth{3},
      _rhythmBarIndex{-1},
      _rhythmEventIndex{0},
      _nRhythmBars{0} {
  loadData(level, allocator);
}

void RhythmicState::loadData(uint32_t level, DStack &allocator) {
  // Read json file
  using json = nlohmann::json;
  std::ifstream i("../data/level1.json");
  json j;
  i >> j;

  // Count how many rhythm bars and events we need to allocate
  _nRhythmBars = j["events"].size();

  // Allocate enough room for our rhythm bars
  _rhythmBars = allocator.alloc<RhythmBar, StackDirection::Bottom>(
      sizeof(RhythmBar) * _nRhythmBars);

  // ALlocate enough room for our rhythm events per bar
  for (size_t i{}; i < _nRhythmBars; i++) {
    _rhythmBars[i].rhythmEvents =
        allocator.alloc<RhythmEvent, StackDirection::Bottom>(
            sizeof(RhythmEvent) * j["events"][i].size());
    _rhythmBars[i].nEvents = j["events"][i].size();
  }

  // Set the correct values
  size_t barIndex{};
  for (const auto &e : j["events"]) {
    size_t eventIndex{};
    for (const auto &r : e) {
      RhythmEvent re{.beat = r["beat"].get<uint32_t>(),
                     .gamepadButton = r["gamepadButton"].get<size_t>()};
      _rhythmBars[barIndex].rhythmEvents[eventIndex] = re;
      eventIndex++;
    }
    barIndex++;
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
  if (_rhythmEventIndex >= _rhythmBars[_rhythmBarIndex].nEvents ||
      _rhythmBarIndex < 0) {  // TODO: Fix the rhythm bar index starting at 0
    return;
  }

  RhythmEvent rhythmEvent =
      _rhythmBars[_rhythmBarIndex].rhythmEvents[_rhythmEventIndex];

  int32_t distance = mp.beatRel - rhythmEvent.beat;

  // If the player was too late to hit the target
  if (distance > BEAT_WINDOW) {
    _playerHealth--;
    _rhythmEventIndex++;

    // if (_playerHealth <= 0) {
    //   std::cout << "DEAD" << std::endl;
    //   frameEvents.addEvent(EventType::PLAYER_DEATH);
    //   return;
    // }

    frameEvents.addEvent(EventType::PLAYER_FAIL);
    return;
  }

  // If pressed any incorrect button
  for (size_t i{}; i < gamepadState.size(); i++) {
    if (i != rhythmEvent.gamepadButton && gamepadState[i]) {
      _playerHealth--;
      _rhythmEventIndex++;

      // if (_playerHealth <= 0) {
      //   frameEvents.addEvent(EventType::PLAYER_DEATH);
      //   return;
      // }

      frameEvents.addEvent(EventType::PLAYER_FAIL);
      return;
    }
  }

  // If pressed the correct button
  // TODO: Need higher resolution here. Being perfect is too easy
  if (rhythmEvent.gamepadButton != GAMEPAD_NONE &&
      gamepadState[rhythmEvent.gamepadButton]) {
    const auto absDistance = abs(distance);

    if (absDistance == 0) {
      std::cout << "perfect" << std::endl;
      frameEvents.addEvent(EventType::PLAYER_PERFECT);
    } else if (absDistance < 2) {
      std::cout << "ok" << std::endl;
      frameEvents.addEvent(EventType::PLAYER_OK);
    } else {
      std::cout << "bad" << std::endl;
      frameEvents.addEvent(EventType::PLAYER_BAD);
    }
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
  // For every new bar, switch between talking and listening
  if (mp.beatRel == 0) {
    // If going from talking to listening
    if (_talking) {
      _rhythmEventIndex = 0;
      _talking = false;
    } else {  // Going from listening to a new round of talking
      _rhythmBarIndex++;
      _rhythmEventIndex = 0;
      _talking = true;

      if (_rhythmBarIndex >= _nRhythmBars) {
        _rhythmBarIndex = 0;
      }
    }
  }

  if (_talking) {
    if (_rhythmEventIndex < _rhythmBars[_rhythmBarIndex].nEvents) {
      auto rhythmEvent =
          _rhythmBars[_rhythmBarIndex].rhythmEvents[_rhythmEventIndex];
      if (rhythmEvent.beat % 16 == mp.beatRel) {
        std::cout << "rhythmEvent: " << rhythmEvent.gamepadButton << std::endl;
        _rhythmEventIndex++;
      }
    }
  }
}
