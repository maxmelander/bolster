#ifndef __START_STATE_H_
#define __START_STATE_H_

#include "game_state.hpp"
class GameStateManager;

class StartState : public GameState {
 public:
  StartState(GameStateManager &gameStateManager);
  void onEnter();
  void onExit();

  void update(float dt, const MusicPos &mp, const GamepadState &gamepadState,
              FrameEvents &frameEvents);

  void rUpdate(const MusicPos &mp, const GamepadState &gamepadState,
               FrameEvents &frameEvents);
};

#endif  // __START_STATE_H_
