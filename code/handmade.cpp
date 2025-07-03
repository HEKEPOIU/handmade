#include "handmade.hpp"
#include <cmath>

internal void GameOutputSound(game_state *GameState,
                              game_sound_output_buffer *SoundBuffer) {

  int16_t ToneVolume = 1000;
  int16_t *SamplesOut = SoundBuffer->Samples;
  int32_t WavePeriod = SoundBuffer->SamplePerSecond / 400;

  for (int32_t SampleIndex = 0; SampleIndex < SoundBuffer->SamplesCount;
       SampleIndex++) {
#if 0
    real32_t SineValue = sinf(GameState->tSine);
    int16_t SampleValue = (int16_t)(SineValue * ToneVolume);
    *SamplesOut++ = SampleValue;
    *SamplesOut++ = SampleValue;
    GameState->tSine += 2 * Pi32 * 1.0f / WavePeriod;
    // NOTE: Wrap around, if not do so, it will have floating point Precision
    // issue.
    if (GameState->tSine > 2.f * Pi32) { GameState->tSine -= 2.f * Pi32; }
#endif
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
internal int32_t RoundReal32ToInt32(real32_t Value) {
  return (int32_t)(Value + 0.5f);
}
#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Min(a, b) ((a) < (b) ? (a) : (b))
internal void DrawRectangle(game_offscreen_buffer *Buffer,
                            real32_t MinX,
                            real32_t MinY,
                            real32_t MaxX,
                            real32_t MaxY,
                            uint32_t Color) {
  int32_t _MinX = Max(RoundReal32ToInt32(MinX), 0);
  int32_t _MaxX = Min(RoundReal32ToInt32(MaxX), Buffer->Width);
  int32_t _MinY = Max(RoundReal32ToInt32(MinY), 0);
  int32_t _MaxY = Min(RoundReal32ToInt32(MaxY), Buffer->Height);

  uint8_t *EndOfBuffer =
      (uint8_t *)Buffer->Memory + Buffer->Pitch * Buffer->Height;

  uint8_t *Pixels = (uint8_t *)Buffer->Memory + _MinX * Buffer->BytesPerPixel +
                    Buffer->Pitch * _MinY;
  for (int32_t Y = _MinY; Y < _MaxY; ++Y) {
    uint32_t *Pixel = (uint32_t *)Pixels;
    for (int32_t X = _MinX; X < _MaxX; ++X) {
      *Pixel++ = Color;
    }
    Pixels += Buffer->Pitch;
  }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert(&Input->Controllers[0].Terminator -
             &Input->Controllers[0].Bottons[0] ==
         (ArrayCount(Input->Controllers[0].Bottons)));
  Assert(sizeof(game_state) <= (uint64_t)Memory->PermanentStorageSize);
  game_state *GameState = (game_state *)Memory->PermanentStorage;
  if (!Memory->IsInitialized) { Memory->IsInitialized = true; }
  for (int32_t ControllerIndex = 0; ControllerIndex < 5; ++ControllerIndex) {
    game_controller_input *Controller = GetController(Input, ControllerIndex);
    if (Controller->IsAnalog) {
    } else {
    }
  }
  DrawRectangle(Buffer,
                0,
                0,
                (real32_t)Buffer->Width,
                (real32_t)Buffer->Height,
                0x00000000);
  DrawRectangle(Buffer, 10, 10, 20, 20, 0xFFFFFFFF);
};

extern "C" GAME_GET_SOUND_SAMPLE(GameGetSoundSample) {
  game_state *GameState = (game_state *)Memory->PermanentStorage;

  GameOutputSound(GameState, SoundBuffer);
}
