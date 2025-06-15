#include "handmade.hpp"
#include <cmath>

internal void GameOutputSound(game_sound_output_buffer *SoundBuffer,
                              int32_t ToneHz) {

  local_persist real32_t tSine = 0;
  int16_t ToneVolume = 1000;
  int16_t *SamplesOut = SoundBuffer->Samples;
  int32_t WavePeriod = SoundBuffer->SamplePerSecond / ToneHz;

  for (int32_t SampleIndex = 0; SampleIndex < SoundBuffer->SamplesCount;
       SampleIndex++) {
    real32_t SineValue = sinf(tSine);
    int16_t SampleValue = (int16_t)(SineValue * ToneVolume);
    *SamplesOut++ = SampleValue;
    *SamplesOut++ = SampleValue;
    tSine += 2 * Pi32 * 1.0f / WavePeriod;
  }
}

internal void RenderWeirdGradient(game_offscreen_buffer *buffer,
                                  int BlueOffset,
                                  int GreenOffset) {
  uint8_t *Row = (uint8_t *)buffer->Memory;
  for (int Y = 0; Y < buffer->Height; ++Y) {
    uint32_t *Pixel = (uint32_t *)Row;
    for (int X = 0; X < buffer->Width; ++X) {
      /*
        LITTLE ENDIAN ARCHITECTURE
        Pixel in memory: RR GG BB xx
        Pixel in Register 0x xxBBGGRR
        -- > Load the memory from left to right, fill out the Register form
        right to left, and windows programmer want Register read like xxRRGGBB.
        Pixel in memory: BB GG RR xx
        Pixel in Register: 0x xxRRGGBB
      */
      uint8_t Green = (uint8_t)(Y + GreenOffset);
      uint8_t Blue = (uint8_t)(X + BlueOffset);

      *Pixel++ = ((Green << 8) | Blue);
    }
    // Pitch -> 4 * 8 * Width  -> 32 BitCount from top
    Row += buffer->Pitch;
  }
}

internal void GameUpdateAndRender(game_memory *Memory,
                                  game_input *Input,
                                  game_offscreen_buffer *Buffer,
                                  game_sound_output_buffer *SoundBuffer) {
  Assert(sizeof(game_state) <= (uint64_t)Memory->PermanentStorageSize);
  game_state *GameState = (game_state *)Memory->PermanentStorage;
  if (!Memory->IsInitialized) {
    const char *FileName = __FILE__;
    debug_read_file_result File = DEBUGPlatformReadEntireFile(FileName);
    if (File.Contents) {
      DEBUGPlatformWriteEntireFile(
          "D:\\handmade\\data\\test.out", File.ContentSize, File.Contents);
      DEBUGPlatformFreeFile(File.Contents);
    }
        
    GameState->ToneHz = 256;
    Memory->IsInitialized = true;
  }
  game_controller_input *Input0 = &Input->Controllers[0];
  if (Input0->IsAnalog) {
    GameState->BlueOffset += (int32_t)(4.0f * Input0->EndX);
    GameState->ToneHz = 256 + (int32_t)(128.0f * (Input0->EndY));
  } else {
  }

  if (Input0->Down.EndedDown) { GameState->GreenOffset += 1; }

  GameOutputSound(SoundBuffer, GameState->ToneHz);
  RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
};
