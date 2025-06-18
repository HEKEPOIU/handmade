#pragma once

#include "Constants.hpp"
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
  int32_t LatencySampleCount;
};

struct win32_debug_time_marker {
  DWORD PlayCursor;
  DWORD WriteCursor;
};
