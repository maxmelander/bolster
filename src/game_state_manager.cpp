#include "game_state_manager.hpp"

#include <iostream>

#include "bs_types.hpp"
#include "rhythmic_state.hpp"

GameStateManager::GameStateManager(DStack &allocator)
    : _gameStateIndex{0}, _gameStates{} {
  // Alloc room for all our game states
  _gameStates[0] = allocator.alloc<RhythmicState, StackDirection::Bottom>();
  // _gameStates[1] = allocator.alloc<RhythmicState, StackDirection::Bottom>();
  // _gameStates[2] = allocator.alloc<RhythmicState, StackDirection::Bottom>();

  // Init the game states
  new (_gameStates[0]) RhythmicState{1, *this};
  // new (_gameStates[1]) RhythmicState{"second state", *this};
  // new (_gameStates[2]) RhythmicState{"third state", *this};
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
