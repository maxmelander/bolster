#include "bs_dialogue_component.hpp"

#include <iostream>

#include "bs_types.hpp"

namespace bs {
DialogueComponent::DialogueComponent()
    : _speaking{true},
      _currentTime{0},
      _currentLine{0},
      _currentSymbol{0},
      _lastMusicPos{0, 0, 0},
      _lines{DialogueLine{{GAMEPAD_A, GAMEPAD_A, GAMEPAD_B, GAMEPAD_NONE}},
             DialogueLine{{GAMEPAD_A, GAMEPAD_B, GAMEPAD_B, GAMEPAD_B}}} {}

size_t DialogueComponent::getCurrentSymbol() {
  return _lines[_currentLine].symbols[_currentSymbol];
}

bool DialogueComponent::progressLine() {
  _currentSymbol++;
  if (_currentSymbol == _lines[_currentLine].symbols.size()) {
    _currentSymbol = 0;
    return false;
  }

  return true;
}

void DialogueComponent::speak() {
  size_t symbol = getCurrentSymbol();

  if (symbol == GAMEPAD_A) {
    std::cout << "A ";
  } else if (symbol == GAMEPAD_B) {
    std::cout << "B ";
  } else if (symbol == GAMEPAD_X) {
    std::cout << "X ";
  } else if (symbol == GAMEPAD_Y) {
    std::cout << "Y ";
  }
}

// Returns true if the user input is correct
bool DialogueComponent::listen(bool buttonsPressed[4]) {
  size_t symbol = getCurrentSymbol();
  if (symbol != GAMEPAD_NONE) {
    if (buttonsPressed[symbol]) {
      std::cout << "C ";
      ;
    } else {
      std::cout << "W ";
    }
    return buttonsPressed[symbol];
  }
  return true;
}

void DialogueComponent::update(float deltaTime, MusicPos musicPos,
                               bool buttonsPressed[4]) {
  // New part of the music
  if (musicPos != _lastMusicPos) {
    _lastMusicPos = musicPos;
  }

  if (_speaking) {
    _currentTime += deltaTime;

    // Wait between symbols
    if (_currentTime > 0.2f) {
      _currentTime = 0;

      speak();
      if (!progressLine()) {
        std::cout << std::endl;
        _speaking = false;
      }
    }
  } else {
    // If a symbol is NONE, skip it
    if (getCurrentSymbol() == GAMEPAD_NONE) {
      if (!progressLine()) {
        _speaking = true;
        _currentLine++;
      }
    } else if (buttonsPressed[GAMEPAD_A] || buttonsPressed[GAMEPAD_B] ||
               buttonsPressed[GAMEPAD_X] || buttonsPressed[GAMEPAD_Y]) {
      listen(buttonsPressed);
      if (!progressLine()) {
        _speaking = true;
        _currentLine++;
      }
    }
  }
}
}  // namespace bs
