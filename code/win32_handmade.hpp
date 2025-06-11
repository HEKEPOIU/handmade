#pragma once

#include "Constants.hpp"
#include <windows.h>

struct win32_offscreen_buffer {
  // Buffer pixel size always be 32 bit, little endian.
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
};

struct win32_window_dimension {
  int Width;
  int Height;
};

struct win32_sound_output {
  uint32_t RunningSampleIndex;
  real32_t tSine;
  int SamplePerSecond;
  // LEFT RIGHT LEFT RIGHT LEFT RIGHT
  // the sample contains left and right channel.
  int BytePerSample;
  int SecondaryBufferSize;
  int LatencySampleCount;
};
