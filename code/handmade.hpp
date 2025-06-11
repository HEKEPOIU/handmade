#pragma once
#include "Constants.hpp"

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

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

struct game_botton_state {
  int32_t HalfTransition;
  bool32_t EndedDown;
};

struct game_controller_input {
  bool32_t IsAnalog;
  real32_t StartX;
  real32_t StartY;

  real32_t MinX;
  real32_t MinY;
  real32_t MaxX;
  real32_t MaxY;

  real32_t EndX;
  real32_t EndY;
  union {
    game_botton_state Botton[6];
    struct {
      game_botton_state Up;
      game_botton_state Down;
      game_botton_state Left;
      game_botton_state Right;
      game_botton_state LeftShoulder;
      game_botton_state RightShoulder;
    };
  };
};

struct game_input {
  game_controller_input Controllers[4];
};

internal void GameUpdateAndRender(game_input *Input,
                                  game_offscreen_buffer *Buffer,
                                  game_sound_output_buffer *SoundBuffer);
