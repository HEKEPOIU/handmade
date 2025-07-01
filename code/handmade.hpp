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

struct thread_context {
  int32_t Placeholder;
};

// -- Platform layers

#if HANDMADE_INTERNAL
struct debug_read_file_result {
  uint32_t ContentSize;
  void *Contents;
};

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name)                                  \
  debug_read_file_result name(thread_context *Thread, const char *FileName)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context* Thread, void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name)                                 \
  bool32_t name(thread_context *Thread, const char *FileName, uint32_t MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif

// -- Game layers
struct game_offscreen_buffer {
  void *Memory;
  int32_t Width;
  int32_t Height;
  int32_t Pitch;
  int32_t BytesPerPixel;
};
struct game_sound_output_buffer {
  int32_t SamplePerSecond;
  int32_t SamplesCount;
  int16_t *Samples;
};

struct game_button_state {
  int32_t HalfTransition;
  bool32_t EndedDown;
};

struct game_controller_input {
  bool32_t IsConnected;
  bool32_t IsAnalog;
  real32_t StickAverageX;
  real32_t StickAverageY;
  union {
    game_button_state Bottons[12];
    struct {
      game_button_state MoveUp;
      game_button_state MoveDown;
      game_button_state MoveLeft;
      game_button_state MoveRight;
      game_button_state ActionUp;
      game_button_state ActionDown;
      game_button_state ActionLeft;
      game_button_state ActionRight;
      game_button_state LeftShoulder;
      game_button_state RightShoulder;
      game_button_state Back;
      game_button_state Start;

      // -- NOTE: This is a terminator, it is Checked by Assert.
      game_button_state Terminator;
    };
  };
};

struct game_input {
  game_button_state MouseButtons[5];
  int32_t MouseX;
  int32_t MouseY;
  int32_t MouseZ;
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

  real32_t tSine;
  int32_t PlayerX;
  int32_t PlayerY;
  real32_t tJump;
};

struct game_memory {
  bool32_t IsInitialized;
  int64_t PermanentStorageSize;
  int64_t TransientStorageSize;
  void *PermanentStorage; // NOTE: Rquire to be cleared to zero at startup.
  void *TransientStorage;
  debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
  debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
  debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
};

#define GAME_UPDATE_AND_RENDER(name)                                           \
  void name(                                                                   \
      thread_context* Thread, game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer)

typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
// in windows api, 0 is success, but we want to return error.
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub) {}

#define GAME_GET_SOUND_SAMPLE(name)                                            \
  void name(thread_context* Thread, game_memory *Memory, game_sound_output_buffer *SoundBuffer)

typedef GAME_GET_SOUND_SAMPLE(game_get_sound_sample);
GAME_GET_SOUND_SAMPLE(GameGetSoundSampleStub) {}

struct game_code {
  game_update_and_render *UpdateAndRender;
  game_get_sound_sample *GetSoundSample;
};
