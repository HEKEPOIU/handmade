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

inline uint32_t RoundReal32ToUInt32(real32_t Value) {
  return (uint32_t)(Value + 0.5f);
}
inline int32_t RoundReal32ToInt32(real32_t Value) {
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
#define TILE_ROW_SIZE 9
#define TILE_COLUMN_SIZE 17

internal bool32_t IsTileMapPointEmpty(tile_map *Map,
                                      real32_t TestX,
                                      real32_t TestY) {

  uint32_t TileIndexX = Clamp(
      uint32_t((TestX - Map->StartX) / Map->TileWidth), 0, Map->CountX - 1);
  uint32_t TileIndexY = Clamp(
      uint32_t((TestY - Map->StartY) / Map->TileHeight), 0, Map->CountY - 1);

  if (Map->TileMap[TileIndexX + TileIndexY * Map->CountX] != 0) return false;
  return true;
}

inline tile_map *
GetWorldCurrentTileMap(tile_world *world, int32_t X, int32_t Y) {
  if ((X >= 0 && uint32_t(X) < world->TileMapCountX) &&
      (Y >= 0 && uint32_t(Y) < world->TileMapCountY))
    return &world->TileMaps[Y * world->TileMapCountX + X];
  return NULL;
}

inline void GetWorldIndex(tile_world *world,
                          real32_t WorldX,
                          real32_t WorldY,
                          int32_t *WorldIndexX,
                          int32_t *WorldIndexY) {

  // NOTE:Currently Assert that all world Tilemap are same Height, Width and
  // Count.
  uint32_t MapWidth = world->TileMaps[0].TileWidth * world->TileMaps[0].CountX;
  uint32_t MapHeight =
      world->TileMaps[0].TileHeight * world->TileMaps[0].CountY;
  *WorldIndexX = int32_t(WorldX / MapWidth);
  *WorldIndexY = int32_t(WorldY / MapHeight);
}

inline tile_map *
GetWorldPointTileMap(tile_world *world, real32_t WorldX, real32_t WorldY) {
  int32_t WorldIndexX;
  int32_t WorldIndexY;
  GetWorldIndex(world, WorldX, WorldY, &WorldIndexX, &WorldIndexY);
  return GetWorldCurrentTileMap(world, WorldIndexX, WorldIndexY);
}
inline void WorldToScreen(tile_world *world,
                          real32_t WorldX,
                          real32_t WorldY,
                          real32_t *ScreenX,
                          real32_t *ScreenY) {

  uint32_t MapWidth = world->TileMaps[0].TileWidth * world->TileMaps[0].CountX;
  uint32_t MapHeight =
      world->TileMaps[0].TileHeight * world->TileMaps[0].CountY;
  int32_t WorldIndexX;
  int32_t WorldIndexY;
  GetWorldIndex(world, WorldX, WorldY, &WorldIndexX, &WorldIndexY);

  if (WorldIndexX < 0 && WorldIndexY < 0) {
    *ScreenX = -1;
    *ScreenY = -1;
  };
  *ScreenX = WorldX - (int32_t)MapWidth * WorldIndexX;
  *ScreenY = WorldY - (int32_t)MapHeight * WorldIndexY;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert(&Input->Controllers[0].Terminator -
             &Input->Controllers[0].Bottons[0] ==
         (ArrayCount(Input->Controllers[0].Bottons)));
  Assert(sizeof(game_state) <= (uint64_t)Memory->PermanentStorageSize);
  game_state *GameState = (game_state *)Memory->PermanentStorage;
  if (!Memory->IsInitialized) {
    Memory->IsInitialized = true;
    GameState->PlayerWorldX = 150;
    GameState->PlayerWorldY = 120;
    GameState->CurrentPlayerWorldIndexX = 0;
    GameState->CurrentPlayerWorldIndexY = 0;
  }
#define TILE_ROW_SIZE 9
#define TILE_COLUMN_SIZE 17

  uint32_t Tile00[TILE_ROW_SIZE][TILE_COLUMN_SIZE] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0},
      {1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0},
      {1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0},
      {1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}};

  uint32_t Tile10[TILE_ROW_SIZE][TILE_COLUMN_SIZE] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
      {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
      {0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1}};

  uint32_t Tile01[TILE_ROW_SIZE][TILE_COLUMN_SIZE] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1},
      {1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

  uint32_t Tile11[TILE_ROW_SIZE][TILE_COLUMN_SIZE] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
      {0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
      {0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

  tile_map Map00{.StartX = -30,
                 .StartY = 0,
                 .CountX = TILE_COLUMN_SIZE,
                 .CountY = TILE_ROW_SIZE,
                 .TileHeight = 60,
                 .TileWidth = 60,
                 .TileMap = (uint32_t *)Tile00};

  tile_map Map10{.StartX = -30,
                 .StartY = 0,
                 .CountX = TILE_COLUMN_SIZE,
                 .CountY = TILE_ROW_SIZE,
                 .TileHeight = 60,
                 .TileWidth = 60,
                 .TileMap = (uint32_t *)Tile10};

  tile_map Map01{.StartX = -30,
                 .StartY = 0,
                 .CountX = TILE_COLUMN_SIZE,
                 .CountY = TILE_ROW_SIZE,
                 .TileHeight = 60,
                 .TileWidth = 60,
                 .TileMap = (uint32_t *)Tile01};

  tile_map Map11{.StartX = -30,
                 .StartY = 0,
                 .CountX = TILE_COLUMN_SIZE,
                 .CountY = TILE_ROW_SIZE,
                 .TileHeight = 60,
                 .TileWidth = 60,
                 .TileMap = (uint32_t *)Tile11};

  tile_map tile_maps[2][2]{
      {Map00, Map10},
      {Map01, Map11},
  };

  tile_world world{.TileMapCountX = 2,
                   .TileMapCountY = 2,
                   .TileMaps = (tile_map *)tile_maps};

  tile_map *CurrentMap =
      GetWorldCurrentTileMap(&world,
                             GameState->CurrentPlayerWorldIndexX,
                             GameState->CurrentPlayerWorldIndexY);

  real32_t PlayerWidth = 0.75f * CurrentMap->TileWidth;
  real32_t PlayerHeight = (real32_t)CurrentMap->TileHeight;
  for (int32_t ControllerIndex = 0; ControllerIndex < 5; ++ControllerIndex) {
    game_controller_input *Controller = GetController(Input, ControllerIndex);
    if (Controller->IsAnalog) {
    } else {
      real32_t PlayerDirX = 0;
      real32_t PlayerDirY = 0;
      real32_t speed = 200;
      if (Controller->MoveUp.EndedDown) { PlayerDirY -= 1; }
      if (Controller->MoveDown.EndedDown) { PlayerDirY += 1; }
      if (Controller->MoveLeft.EndedDown) { PlayerDirX -= 1; }
      if (Controller->MoveRight.EndedDown) { PlayerDirX += 1; }
      real32_t NewPlayerPositonX =
          GameState->PlayerWorldX + PlayerDirX * speed * Input->DeltaTime;
      real32_t NewPlayerPositonY =
          GameState->PlayerWorldY + PlayerDirY * speed * Input->DeltaTime;

      tile_map *NewMap =
          GetWorldPointTileMap(&world, NewPlayerPositonX, NewPlayerPositonY);

      real32_t PlayerScreenX;
      real32_t PlayerScreenY;
      WorldToScreen(&world,
                    NewPlayerPositonX,
                    NewPlayerPositonY,
                    &PlayerScreenX,
                    &PlayerScreenY);

      if (NewMap != NULL) { CurrentMap = NewMap; }

      bool32_t IsCenterEmpty =
          IsTileMapPointEmpty(CurrentMap, PlayerScreenX, PlayerScreenY);
      bool32_t IsLeftEmpty = IsTileMapPointEmpty(
          CurrentMap, PlayerScreenX + PlayerWidth / 2, PlayerScreenY);
      bool32_t IsRightEmpty = IsTileMapPointEmpty(
          CurrentMap, PlayerScreenX - PlayerWidth / 2, PlayerScreenY);

      if (!IsCenterEmpty || !IsLeftEmpty || !IsRightEmpty) continue;
      GameState->PlayerWorldX = NewPlayerPositonX;
      GameState->PlayerWorldY = NewPlayerPositonY;
    }
  }
  real32_t PlayerScreenX;
  real32_t PlayerScreenY;
  WorldToScreen(&world,
                GameState->PlayerWorldX,
                GameState->PlayerWorldY,
                &PlayerScreenX,
                &PlayerScreenY);

  real32_t PlayerMinX = PlayerScreenX - 0.5f * PlayerWidth;
  real32_t PlayerMinY = PlayerScreenY - PlayerHeight;

  DrawRectangle(Buffer,
                0,
                0,
                (real32_t)Buffer->Width,
                (real32_t)Buffer->Height,
                0.7f,
                0.8f,
                0.7f);

  Assert(CurrentMap != NULL);

  for (uint32_t Row = 0; Row < TILE_ROW_SIZE; ++Row) {
    real32_t MinY = real32_t(CurrentMap->StartY + Row * CurrentMap->TileHeight);
    real32_t MaxY =
        real32_t(CurrentMap->StartY + (Row + 1) * CurrentMap->TileHeight);
    for (uint32_t Column = 0; Column < TILE_COLUMN_SIZE; ++Column) {
      if (CurrentMap->TileMap[Row * CurrentMap->CountX + Column] != 1) continue;
      real32_t MinX =
          real32_t(CurrentMap->StartX + Column * CurrentMap->TileWidth);
      real32_t MaxX =
          real32_t(CurrentMap->StartX + (Column + 1) * CurrentMap->TileWidth);
      DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, 0.5f, 0.5f, 0.5f);
    }
  }

  DrawRectangle(Buffer,
                PlayerMinX,
                PlayerMinY,
                PlayerMinX + PlayerWidth,
                PlayerMinY + PlayerHeight,
                1,
                1,
                0);
};

extern "C" GAME_GET_SOUND_SAMPLE(GameGetSoundSample) {
  game_state *GameState = (game_state *)Memory->PermanentStorage;

  GameOutputSound(GameState, SoundBuffer);
}
