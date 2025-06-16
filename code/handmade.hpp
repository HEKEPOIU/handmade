#pragma once
#include "Constants.hpp"

#if HANDMADE_SLOW
#define Assert(Expression)                                                     \
  if (!(Expression)) { *(int *)0 = 0; }
#else
#define Assert(Expression)
#endif
#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

inline uint32_t SafeTruncateUInt32(uint64_t Value) {
  Assert(Value <= UINT32_MAX);
  return (uint32_t)Value;
}

// -- Platform layers

#if HANDMADE_INTERNAL
struct debug_read_file_result {
  uint32_t ContentSize;
  void *Contents;
};
internal debug_read_file_result
DEBUGPlatformReadEntireFile(const char *FileName);
internal void DEBUGPlatformFreeFile(void *Memory);
internal bool32_t DEBUGPlatformWriteEntireFile(const char *FileName,
                                               uint32_t MemorySize,
                                               void *Memory);
#endif

// -- Game layers
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
  bool32_t IsConnected;
  bool32_t IsAnalog;
  real32_t StickAverageX;
  real32_t StickAverageY;
  union {
    game_botton_state Bottons[12];
    struct {
      game_botton_state MoveUp;
      game_botton_state MoveDown;
      game_botton_state MoveLeft;
      game_botton_state MoveRight;
      game_botton_state ActionUp;
      game_botton_state ActionDown;
      game_botton_state ActionLeft;
      game_botton_state ActionRight;
      game_botton_state LeftShoulder;
      game_botton_state RightShoulder;
      game_botton_state Back;
      game_botton_state Start;


      // -- NOTE: This is a terminator, it is Checked by Assert.
      game_botton_state Terminator;
    };
  };
};

struct game_input {
  game_controller_input Controllers[5];
};

inline game_controller_input *GetController(game_input *Input, uint32_t Index) {
  Assert(Index >= 0 && (size_t)Index < ArrayCount(Input->Controllers));
  return &Input->Controllers[Index];
}

struct game_state {
  int32_t BlueOffset;
  int32_t GreenOffset;
  int32_t ToneHz;
};

struct game_memory {
  bool32_t IsInitialized;
  int64_t PermanentStorageSize;
  int64_t TransientStorageSize;
  void *PermanentStorage; // NOTE: Rquire to be cleared to zero at startup.
  void *TransientStorage;
};

internal void GameUpdateAndRender(game_memory *Memory,
                                  game_input *Input,
                                  game_offscreen_buffer *Buffer,
                                  game_sound_output_buffer *SoundBuffer);
