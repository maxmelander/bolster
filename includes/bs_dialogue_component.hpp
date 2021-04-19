#ifndef __BS_DIALOG_COMPONENT_H_
#define __BS_DIALOG_COMPONENT_H_

#include <array>
#include <string>

#include "bs_types.hpp"

namespace bs {

constexpr size_t MAX_LINE_LENGTH = 4;
constexpr size_t MAX_DIALOGUE_LINES = 4;

struct DialogueLine {
  std::array<size_t, MAX_LINE_LENGTH> symbols;
};

class DialogueComponent {
 public:
  DialogueComponent();

  virtual void update(float deltaTime, MusicPos musicPos,
                      bool buttonsPressed[4]);

  size_t getCurrentSymbol();
  void speak();
  bool listen(bool buttonsPressed[4]);
  bool progressLine();

 private:
  bool _speaking;
  float _currentTime;
  size_t _currentLine;
  size_t _currentSymbol;
  MusicPos _lastMusicPos;
  std::array<DialogueLine, MAX_DIALOGUE_LINES> _lines;
};
}  // namespace bs

#endif  // __BS_DIALOG_COMPONENT_H_
