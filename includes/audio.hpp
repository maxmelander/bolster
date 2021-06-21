#ifndef __AUDIO_H_
#define __AUDIO_H_

#include "bs_types.hpp"
#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_wavstream.h"

class AudioEngine {
 public:
  AudioEngine();
  ~AudioEngine();

  void load(const char *filename, double bpm);
  void play();
  void processEvents(const FrameEvents &frameEvents);
  MusicPos update(float deltaTime);

 private:
  void playBackground();
  void stopBackground();

 private:
  SoLoud::Soloud _soloud;
  SoLoud::WavStream _wavStream;
  SoLoud::Wav _downWav;
  SoLoud::Wav _rightWav;
  SoLoud::Wav _successWav;
  double _bpm;
  double _spb;
  int _wavHandle;
  double _currentTime;
  MusicPos _musicPos;
};

#endif  // __AUDIO_H_
