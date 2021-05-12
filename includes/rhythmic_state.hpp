#ifndef __START_STATE_H_
#define __START_STATE_H_

#include <string>

#include "bs_types.hpp"
#include "game_state.hpp"

// Forward declaration
class GameStateManager;

class RhythmicState : public GameState {
 public:
  RhythmicState(const std::string &name, GameStateManager &gameStateManager);
  void onEnter();
  void onExit();
  void onObscure();
  void onReveal();
  void update(float dt, const MusicPos &mp, const GamepadState &gamepadState,
              FrameEvents &frameEvents);

 private:
  virtual void processInput(const GamepadState &gamepadState,
                            const MusicPos &mp, FrameEvents &frameEvents);

 private:
  std::string _name;
  uint32_t _playerHealth;
};

#endif  // __START_STATE_H_
