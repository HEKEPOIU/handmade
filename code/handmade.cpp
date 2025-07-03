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

internal uint32_t RoundReal32ToUInt32(real32_t Value) {
  return (uint32_t)(Value + 0.5f);
}
internal int32_t RoundReal32ToInt32(real32_t Value) {
  return (int32_t)(Value + 0.5f);
}
internal void DrawRectangle(game_offscreen_buffer *Buffer,
                            real32_t MinX,
                            real32_t MinY,
                            real32_t MaxX,
                            real32_t MaxY,
                            real32_t R,
                            real32_t G,
                            real32_t B) {
  int32_t _MinX = Max(RoundReal32ToInt32(MinX), 0);
  int32_t _MaxX = Min(RoundReal32ToInt32(MaxX), Buffer->Width);
  int32_t _MinY = Max(RoundReal32ToInt32(MinY), 0);
  int32_t _MaxY = Min(RoundReal32ToInt32(MaxY), Buffer->Height);

  uint8_t *EndOfBuffer =
      (uint8_t *)Buffer->Memory + Buffer->Pitch * Buffer->Height;

  uint8_t *Pixels = (uint8_t *)Buffer->Memory + _MinX * Buffer->BytesPerPixel +
                    Buffer->Pitch * _MinY;
  uint32_t Color =
      (RoundReal32ToUInt32(R * 255.0f) << 16 |
       RoundReal32ToUInt32(G * 255.0f) << 8 | RoundReal32ToUInt32(B * 255.0f));
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
      real32_t PlayerDirX = 0;
      real32_t PlayerDirY = 0;
      real32_t speed = 500;
      if (Controller->MoveUp.EndedDown) { PlayerDirY -= 1; }
      if (Controller->MoveDown.EndedDown) { PlayerDirY += 1; }
      if (Controller->MoveLeft.EndedDown) { PlayerDirX -= 1; }
      if (Controller->MoveRight.EndedDown) { PlayerDirX += 1; }
      GameState->PlayerX += PlayerDirX * speed * Input->DeltaTime;
      GameState->PlayerY += PlayerDirY * speed * Input->DeltaTime;
    }
  }
  uint32_t TileMap[9][17] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1},
      {1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1},
      {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0},
      {1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0},
      {1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0},
      {0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1}};
  DrawRectangle(Buffer,
                0,
                0,
                (real32_t)Buffer->Width,
                (real32_t)Buffer->Height,
                0.7f,
                0.8f,
                0.7f);

  int32_t XPadding = -30;
  int32_t YPadding = 0;
  uint32_t TileHeight = 60;
  uint32_t TileWeight = 60;
  for (uint32_t Row = 0; Row < 9; ++Row) {
    real32_t MinY = real32_t(YPadding + Row * TileHeight);
    real32_t MaxY = real32_t(YPadding + (Row + 1) * TileHeight);
    for (uint32_t Column = 0; Column < 17; ++Column) {
      if (TileMap[Row][Column] != 1) continue;
      real32_t MinX = real32_t(XPadding + Column * TileWeight);
      real32_t MaxX = real32_t(XPadding + (Column + 1) * TileWeight);
      DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, 0.5f, 0.5f, 0.5f);
    }
  }

  real32_t PlayerWeight = 0.75f * TileWeight;
  real32_t PlayerHeight = (real32_t)TileHeight;
  real32_t PlayerMinX = GameState->PlayerX - 0.5f * PlayerWeight;
  real32_t PlayerMinY = GameState->PlayerY - 0.5f * PlayerHeight;

  DrawRectangle(Buffer,
                PlayerMinX,
                PlayerMinY,
                PlayerMinX + PlayerWeight,
                PlayerMinY + PlayerHeight,
                1,
                1,
                0);
};

extern "C" GAME_GET_SOUND_SAMPLE(GameGetSoundSample) {
  game_state *GameState = (game_state *)Memory->PermanentStorage;

  GameOutputSound(GameState, SoundBuffer);
}
