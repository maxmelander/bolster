#include "game_state_manager.hpp"

#include <iostream>

#include "bs_types.hpp"
#include "end_state.hpp"
#include "rhythmic_state.hpp"
#include "start_state.hpp"

GameStateManager::GameStateManager(DStack &allocator)
    : _gameStateIndex{0}, _gameStates{} {
  // Alloc room for all our game states
  _gameStates[0] = allocator.alloc<StartState, StackDirection::Bottom>();
  _gameStates[1] = allocator.alloc<RhythmicState, StackDirection::Bottom>();
  _gameStates[2] = allocator.alloc<EndState, StackDirection::Bottom>();
  //
  // Init the game states
  new (_gameStates[0]) StartState{*this};
  new (_gameStates[1]) RhythmicState{1, *this, allocator};
  new (_gameStates[2]) EndState{*this};

  _gameStates[_gameStateIndex]->onEnter();
}

GameStateManager::~GameStateManager() {}

void GameStateManager::nextState() noexcept {
  if (_gameStateIndex + 1 < _gameStates.size()) {
    _gameStates[_gameStateIndex]->onExit();
    _gameStateIndex++;
    _gameStates[_gameStateIndex]->onEnter();
  }
}

void GameStateManager::previousState() noexcept {
  if (_gameStateIndex > 0) {
    _gameStates[_gameStateIndex]->onExit();
    _gameStateIndex--;
    _gameStates[_gameStateIndex]->onEnter();
  }
}

void GameStateManager::restartState() noexcept {
  _gameStates[_gameStateIndex]->onExit();
  _gameStates[_gameStateIndex]->onEnter();
}

void GameStateManager::restartGame() noexcept {
  _gameStates[_gameStateIndex]->onExit();
  _gameStateIndex = 0;
  _gameStates[_gameStateIndex]->onEnter();
}

void GameStateManager::update(float dt, const MusicPos &mp,
                              const GamepadState &gamepadState,
                              FrameEvents &frameEvents) {
  _gameStates[_gameStateIndex]->update(dt, mp, gamepadState, frameEvents);
}

void GameStateManager::rUpdate(const MusicPos &mp,
                               const GamepadState &gamepadState,
                               FrameEvents &frameEvents) {
  _gameStates[_gameStateIndex]->rUpdate(mp, gamepadState, frameEvents);
}
