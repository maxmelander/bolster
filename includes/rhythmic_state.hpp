#ifndef __START_STATE_H_
#define __START_STATE_H_

#include <array>

#include "bs_types.hpp"
#include "game_state.hpp"

// Forward declaration
class GameStateManager;

class RhythmicState : public GameState {
  static constexpr int32_t BEAT_WINDOW = 2;

 public:
  RhythmicState(uint32_t level, GameStateManager &gameStateManager);
  void onEnter();
  void onExit();
  void onObscure();
  void onReveal();
  void update(float dt, const MusicPos &mp, const GamepadState &gamepadState,
              FrameEvents &frameEvents);

  void rUpdate(const MusicPos &mp, const GamepadState &gamepadState,
               FrameEvents &frameEvents);
  // protected:
  //  virtual void processInput(const GamepadState &gamepadState,
  //                            const MusicPos &mp, FrameEvents &frameEvents);
  //
 private:
  void processInput(const GamepadState &gamepadState, const MusicPos &mp,
                    FrameEvents &frameEvents);
  void loadData(uint32_t level);

 private:
  bool _talking;
  uint32_t _playerHealth;
  size_t _rhythmEventIndex;
  std::array<RhythmEvent, 4> _rhythmEvents;
};

#endif  // __START_STATE_H_
