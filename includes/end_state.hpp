#ifndef __END_STATE_H_
#define __END_STATE_H_

#include "game_state.hpp"
class GameStateManager;

class EndState : public GameState {
 public:
  EndState(GameStateManager &gameStateManager);
  void onEnter();
  void onExit();

  void update(float dt, const MusicPos &mp, const GamepadState &gamepadState,
              FrameEvents &frameEvents);

  void rUpdate(const MusicPos &mp, const GamepadState &gamepadState,
               FrameEvents &frameEvents);
};

#endif  // __END_STATE_H_
