#ifndef __GAME_STATE_H_
#define __GAME_STATE_H_

#include "bs_types.hpp"

class GameStateManager;

class GameState {
 public:
  GameState(GameStateManager &gameStateManager)
      : _gameStateManager{gameStateManager} {};
  // virtual ~GameState() = 0;
  // Called after being pushed on the stack
  virtual void onEnter() = 0;
  // Called before being popped off the stack
  virtual void onExit() = 0;
  // Called before another state is pushed on top of this one
  virtual void onObscure() = 0;
  // Called after becoming the topmost state again
  virtual void onReveal() = 0;
  virtual void update(float dt, const MusicPos &mp,
                      const GamepadState &gamepadState,
                      FrameEvents &frameEvents) = 0;
  // Rhythmic update
  virtual void rUpdate(const MusicPos &mp, const GamepadState &gamepadState,
                       FrameEvents &frameEvents) = 0;

  // protected:
  //  virtual void processInput(const GamepadState &gamepadState,
  //                            const MusicPos &mp, FrameEvents &frameEvents);
  //
 private:
  virtual void processInput(const GamepadState &gamepadState,
                            const MusicPos &mp, FrameEvents &frameEvents) = 0;

 protected:
  GameStateManager &_gameStateManager;
};

#endif  // __GAME_STATE_H_
