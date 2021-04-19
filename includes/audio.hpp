#ifndef __AUDIO_H_
#define __AUDIO_H_

#include "bs_types.hpp"
#include "soloud.h"
#include "soloud_wavstream.h"

class AudioEngine {
 public:
  AudioEngine();
  ~AudioEngine();

  void load(const char *filename, double bpm);
  void play();
  void playBackground();
  bs::MusicPos update(float deltaTime);

 private:
  SoLoud::Soloud _soloud;
  SoLoud::WavStream _wavStream;
  double _bpm;
  double _spb;
  int _wavHandle;
  double _currentTime;
  bs::MusicPos _musicPos;
};

#endif  // __AUDIO_H_
