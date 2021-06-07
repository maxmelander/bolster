#ifndef __GAME_STATE_MANAGER_H_
#define __GAME_STATE_MANAGER_H_

#include <array>

#include "bs_types.hpp"
#include "dstack.hpp"
#include "game_state.hpp"

class GameStateManager {
 public:
  GameStateManager(DStack &allocator);
  ~GameStateManager();

  // Copy constructor
  GameStateManager(const GameStateManager &) = delete;
  // Move constructor
  GameStateManager(const GameStateManager &&) = delete;
  // Copy assignment
  GameStateManager &operator=(const GameStateManager &) = delete;
  // Move assignment
  GameStateManager &operator=(const GameStateManager &&) = delete;

  void nextState() noexcept;
  void previousState() noexcept;
  void restartState() noexcept;
  void restartGame() noexcept;

  void update(float dt, const MusicPos &mp, const GamepadState &gamepadState,
              FrameEvents &frameEvents);

  void rUpdate(const MusicPos &mp, const GamepadState &gamepadState,
               FrameEvents &frameEvents);

 private:
  size_t _gameStateIndex;
  std::array<GameState *, 5> _gameStates;
};

#endif  // __GAME_STATE_MANAGER_H_
