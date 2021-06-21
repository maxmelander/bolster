#ifndef __RHYTHMIC_STATE_H_
#define __RHYTHMIC_STATE_H_

#include <array>

#include "bs_types.hpp"
#include "dstack.hpp"
#include "game_state.hpp"

// Forward declaration
class GameStateManager;

class RhythmicState : public GameState {
  static constexpr int32_t BEAT_WINDOW = 2;

 public:
  RhythmicState(uint32_t level, GameStateManager &gameStateManager,
                DStack &allocator);
  void onEnter();
  void onExit();
  void update(float dt, const MusicPos &mp, const GamepadState &gamepadState,
              FrameEvents &frameEvents);

  void rUpdate(const MusicPos &mp, const GamepadState &gamepadState,
               FrameEvents &frameEvents);

 private:
  void processInput(const GamepadState &gamepadState, const MusicPos &mp,
                    FrameEvents &frameEvents);
  void loadData(uint32_t level, DStack &allocator);

 private:
  bool _talking;
  uint32_t _playerHealth;

  int16_t _rhythmBarIndex;
  int16_t _rhythmEventIndex;

  size_t _nRhythmBars;

  RhythmBar *_rhythmBars;
  // std::array<RhythmEvent, 4> _rhythmEvents;
};

#endif  // __RHYTHMIC_STATE_H_
