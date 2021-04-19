#include "audio.hpp"

#include <stdint.h>

#include <iostream>

#include "bs_types.hpp"

AudioEngine::AudioEngine() { _soloud.init(); }

AudioEngine::~AudioEngine() { _soloud.deinit(); }

void AudioEngine::load(const char *filename, double bpm) {
  _wavStream.load(filename);
  _bpm = bpm;
  _spb = (1. / (bpm / 60.));  // / 8.;  // 32th beat
}

void AudioEngine::play() { _wavHandle = _soloud.play(_wavStream); }

void AudioEngine::playBackground() {
  _wavHandle = _soloud.playBackground(_wavStream);
}

bs::MusicPos AudioEngine::update(float deltaTime) {
  _currentTime = _soloud.getStreamTime(_wavHandle) + 1.;

  _musicPos.beat = _currentTime / _spb;
  _musicPos.period = _musicPos.beat / 64;
  _musicPos.barRel = (_musicPos.beat / 32) % 4;
  _musicPos.beatRel = _musicPos.beat % 32;

  return _musicPos;
}