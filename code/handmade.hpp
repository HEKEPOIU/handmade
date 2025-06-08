#pragma once
#include "Constants.hpp"

struct game_offscreen_buffer {
  void *Memory;
  int Width;
  int Height;
  int Pitch;
};
struct game_sound_output_buffer {
  int32_t SamplePerSecond;
  int32_t SamplesCount;
  int16_t *Samples;
};

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer,
                                  int BlueOffset,
                                  int GreenOffset,
                                  game_sound_output_buffer *SoundBuffer);
