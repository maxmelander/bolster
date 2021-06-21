#include "audio.hpp"

#include <stdint.h>

#include <iostream>

#include "bs_types.hpp"

AudioEngine::AudioEngine() {
  _soloud.init();
  load("../audio/b2.mp3", 84.5);

  _downWav.load("../audio/down.wav");
  _rightWav.load("../audio/right.wav");
  _successWav.load("../audio/success.mp3");
}

AudioEngine::~AudioEngine() { _soloud.deinit(); }

void AudioEngine::load(const char *filename, double bpm) {
  _wavStream.load(filename);
  _bpm = bpm;
  _spb = (1. / (bpm / 60.)) / 4.;  // 16th beat
}

void AudioEngine::play() { _wavHandle = _soloud.play(_wavStream); }

void AudioEngine::playBackground() {
  _wavHandle = _soloud.playBackground(_wavStream);
}

void AudioEngine::stopBackground() { _soloud.stop(_wavHandle); }

void AudioEngine::processEvents(const FrameEvents &frameEvents) {
  // Check for relevant frame events
  // TODO: cleaner contains type functionality in frameEvents maybe?
  for (size_t i{}; i < frameEvents.nEvents; i++) {
    switch (frameEvents.events[i]) {
      case EventType::GAME_START:
        playBackground();
        break;
      case EventType::GAME_END:
      case EventType::PLAYER_DEATH:
        stopBackground();
        break;

      case EventType::RHYTHM_DOWN:
        _soloud.play(_downWav);
        break;
      case EventType::RHYTHM_RIGHT:
        _soloud.play(_rightWav);
        break;
      case EventType::PLAYER_PERFECT:
      case EventType::PLAYER_OK: {
        int h = _soloud.play(_successWav, 1, 0, 1);  // start paused
        _soloud.seek(h, 0.38f);                      // seek
        _soloud.setPause(h, 0);                      // unpause
        break;
      }
      default:
        break;
    }
  }
}

MusicPos AudioEngine::update(float deltaTime) {
  // Calculate current music pos, if playing
  _currentTime = _soloud.getStreamTime(_wavHandle) - 3.00;

  _musicPos.beat = _currentTime / _spb;
  _musicPos.period = _musicPos.beat / 64;
  _musicPos.barRel = (_musicPos.beat / 64) % 4;
  _musicPos.beatRel = _musicPos.beat % 16;

  return _musicPos;
}
