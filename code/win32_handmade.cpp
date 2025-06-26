
// https://stackoverflow.com/questions/4845198/fatal-error-no-target-architecture-in-visual-studio
// There are a number of child header files that are automatically included with
// windows.h. Many of these files cannot simply be included by themselves (they
// are not self-contained), because of dependencies.
#include "handmade.hpp"

// clang-format off
#include <cstdio>
#include <windows.h>
// clang-format on
#include "Constants.hpp"
#include "win32_handmade.hpp"
#include "xinput.h"
#include <dsound.h>

global_persist bool32_t GlobalRunning;
global_persist bool32_t GlobalPause;
global_persist win32_offscreen_buffer GlobalBackbuffer;
global_persist LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_persist int64_t GlobalPerfCountFrequencyPerSecond;

#define X_INPUT_GET_STATE(name)                                                \
  DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
// in windows api, 0 is success, but we want to return error.
X_INPUT_GET_STATE(XInputGetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }

#define X_INPUT_SET_STATE(name)                                                \
  DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)

typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }

#define DIRECT_SOUND_CREATE(name)                                              \
  HRESULT WINAPI name(                                                         \
      LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)

typedef DIRECT_SOUND_CREATE(direct_sound_create);

global_persist x_input_get_state *XInputGetState_ = XInputGetStateStub;
global_persist x_input_set_state *XInputSetState_ = XInputSetStateStub;

// Self loading XInputGetState and XInputSetState,
// because if we just link xinput.lib, for some one don't have xinput.h,
// it will fail when open game, but the game are note depend on gamepad.
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

void DEBUGPlatformFreeFileMemory(void *Memory) {
  VirtualFree(Memory, 0, MEM_RELEASE);
};
debug_read_file_result DEBUGPlatformReadEntireFile(const char *FileName) {
  debug_read_file_result Result{};
  HANDLE FileHandle = CreateFile(
      FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if (FileHandle != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER FileSize;
    if (GetFileSizeEx(FileHandle, &FileSize)) {

      uint32_t FileSize32 = SafeTruncateUInt32(FileSize.QuadPart);
      // NOTE: VirtualAlloc are used to big trunk of memort,
      // https://stackoverflow.com/questions/872072/whats-the-differences-between-virtualalloc-and-heapalloc
      //  HeapAlloc use VirtualAlloc to allocate Big trunk of memory.
      //  and manage sets up an internal data structure that can track further
      //  smaller size allocations within the reserved block of virtual memory.
      Result.Contents =
          VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      if (Result.Contents) {

        DWORD BytesRead;
        if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
            (BytesRead == FileSize32)) {
          Result.ContentSize = FileSize32;
        } else {
          // TODO: Logging
          DEBUGPlatformFreeFileMemory(Result.Contents);
          Result.Contents = 0;
        }
      } else {

        // TODO: Logging
      }
    } else {

      // TODO: Logging
    }
    CloseHandle(FileHandle);
  } else {
    // TODO: Logging
  }
  return Result;
};
bool32_t DEBUGPlatformWriteEntireFile(const char *FileName,
                                      uint32_t MemorySize,
                                      void *Memory) {
  bool32_t Result = false;
  HANDLE FileHandle =
      CreateFile(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if (FileHandle != INVALID_HANDLE_VALUE) {

    DWORD BytesWritten;
    if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0) &&
        (BytesWritten == MemorySize)) {
      Result = true;
    } else {
      // TODO: Logging
    }
    CloseHandle(FileHandle);
  } else {
    // TODO: Logging
  }
  return Result;
};

inline FILETIME Win32GetLastWriteTime(const char *FileName) {
  FILETIME Result{};
  WIN32_FIND_DATA FindData;
  HANDLE FileHandle = FindFirstFile(FileName, &FindData);
  if (FileHandle != INVALID_HANDLE_VALUE) {
    Result = FindData.ftLastWriteTime;
    FindClose(FileHandle);
  }
  return Result;
}

internal win32_game_code Win32LoadGameCode(const char *SourceDllName,
                                           const char *TempDllName) {
  win32_game_code Result{};
  Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDllName);
  CopyFile(SourceDllName, TempDllName, false);
  Result.GameCodeDLL = LoadLibrary(TempDllName);
  if (Result.GameCodeDLL) {
    Result.GameCode.UpdateAndRender = (game_update_and_render *)GetProcAddress(
        Result.GameCodeDLL, "GameUpdateAndRender");
    Result.GameCode.GetSoundSample = (game_get_sound_sample *)GetProcAddress(
        Result.GameCodeDLL, "GameGetSoundSample");
    Result.IsValid =
        Result.GameCode.UpdateAndRender && Result.GameCode.GetSoundSample;
  }
  if (!Result.IsValid) {
    Result.GameCode.UpdateAndRender = GameUpdateAndRenderStub;
    Result.GameCode.GetSoundSample = GameGetSoundSampleStub;
  }
  return Result;
}
internal void Win32UnloadGameCode(win32_game_code *GameCode) {
  if (GameCode->GameCodeDLL) { FreeLibrary(GameCode->GameCodeDLL); }
  GameCode->IsValid = false;
  GameCode->GameCode.UpdateAndRender = GameUpdateAndRenderStub;
  GameCode->GameCode.GetSoundSample = GameGetSoundSampleStub;
}

internal void Win32LoadXInput(void) {
  HMODULE XInputModule = LoadLibrary("xinput1_4.dll");
  if (!XInputModule) { XInputModule = LoadLibrary("xinput9_1_0.dll"); }
  if (!XInputModule) { XInputModule = LoadLibrary("xinput1_3.dll"); }
  // TODO: Logging XInput version

  if (XInputModule) {
    XInputGetState =
        (x_input_get_state *)GetProcAddress(XInputModule, "XInputGetState");
    if (!XInputGetState) { XInputGetState = XInputGetStateStub; }
    XInputSetState =
        (x_input_set_state *)GetProcAddress(XInputModule, "XInputSetState");
    if (!XInputSetState) { XInputGetState = XInputGetStateStub; }
  } else {
    // TODO: Logging
  }
}

internal void
Win32InitDSound(HWND Window, int32_t SamplePerSecond, int32_t BufferSize) {
  HMODULE DSoundModule = LoadLibrary("dsound.dll");
  if (DSoundModule) {
    direct_sound_create *DirectSoundCreate =
        (direct_sound_create *)GetProcAddress(DSoundModule,
                                              "DirectSoundCreate");
    // In some how, DirectSound are use oop, it write in C++
    LPDIRECTSOUND DirectSound;
    if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
      WAVEFORMATEX WaveFormat{
          .wFormatTag = WAVE_FORMAT_PCM,
          .nChannels = 2,
          .nSamplesPerSec = (DWORD)SamplePerSecond,
          .wBitsPerSample = 16,
      };
      WaveFormat.nBlockAlign =
          (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
      WaveFormat.nAvgBytesPerSec =
          WaveFormat.nBlockAlign * WaveFormat.nSamplesPerSec;

      if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
        DSBUFFERDESC BufferDescrption{
            .dwSize = sizeof(BufferDescrption),
            .dwFlags = DSBCAPS_PRIMARYBUFFER,
            .dwBufferBytes = 0,
        };

        // NOTE: In this place, PrimaryBuffer are only used to set the format.
        // it is the handle of audio Device.
        LPDIRECTSOUNDBUFFER PrimaryBuffer;
        if (SUCCEEDED(DirectSound->CreateSoundBuffer(
                &BufferDescrption, &PrimaryBuffer, 0))) {
          auto Error = PrimaryBuffer->SetFormat(&WaveFormat);
          if (SUCCEEDED(Error)) {
            OutputDebugString("Create PrimaryBuffer Success\n");
          } else {

            // TODO : Logging
          }
        } else {
          // TODO : Logging
        }

      } else {
        // TODO : Logging
      }
      DSBUFFERDESC BufferDescrption{
          .dwSize = sizeof(BufferDescrption),
          .dwFlags = DSBCAPS_GETCURRENTPOSITION2,
          .dwBufferBytes = (DWORD)BufferSize,
          .lpwfxFormat = &WaveFormat,
      };

      // NOTE: And this place, SecondaryBuffer are used to actually play audio.
      if (SUCCEEDED(DirectSound->CreateSoundBuffer(
              &BufferDescrption, &GlobalSecondaryBuffer, 0))) {
        OutputDebugString("Create SecondaryBuffer Success\n");
      } else {
        // TODO : Logging
      }

    } else {
      // TODO : Logging
    }
  }
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window) {
  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  LONG Width = ClientRect.right - ClientRect.left;
  LONG Height = ClientRect.bottom - ClientRect.top;
  return {Width, Height};
}

// DIB: Device Independent Bitmap
internal void
Win32ResizeDIBSection(win32_offscreen_buffer *buffer, int Width, int Height) {
  if (buffer->Memory) { VirtualFree(buffer->Memory, 0, MEM_RELEASE); }

  int BytePerPixel = 4;
  buffer->Width = Width;
  buffer->Height = Height;
  buffer->BytesPerPixel = BytePerPixel;

  buffer->Info.bmiHeader.biSize = sizeof(buffer->Info.bmiHeader);
  buffer->Info.bmiHeader.biWidth = buffer->Width;
  // if biHeight > 0 => bottom-up bitmap, origin from lower left corner.
  // if biHeight < 0 => top-down bitmap, origin from upper left corner.
  buffer->Info.bmiHeader.biHeight = -buffer->Height;
  buffer->Info.bmiHeader.biPlanes = 1;
  buffer->Info.bmiHeader.biBitCount = 32;
  buffer->Info.bmiHeader.biCompression = BI_RGB;

  int BitmapMemorySize = 4 * Width * Height;
  buffer->Memory = VirtualAlloc(
      0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

  buffer->Pitch = Width * BytePerPixel;
}

internal void Win32DisplayBufferInWindow(win32_offscreen_buffer *buffer,
                                         HDC DeviceContext,
                                         int WindowWidth,
                                         int WindowHeight) {
  // clang-format off
  StretchDIBits(DeviceContext,
                0, 0, WindowWidth, WindowHeight,
                0, 0, buffer->Width, buffer->Height,
                buffer->Memory,
                &buffer->Info,
                DIB_RGB_COLORS,
                SRCCOPY);
  // clang-format on
};

internal LRESULT WINAPI Win32MainWindowsCallback(HWND Window,
                                                 UINT Message,
                                                 WPARAM WParam,
                                                 LPARAM LParam) {
  LRESULT Result = 0;
  switch (Message) {
  case WM_SIZE: {
  } break;
  case WM_DESTROY: {
    GlobalRunning = false;
  } break;

  case WM_CLOSE: {
    GlobalRunning = false;
  } break;

  // NOTE: KeyboardInput.
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP: {
    Assert(!"Keyboard Input came in through a non-dispatch message");

  } break;

  case WM_ACTIVATEAPP: {

    if (WParam == TRUE) {
      SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
    } else {
      SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 128, LWA_ALPHA);
    }
  } break;
  case WM_PAINT: {
    PAINTSTRUCT Paint;
    HDC DeviceContext = BeginPaint(Window, &Paint);
    // Paint.rcPaint is the area that needs to be repainted.(dirty area)
    // but we always want to repaint the whole window.
    // LONG X = Paint.rcPaint.left;
    // LONG Y = Paint.rcPaint.top;
    // LONG Width = Paint.rcPaint.right - Paint.rcPaint.left;
    // LONG Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
    Win32DisplayBufferInWindow(
        &GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
    EndPaint(Window, &Paint);
  } break;
  default: {
    Result = DefWindowProc(Window, Message, WParam, LParam);
  } break;
  }
  return Result;
}

internal void Win32ClearBuffer(win32_sound_output *SoundOutput) {

  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;
  if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0,
                                            SoundOutput->SecondaryBufferSize,
                                            &Region1,
                                            &Region1Size,
                                            &Region2,
                                            &Region2Size,
                                            0))) {
    int8_t *DestSample = (int8_t *)Region1;
    for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ByteIndex++) {
      *DestSample++ = 0;
    }

    DestSample = (int8_t *)Region2;
    for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ByteIndex++) {
      *DestSample++ = 0;
    }
    GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  }
}

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput,
                                   DWORD BytesToLock,
                                   DWORD BytesToWrite,
                                   game_sound_output_buffer *SourceBuffer) {
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  auto Error = GlobalSecondaryBuffer->Lock(BytesToLock,
                                           BytesToWrite,
                                           &Region1,
                                           &Region1Size,
                                           &Region2,
                                           &Region2Size,
                                           0);
  if (SUCCEEDED(Error)) {
    DWORD Region1SampleCount = Region1Size / SoundOutput->BytePerSample;
    int16_t *DestSample = (int16_t *)Region1;
    int16_t *SourceSample = SourceBuffer->Samples;
    for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount;
         SampleIndex++) {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      ++SoundOutput->RunningSampleIndex;
    }

    DestSample = (int16_t *)Region2;
    DWORD Region2SampleCount = Region2Size / SoundOutput->BytePerSample;
    for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount;
         SampleIndex++) {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      ++SoundOutput->RunningSampleIndex;
    }
    GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  };
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputBottonState,
                                              game_botton_state *OldState,
                                              DWORD BottonBit,
                                              game_botton_state *NewState) {
  NewState->EndedDown = ((XInputBottonState & BottonBit) == BottonBit);
  NewState->HalfTransition =
      (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void Win32ProcessKeyboradMessage(game_botton_state *NewState,
                                          bool32_t IsDown) {
  Assert(NewState->EndedDown != IsDown);
  NewState->EndedDown = IsDown;
  ++NewState->HalfTransition;
}

internal void Win32BeginRecordingInput(win32_state *Win32State,
                                       int32_t InputRecordingIndex) {
  Win32State->InputRecordingIndex = InputRecordingIndex;
  const char *Filename = "handmade.hmr";
  Win32State->RecordingHandle =
      CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

  DWORD ByteToWrite = (DWORD)Win32State->TotalSize;
  Assert(Win32State->TotalSize == ByteToWrite);
  DWORD BytesWritten;
  WriteFile(Win32State->RecordingHandle,
            Win32State->GameMemoryBlock,
            ByteToWrite,
            &BytesWritten,
            0);
}

internal void Win32EndRecordingInput(win32_state *Win32State) {
  Win32State->InputRecordingIndex = 0;
  CloseHandle(Win32State->RecordingHandle);
}

internal void Win32BeginPlaybackInput(win32_state *Win32State,
                                      int32_t InputPlaybackIndex) {
  Win32State->InputPlayingIndex = InputPlaybackIndex;
  const char *Filename = "handmade.hmr";
  Win32State->PlaybackHandle = CreateFileA(
      Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  DWORD ByteToRead = (DWORD)Win32State->TotalSize;
  Assert(Win32State->TotalSize == ByteToRead);
  DWORD BytesRead;
  ReadFile(Win32State->PlaybackHandle,
           Win32State->GameMemoryBlock,
           ByteToRead,
           &BytesRead,
           0);
}

internal void Win32EndPlaybackInput(win32_state *Win32State) {
  Win32State->InputPlayingIndex = 0;
  CloseHandle(Win32State->PlaybackHandle);
}

internal void Win32RecordInput(win32_state *Win32State, game_input *NewInput) {
  DWORD BytesWritten;
  // NOTE: because we use same handle, so it behavior like Append the file.
  WriteFile(Win32State->RecordingHandle,
            NewInput,
            sizeof(game_input),
            &BytesWritten,
            0);
}

internal void Win32PlaybackInput(win32_state *Win32State,
                                 game_input *NewInput) {
  DWORD BytesRead;

  // NOTE: because we use same handle, so it behavior like read game_input size
  // every frame.
  if (ReadFile(Win32State->PlaybackHandle,
               NewInput,
               sizeof(game_input),
               &BytesRead,
               0)) {
    if (BytesRead == 0) {
      int32_t PlaybackIndex = Win32State->InputPlayingIndex;
      Win32EndPlaybackInput(Win32State);
      Win32BeginPlaybackInput(Win32State, PlaybackIndex);
      ReadFile(Win32State->PlaybackHandle,
               NewInput,
               sizeof(game_input),
               &BytesRead,
               0);
    }
  }
}

internal void
Win32ProcessPendingMessageLoop(win32_state *Win32State,
                               game_controller_input *KeyboardController) {
  MSG Message;
  while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
    switch (Message.message) {

    case WM_QUIT: {
      GlobalRunning = false;
      break;
    }
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
      uint32_t VkCode = (uint32_t)Message.wParam;

      WORD KeyFlags = HIWORD(Message.lParam);

      // https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#keystroke-message-flags
      // previous key state flag.
      const bool32_t WasDown = ((KeyFlags & KF_REPEAT) == KF_REPEAT);

      // transition state flag.
      const bool32_t IsDown = ((KeyFlags & KF_UP) == 0);
      // We use bool32_t to avoid warning.
      // because we don't need to compare with other, so we dont need to
      // convert it to bool. we only need to know if it 0 or not.
      const bool32_t AltDown = (KeyFlags & KF_ALTDOWN);
      if (WasDown != IsDown) { // Botton Down
        if (VkCode == 'W') {
          Win32ProcessKeyboradMessage(&KeyboardController->MoveUp, IsDown);
        } else if (VkCode == 'S') {
          Win32ProcessKeyboradMessage(&KeyboardController->MoveDown, IsDown);
        } else if (VkCode == 'A') {
          Win32ProcessKeyboradMessage(&KeyboardController->MoveLeft, IsDown);
        } else if (VkCode == 'D') {
          Win32ProcessKeyboradMessage(&KeyboardController->MoveRight, IsDown);
        } else if (VkCode == 'Q') {
          Win32ProcessKeyboradMessage(&KeyboardController->LeftShoulder,
                                      IsDown);
        } else if (VkCode == 'E') {
          Win32ProcessKeyboradMessage(&KeyboardController->RightShoulder,
                                      IsDown);
        } else if (VkCode == 'K') {
          Win32ProcessKeyboradMessage(&KeyboardController->ActionUp, IsDown);
        } else if (VkCode == 'J') {
          Win32ProcessKeyboradMessage(&KeyboardController->ActionDown, IsDown);
        } else if (VkCode == 'H') {
          Win32ProcessKeyboradMessage(&KeyboardController->ActionLeft, IsDown);
        } else if (VkCode == 'L') {
          Win32ProcessKeyboradMessage(&KeyboardController->ActionRight, IsDown);
        } else if (VkCode == VK_ESCAPE) {
          Win32ProcessKeyboradMessage(&KeyboardController->Start, IsDown);
        } else if (VkCode == VK_SPACE) {
          Win32ProcessKeyboradMessage(&KeyboardController->Back, IsDown);
        }
#if HANDMADE_INTERNAL
        else if (VkCode == 'P') {
          if (IsDown) { GlobalPause = !GlobalPause; }
        } else if (VkCode == 'R') {
          if (IsDown) {
            if (Win32State->InputRecordingIndex == 0) {
              Win32BeginRecordingInput(Win32State, 1);
            } else {
              Win32EndRecordingInput(Win32State);
              Win32BeginPlaybackInput(Win32State, 1);
            }
          }
        }
#endif
      }
      if (AltDown && VkCode == VK_F4) { GlobalRunning = false; }

      break;
    }
    default: {
      TranslateMessage(&Message);
      // NOTE: It will dispatch to Upper Registered Windows Message
      // Handler.
      DispatchMessage(&Message);
      break;
    }
    }
  }
}
internal real32_t Win32ProcessXinputStickValue(SHORT Value, SHORT DeadZone) {
  real32_t Result = 0;
  if (Value > DeadZone) {
    Result = (Value - DeadZone) / (32767.0f - DeadZone);
  } else if (Value < -DeadZone) {
    Result = (Value + DeadZone) / (32767.0f - DeadZone);
  }
  return Result;
}

internal void Win32DebugDrawLine(win32_offscreen_buffer *Buffer,
                                 int32_t X,
                                 int32_t Top,
                                 int32_t Bottom,
                                 uint32_t Color) {
  if (Top <= 0) { Top = 0; }
  if (Bottom > Buffer->Height) { Bottom = Buffer->Height; }

  if (X >= 0 && X < Buffer->Width) {

    uint8_t *Pixels = (uint8_t *)Buffer->Memory + X * Buffer->BytesPerPixel +
                      Buffer->Pitch * Top;
    for (int32_t Y = Top; Y < Bottom; ++Y) {
      *(uint32_t *)Pixels = Color;
      Pixels += Buffer->Pitch;
    }
  }
}
inline void Win32DrawSoundBufferMarker(win32_offscreen_buffer *Buffer,
                                       win32_sound_output *SoundOutput,
                                       real32_t C,
                                       int32_t PadX,
                                       int Top,
                                       int Bottom,
                                       DWORD Value,
                                       uint32_t Color) {

  real32_t CurrentMarkerX = C * Value;
  int32_t X = PadX + (int32_t)CurrentMarkerX;
  Win32DebugDrawLine(Buffer, X, Top, Bottom, Color);
}

internal void Win32DebugSyncDisplayBuffer(win32_offscreen_buffer *Buffer,
                                          int32_t MarkerCount,
                                          win32_debug_time_marker *Markers,
                                          int32_t CurrentMarker,
                                          win32_sound_output *SoundOutput,
                                          real32_t TargetSecondsPerFrame) {
  int32_t PadX = 16;
  int32_t PadY = 16;
  int32_t LineHeight = 64;

  real32_t C =
      (real32_t)(Buffer->Width - 2 * PadX) / SoundOutput->SecondaryBufferSize;
  for (int32_t PlayCursorIndex = 1; PlayCursorIndex < MarkerCount;
       ++PlayCursorIndex) {
    DWORD PlayColor = 0xFFFFFFFF;
    DWORD WriteColor = 0xFFFF0000;
    DWORD ExpectedFlipColor = 0xFFFFFF00;
    int32_t Top = PadY;
    int32_t Bottom = PadY + LineHeight;
    win32_debug_time_marker *ThisMarker = &Markers[PlayCursorIndex];
    Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);
    if (PlayCursorIndex == CurrentMarker) {
      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;
      int32_t FirstTop = Top;
      Win32DrawSoundBufferMarker(Buffer,
                                 SoundOutput,
                                 C,
                                 PadX,
                                 Top,
                                 Bottom,
                                 ThisMarker->OutputPlayCursor,
                                 PlayColor);

      Win32DrawSoundBufferMarker(Buffer,
                                 SoundOutput,
                                 C,
                                 PadX,
                                 Top,
                                 Bottom,
                                 ThisMarker->OutputWriteCursor,
                                 WriteColor);

      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;
      Win32DrawSoundBufferMarker(Buffer,
                                 SoundOutput,
                                 C,
                                 PadX,
                                 Top,
                                 Bottom,
                                 ThisMarker->OutputLocation,
                                 PlayColor);
      Win32DrawSoundBufferMarker(Buffer,
                                 SoundOutput,
                                 C,
                                 PadX,
                                 Top,
                                 Bottom,
                                 ThisMarker->OutputLocation +
                                     ThisMarker->OutputByteCount,
                                 WriteColor);

      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;
      Win32DrawSoundBufferMarker(Buffer,
                                 SoundOutput,
                                 C,
                                 PadX,
                                 FirstTop,
                                 Bottom,
                                 ThisMarker->ExpectedFlipPlayCursor,
                                 ExpectedFlipColor);
    }

    Win32DrawSoundBufferMarker(Buffer,
                               SoundOutput,
                               C,
                               PadX,
                               Top,
                               Bottom,
                               ThisMarker->FlipPlayCursor,
                               PlayColor);
    Win32DrawSoundBufferMarker(Buffer,
                               SoundOutput,
                               C,
                               PadX,
                               Top,
                               Bottom,
                               ThisMarker->FlipWriteCursor,
                               WriteColor);
  }
}

inline LARGE_INTEGER Win32GetWallClock() {
  LARGE_INTEGER Result;
  QueryPerformanceCounter(&Result);
  return Result;
}

inline real32_t Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
  return (real32_t)(End.QuadPart - Start.QuadPart) /
         GlobalPerfCountFrequencyPerSecond;
}

void CatString(size_t SourceACount,
               const char *SourceA,
               size_t SourceBCount,
               const char *SourceB,
               size_t DestCount,
               char *Dest) {
  Assert(SourceACount + SourceBCount <= DestCount + 1);
  for (size_t Index = 0; Index < DestCount; ++Index) {
    if (Index < SourceACount) {
      Dest[Index] = SourceA[Index];
    } else if (Index < SourceACount + SourceBCount) {
      Dest[Index] = SourceB[Index - SourceACount];
    } else {
      Dest[Index] = 0;
      return;
    }
  }
}

int WINAPI WinMain(HINSTANCE Instance,
                   HINSTANCE PrevInstance,
                   LPSTR CommandLine,
                   int ShowCode) {
  // NOTE: the MAX_PATH are not actuall MaxPath anymore,
  //       should use other way to ship in the actual code.
  char EXEFileName[MAX_PATH];
  DWORD SizeOfFileName = GetModuleFileNameA(0, EXEFileName, MAX_PATH);
  char *OnePastLastSlash = EXEFileName;
  for (char *Current = EXEFileName; *Current; ++Current) {
    if (*Current == '\\') { OnePastLastSlash = Current + 1; }
  }
  char SourceGameCodeDLLFilename[] = "handmade.dll";
  char SourceGameCodeDLLFullPath[31];
  CatString(OnePastLastSlash - EXEFileName, // 18 this not include '\0'
            EXEFileName,
            sizeof(SourceGameCodeDLLFilename), // 12 but it include '\0'
            SourceGameCodeDLLFilename,
            sizeof(SourceGameCodeDLLFullPath),
            SourceGameCodeDLLFullPath);

  char TempGameCodeDLLFilename[] = "handmade_temp.dll";
  char TempGameCodeDLLFullPath[MAX_PATH];
  CatString(OnePastLastSlash - EXEFileName,
            EXEFileName,
            sizeof(TempGameCodeDLLFilename),
            TempGameCodeDLLFilename,
            sizeof(TempGameCodeDLLFullPath),
            TempGameCodeDLLFullPath);

  LARGE_INTEGER PerfCountFrequencyResult;
  QueryPerformanceFrequency(&PerfCountFrequencyResult);
  GlobalPerfCountFrequencyPerSecond = PerfCountFrequencyResult.QuadPart;

  UINT DesiredSchedulerMS = 1;
  bool32_t SleepIsGranular =
      timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR;

  Win32LoadXInput();
  WNDCLASS WindowsClass{};

  Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

  WindowsClass.style = CS_HREDRAW | CS_VREDRAW;
  // NOTE: Register Windows Message Handler.
  WindowsClass.lpfnWndProc = Win32MainWindowsCallback;
  WindowsClass.hInstance = Instance;
  WindowsClass.lpszClassName = "HandmadeHeroWindowsClass";

#define MonitorRefreshRate 60
#define GameUpdateHz                                                           \
  ((int32_t)(MonitorRefreshRate / 2.f)) // Currently use software rendering.
  real32_t TargetSecondsPerFrame = 1.f / GameUpdateHz;
  if (RegisterClass(&WindowsClass)) {
    HWND Window = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED,
                                 WindowsClass.lpszClassName,
                                 "Handmade Hero",
                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 0,
                                 0,
                                 Instance,
                                 0);
    if (Window) {

      win32_sound_output SoundOutput{
          .RunningSampleIndex = 0,
          .tSine = 0,
          .SamplePerSecond = 48000,
          .BytePerSample = sizeof(int16_t) * 2,
      };
      SoundOutput.SecondaryBufferSize =
          SoundOutput.SamplePerSecond * SoundOutput.BytePerSample;
      SoundOutput.LatencySampleCount =
          (int32_t)(5.f *
                    ((real32_t)SoundOutput.SamplePerSecond / GameUpdateHz));
      SoundOutput.SafetyBytes = (SoundOutput.SamplePerSecond *
                                 SoundOutput.BytePerSample / GameUpdateHz) /
                                2;

      Win32InitDSound(
          Window, SoundOutput.SamplePerSecond, SoundOutput.SecondaryBufferSize);
      Win32ClearBuffer(&SoundOutput);
      auto error = GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      win32_state Win32State{};
      GlobalRunning = true;

      int16_t *Samples =
          (int16_t *)VirtualAlloc(0,
                                  SoundOutput.SecondaryBufferSize,
                                  MEM_RESERVE | MEM_COMMIT,
                                  PAGE_READWRITE);
#if HANDMADE_INTERNAL
      LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
      LPVOID BaseAddress = 0;
#endif

      game_memory GameMemory{
          .PermanentStorageSize = Megabytes(64),
          .TransientStorageSize = Gigabytes(1),
          .DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile,
          .DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory,
          .DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile,
      };
      Win32State.TotalSize =
          GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
      Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress,
                                                Win32State.TotalSize,
                                                MEM_RESERVE | MEM_COMMIT,
                                                PAGE_READWRITE);
      GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
      GameMemory.TransientStorage = (uint8_t *)GameMemory.PermanentStorage +
                                    GameMemory.PermanentStorageSize;

      if (!GameMemory.PermanentStorage || !Samples ||
          !GameMemory.TransientStorage) {
        // TODO: Logging
        return -1;
      }

      game_input Input[2]{};
      game_input *OldInput = &Input[0];
      game_input *NewInput = &Input[1];

      LARGE_INTEGER LastCounter = Win32GetWallClock();
      LARGE_INTEGER FlipWallClock = Win32GetWallClock();

      int32_t DebugTimeMarkerIndex = 0;
      win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2]{};

      DWORD AudioLatencyByte = 0;
      real32_t AudioLatencySecond = 0;
      bool32_t SoundIsValid = false;
      // the RDTSC is an instruction that provides cpu cycle count in
      // currenttime.

      win32_game_code Game =
          Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);

      uint64_t LastCyclesCount = __rdtsc();
      while (GlobalRunning) {

        FILETIME NEWDLLWriteTime =
            Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
        if (CompareFileTime(&NEWDLLWriteTime, &Game.DLLLastWriteTime) != 0) {
          Win32UnloadGameCode(&Game);
          Game = Win32LoadGameCode(SourceGameCodeDLLFullPath,
                                   TempGameCodeDLLFullPath);
        }
        game_controller_input *OldKeyboardController =
            GetController(OldInput, 0);
        game_controller_input *NewKeyboardController =
            GetController(NewInput, 0);
        *NewKeyboardController = {};
        NewKeyboardController->IsConnected = true;
        for (size_t BottonIndex = 0;
             BottonIndex < ArrayCount(NewKeyboardController->Bottons);
             ++BottonIndex) {
          NewKeyboardController->Bottons[BottonIndex].EndedDown =
              OldKeyboardController->Bottons[BottonIndex].EndedDown;
        }

        Win32ProcessPendingMessageLoop(&Win32State, NewKeyboardController);

        // NOTE: HandleWindowMessage, don't need to pass Window handle,
        // it will get all message form this process.

        DWORD MaxControllerCount = XUSER_MAX_COUNT;
        if (MaxControllerCount > ArrayCount(NewInput->Controllers) - 1) {
          MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
        }
        for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount;
             ++ControllerIndex) {
          DWORD MapedControllerIndex = ControllerIndex + 1;
          game_controller_input *OldController =
              GetController(OldInput, MapedControllerIndex);
          game_controller_input *NewController =
              GetController(NewInput, MapedControllerIndex);
          XINPUT_STATE ControllerState;
          if (XInputGetState(ControllerIndex, &ControllerState) ==
              ERROR_SUCCESS) {
            // ControllerState.dwPacketNumber used to detect if the controller
            // state changed, it will be incremented by 1 every time the state
            // is updated.
            // And We cant use this to check is the pulling speed is fast
            // enough
            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
            bool32_t DpadUp = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
            bool32_t DpadDown = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
            bool32_t DpadLeft = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
            bool32_t DpadRight = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

            NewController->IsAnalog = true;
            NewController->IsConnected = true;
            NewController->StickAverageX = Win32ProcessXinputStickValue(
                Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
            NewController->StickAverageY = Win32ProcessXinputStickValue(
                Pad->sThumbLY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
            if (NewController->StickAverageX != 0.0f ||
                NewController->StickAverageY != 0.0f) {
              NewController->IsAnalog = true;
            }
            if (DpadUp) {
              NewController->StickAverageY += 1.0f;
              NewController->IsAnalog = false;
            }
            if (DpadDown) {
              NewController->StickAverageY -= 1.0f;
              NewController->IsAnalog = false;
            }
            if (DpadLeft) {
              NewController->StickAverageX -= 1.0f;
              NewController->IsAnalog = false;
            }
            if (DpadRight) {
              NewController->StickAverageX += 1.0f;
              NewController->IsAnalog = false;
            }

            real32_t Threshold = 0.5f;
            Win32ProcessXInputDigitalButton(
                (NewController->StickAverageY < -Threshold) ? 1 : 0,
                &OldController->MoveDown,
                XINPUT_GAMEPAD_A,
                &NewController->MoveDown);

            Win32ProcessXInputDigitalButton(
                (NewController->StickAverageX > Threshold) ? 1 : 0,
                &OldController->MoveRight,
                XINPUT_GAMEPAD_B,
                &NewController->MoveRight);

            Win32ProcessXInputDigitalButton(
                (NewController->StickAverageX < -Threshold) ? 1 : 0,
                &OldController->MoveLeft,
                XINPUT_GAMEPAD_X,
                &NewController->MoveLeft);
            Win32ProcessXInputDigitalButton(
                (NewController->StickAverageY > Threshold) ? 1 : 0,
                &OldController->MoveUp,
                XINPUT_GAMEPAD_Y,
                &NewController->MoveUp);

            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->ActionDown,
                                            XINPUT_GAMEPAD_A,
                                            &NewController->ActionDown);

            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->ActionRight,
                                            XINPUT_GAMEPAD_B,
                                            &NewController->ActionRight);

            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->ActionLeft,
                                            XINPUT_GAMEPAD_X,
                                            &NewController->ActionLeft);
            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->ActionUp,
                                            XINPUT_GAMEPAD_Y,
                                            &NewController->ActionUp);

            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->LeftShoulder,
                                            XINPUT_GAMEPAD_LEFT_SHOULDER,
                                            &NewController->LeftShoulder);
            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->RightShoulder,
                                            XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                            &NewController->RightShoulder);

            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->Start,
                                            XINPUT_GAMEPAD_START,
                                            &NewController->Start);
            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->Back,
                                            XINPUT_GAMEPAD_BACK,
                                            &NewController->Back);

          } else {
            // NOTE: If ControllerIndex is not connected, we should skip it.
            NewController->IsConnected = false;
          }
        }

        if (GlobalPause) { continue; }
        game_offscreen_buffer Buffer{
            .Memory = GlobalBackbuffer.Memory,
            .Width = GlobalBackbuffer.Width,
            .Height = GlobalBackbuffer.Height,
            .Pitch = GlobalBackbuffer.Pitch,
            .BytesPerPixel = GlobalBackbuffer.BytesPerPixel,
        };
        if (Win32State.InputRecordingIndex) {
          Win32RecordInput(&Win32State, NewInput);
        }
        if (Win32State.InputPlayingIndex) {
          Win32PlaybackInput(&Win32State, NewInput);
        }
        Game.GameCode.UpdateAndRender(&GameMemory, NewInput, &Buffer);

        DWORD PlayCursor;
        DWORD WriteCursor;
        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
        real32_t FromBeginToAudioSeconds =
            Win32GetSecondsElapsed(LastCounter, AudioWallClock) * 1000;
        if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor,
                                                      &WriteCursor) == DS_OK) {

          if (!SoundIsValid) {
            SoundOutput.RunningSampleIndex =
                WriteCursor / SoundOutput.BytePerSample;
            SoundIsValid = true;
          }
          DWORD BytesToLock =
              (SoundOutput.RunningSampleIndex * SoundOutput.BytePerSample) %
              SoundOutput.SecondaryBufferSize;

          DWORD ExpectedSoundBytePerFrame =
              (SoundOutput.SamplePerSecond * SoundOutput.BytePerSample) /
              GameUpdateHz;
          DWORD SecondLeftUntilFlip =
              (DWORD)(TargetSecondsPerFrame - FromBeginToAudioSeconds);
          DWORD ExpectedSoundByteUntilFlip =
              (DWORD)((SecondLeftUntilFlip / TargetSecondsPerFrame) *
                      ExpectedSoundBytePerFrame);

          DWORD ExpectedFrameBoundaryByte =
              PlayCursor + ExpectedSoundBytePerFrame;

          DWORD SafeWriteCursor = WriteCursor;
          if (SafeWriteCursor < PlayCursor) {
            SafeWriteCursor += SoundOutput.SecondaryBufferSize;
          }
          SafeWriteCursor += SoundOutput.SafetyBytes;

          bool32_t IsAudioLatency =
              (SafeWriteCursor >= ExpectedFrameBoundaryByte);

          DWORD TargetCursor;
          if (IsAudioLatency) {
            TargetCursor = (WriteCursor + ExpectedSoundBytePerFrame +
                            SoundOutput.SafetyBytes);
          } else {
            TargetCursor =
                ExpectedFrameBoundaryByte + ExpectedSoundBytePerFrame;
          }
          TargetCursor %= SoundOutput.SecondaryBufferSize;

          DWORD BytesToWrite = 0;
          if (BytesToLock > TargetCursor) {
            BytesToWrite = SoundOutput.SecondaryBufferSize - BytesToLock;
            BytesToWrite += TargetCursor;
          } else {
            BytesToWrite = TargetCursor - BytesToLock;
          }
          game_sound_output_buffer SoundBuffer{
              .SamplePerSecond = SoundOutput.SamplePerSecond,
              .SamplesCount = (int32_t)BytesToWrite / SoundOutput.BytePerSample,
              .Samples = Samples,
          };
          Game.GameCode.GetSoundSample(&GameMemory, &SoundBuffer);

#if HANDMADE_INTERNAL

          win32_debug_time_marker *CurrentMarker =
              &DebugTimeMarkers[DebugTimeMarkerIndex];
          CurrentMarker->OutputPlayCursor = PlayCursor;
          CurrentMarker->OutputWriteCursor = WriteCursor;
          CurrentMarker->OutputLocation = BytesToLock;
          CurrentMarker->OutputByteCount = BytesToWrite;
          CurrentMarker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

          AudioLatencyByte =
              ((WriteCursor - PlayCursor) + SoundOutput.SecondaryBufferSize) %
              SoundOutput.SecondaryBufferSize;
          AudioLatencySecond = (real32_t)AudioLatencyByte /
                               SoundOutput.BytePerSample /
                               SoundOutput.SamplePerSecond;
          char TextBuffer[256];

          sprintf_s(TextBuffer,
                    "BTL:%u, TC:%u, BTW:%u BTC:%u, (%f)\n",
                    BytesToLock,
                    TargetCursor,
                    BytesToWrite,
                    AudioLatencyByte,
                    AudioLatencySecond);
          OutputDebugString(TextBuffer);
#endif
          Win32FillSoundBuffer(
              &SoundOutput, BytesToLock, BytesToWrite, &SoundBuffer);
        } else {
          SoundIsValid = false;
        }

        LARGE_INTEGER WorkCounter = Win32GetWallClock();
        real32_t SecondsElapsed =
            Win32GetSecondsElapsed(LastCounter, WorkCounter);

        if (SecondsElapsed < TargetSecondsPerFrame) {
          if (SleepIsGranular) {
            DWORD SleepMs =
                (DWORD)(TargetSecondsPerFrame - SecondsElapsed) * 1000;
            Sleep(SleepMs);
          }

          if (Win32GetSecondsElapsed(LastCounter, Win32GetWallClock()) <
              TargetSecondsPerFrame) {
            // NOTE: Logging Missed sleep
          }
          while (SecondsElapsed < TargetSecondsPerFrame) {
            SecondsElapsed =
                Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
          }
        } else {
          // NOTE: MISSED_FRAMES!!
        }

        LARGE_INTEGER EndCounter = Win32GetWallClock();
        real32_t MSPerFrame =
            Win32GetSecondsElapsed(LastCounter, EndCounter) * 1000;
        LastCounter = EndCounter;

        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if HANDMADE_INTERNAL
        Win32DebugSyncDisplayBuffer(&GlobalBackbuffer,
                                    ArrayCount(DebugTimeMarkers),
                                    DebugTimeMarkers,
                                    DebugTimeMarkerIndex - 1,
                                    &SoundOutput,
                                    TargetSecondsPerFrame);
#endif

        HDC DeviceContext = GetDC(Window);
        Win32DisplayBufferInWindow(&GlobalBackbuffer,
                                   DeviceContext,
                                   Dimension.Width,
                                   Dimension.Height);
        ReleaseDC(Window, DeviceContext);
        FlipWallClock = Win32GetWallClock();
#if HANDMADE_INTERNAL
        {
          DWORD FlipPlayCursor;
          DWORD FlipWriteCursor;
          if (GlobalSecondaryBuffer->GetCurrentPosition(
                  &FlipPlayCursor, &FlipWriteCursor) == DS_OK) {
            win32_debug_time_marker *CurrentMarker =
                &DebugTimeMarkers[DebugTimeMarkerIndex];
            CurrentMarker->FlipPlayCursor = FlipPlayCursor;
            CurrentMarker->FlipWriteCursor = FlipWriteCursor;
          }
        }
#endif
        game_input *Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;

        uint64_t EndCyclesCount = __rdtsc();
        uint64_t CyclesElapsedPerFrame = EndCyclesCount - LastCyclesCount;
        LastCyclesCount = EndCyclesCount;

        real32_t FPS = 0.0;
        real32_t MCPF = (real32_t)CyclesElapsedPerFrame / (1000 * 1000);
        char FPSBuffer[256];
        sprintf_s(FPSBuffer,
                  "%.02fms/f, %.02ff/s, %.02fmc/s\n",
                  FromBeginToAudioSeconds,
                  FPS,
                  MCPF);
        OutputDebugString(FPSBuffer);

#if HANDMADE_INTERNAL
        ++DebugTimeMarkerIndex;
        DebugTimeMarkerIndex %= ArrayCount(DebugTimeMarkers);
#endif
      }
    } else {
      // TODO: Logging when failed
    }
  } else {
    // TODO: Logging when failed
  }

  return 0;
}
