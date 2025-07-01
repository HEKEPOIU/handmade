#pragma once

#include "Constants.hpp"
#include "handmade.hpp"
#include <windows.h>

struct win32_offscreen_buffer {
  // Buffer pixel size always be 32 bit, little endian.
  BITMAPINFO Info;
  void *Memory;
  int32_t Width;
  int32_t Height;
  int32_t Pitch;
  int32_t BytesPerPixel;
};

struct win32_window_dimension {
  int32_t Width;
  int32_t Height;
};

struct win32_sound_output {
  uint32_t RunningSampleIndex;
  real32_t tSine;
  int32_t SamplePerSecond;
  // LEFT RIGHT LEFT RIGHT LEFT RIGHT
  // the sample contains left and right channel.
  int32_t BytePerSample;
  DWORD SecondaryBufferSize;
  DWORD SafetyBytes;
};

struct win32_debug_time_marker {
  DWORD OutputPlayCursor;
  DWORD OutputWriteCursor;
  DWORD OutputLocation;
  DWORD OutputByteCount;
  DWORD ExpectedFlipPlayCursor;

  DWORD FlipPlayCursor;
  DWORD FlipWriteCursor;
};

struct win32_game_code {
  HMODULE GameCodeDLL;
  FILETIME DLLLastWriteTime;
  game_code GameCode;

  bool32_t IsValid;
};

// NOTE: the MAX_PATH are not actuall MaxPath anymore,
//       should use other way to ship in the actual code.
#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_replay_buffer {
  HANDLE FileHandle;
  HANDLE MemoryMap;
  char Filename[WIN32_STATE_FILE_NAME_COUNT];
  void *MemoryBlock;
};
struct win32_state {
  uint64_t TotalSize;
  void *GameMemoryBlock;
  win32_replay_buffer ReplayBuffer[4];

  HANDLE RecordingHandle;
  int32_t InputRecordingIndex;

  HANDLE PlaybackHandle;
  int32_t InputPlayingIndex;
  char EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
  char *OnePastLastSlash;
};
