#include "handmade.hpp"
#include <cmath>

void GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer) {

  int16_t ToneVolume = 1000;
  int16_t *SamplesOut = SoundBuffer->Samples;
  int32_t WavePeriod = SoundBuffer->SamplePerSecond / GameState->ToneHz;

  for (int32_t SampleIndex = 0; SampleIndex < SoundBuffer->SamplesCount;
       SampleIndex++) {
    real32_t SineValue = sinf(GameState->tSine);
    int16_t SampleValue = (int16_t)(SineValue * ToneVolume);
    *SamplesOut++ = SampleValue;
    *SamplesOut++ = SampleValue;
    GameState->tSine += 2 * Pi32 * 1.0f / WavePeriod;
    // NOTE: Wrap around, if not do so, it will have floating point Precision
    // issue.
    if (GameState->tSine > 2.f * Pi32) { GameState->tSine -= 2.f * Pi32; }
  }
}

void RenderWeirdGradient(game_offscreen_buffer *buffer,
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

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert(&Input->Controllers[0].Terminator -
             &Input->Controllers[0].Bottons[0] ==
         (ArrayCount(Input->Controllers[0].Bottons)));
  Assert(sizeof(game_state) <= (uint64_t)Memory->PermanentStorageSize);
  game_state *GameState = (game_state *)Memory->PermanentStorage;
  if (!Memory->IsInitialized) {
    const char *FileName = __FILE__;
    debug_read_file_result File = Memory->DEBUGPlatformReadEntireFile(FileName);
    if (File.Contents) {
      Memory->DEBUGPlatformWriteEntireFile(
          "D:\\handmade\\data\\test.out", File.ContentSize, File.Contents);
      Memory->DEBUGPlatformFreeFileMemory(File.Contents);
    }

    GameState->ToneHz = 256;
    GameState->tSine = 0;
    Memory->IsInitialized = true;
  }
  for (int32_t ControllerIndex = 0; ControllerIndex < 5; ++ControllerIndex) {
    game_controller_input *Controller = GetController(Input, ControllerIndex);
    if (Controller->IsAnalog) {
      GameState->BlueOffset += (int32_t)(4.0f * Controller->StickAverageX);
      GameState->ToneHz = 256 + (int32_t)(128.0f * (Controller->StickAverageY));
    } else {
      if (Controller->MoveLeft.EndedDown) { GameState->BlueOffset -= 1; }
      if (Controller->MoveRight.EndedDown) { GameState->BlueOffset += 1; }
    }

    if (Controller->ActionDown.EndedDown) { GameState->GreenOffset += 1; }
  }

  RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
};

extern "C" GAME_GET_SOUND_SAMPLE(GameGetSoundSample) {
  game_state *GameState = (game_state *)Memory->PermanentStorage;

  GameOutputSound(GameState, SoundBuffer);
}
