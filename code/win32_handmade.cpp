// windows.h fucking need to be included before xinput.h
// https://stackoverflow.com/questions/4845198/fatal-error-no-target-architecture-in-visual-studio
// There are a number of child header files that are automatically included with
// windows.h. Many of these files cannot simply be included by themselves (they
// are not self-contained), because of dependencies.
#include "handmade.cpp"

// clang-format off
#include <windows.h>
// clang-format on
#include "Constants.hpp"
#include "win32_handmade.hpp"
#include "xinput.h"
#include <dsound.h>
#include <malloc.h>

global_persist bool32_t GlobalRunning;
global_persist win32_offscreen_buffer GlobalBackbuffer;
global_persist LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
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

internal debug_read_file_result
DEBUGPlatformReadEntireFile(const char *FileName) {
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
          DEBUGPlatformFreeFile(Result.Contents);
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
internal void DEBUGPlatformFreeFile(void *Memory) {
  VirtualFree(Memory, 0, MEM_RELEASE);
};
internal bool32_t DEBUGPlatformWriteEntireFile(const char *FileName,
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
          .dwFlags = 0,
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
  NewState->EndedDown = IsDown;
  ++NewState->HalfTransition;
}
internal void Win32MessageLoop(game_controller_input *KeyboardController) {
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
        } else if (VkCode == 'S') {
        } else if (VkCode == 'A') {
        } else if (VkCode == 'D') {
        } else if (VkCode == 'Q') {
          Win32ProcessKeyboradMessage(&KeyboardController->LeftShoulder,
                                      IsDown);
        } else if (VkCode == 'E') {
          Win32ProcessKeyboradMessage(&KeyboardController->RightShoulder,
                                      IsDown);
        } else if (VkCode == VK_UP) {
          Win32ProcessKeyboradMessage(&KeyboardController->Up, IsDown);
        } else if (VkCode == VK_DOWN) {
          Win32ProcessKeyboradMessage(&KeyboardController->Down, IsDown);
        } else if (VkCode == VK_LEFT) {
          Win32ProcessKeyboradMessage(&KeyboardController->Left, IsDown);
        } else if (VkCode == VK_RIGHT) {
          Win32ProcessKeyboradMessage(&KeyboardController->Right, IsDown);
        } else if (VkCode == VK_ESCAPE) {
          GlobalRunning = false;
        } else if (VkCode == VK_SPACE) {
        }
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

int WINAPI WinMain(HINSTANCE Instance,
                   HINSTANCE PrevInstance,
                   LPSTR CommandLine,
                   int ShowCode) {
  LARGE_INTEGER PerfCountFrequencyResult;
  QueryPerformanceFrequency(&PerfCountFrequencyResult);
  // counts per second
  int64_t PerfCountFrequencyPerSecond = PerfCountFrequencyResult.QuadPart;

  Win32LoadXInput();
  WNDCLASS WindowsClass{};

  Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

  WindowsClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  // NOTE: Register Windows Message Handler.
  WindowsClass.lpfnWndProc = Win32MainWindowsCallback;
  WindowsClass.hInstance = Instance;
  WindowsClass.lpszClassName = "HandmadeHeroWindowsClass";

  if (RegisterClass(&WindowsClass)) {
    HWND Window = CreateWindowEx(0,
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

      // Because we are using CS_OWNDC, we don't need to call ReleaseDC.
      // means we don't share the device context with other windows.
      // also, we don't need to call ReleaseDC.
      HDC DeviceContext = GetDC(Window);

      win32_sound_output SoundOutput{
          .RunningSampleIndex = 0,
          .tSine = 0,
          .SamplePerSecond = 48000,
          .BytePerSample = sizeof(int16_t) * 2,
          // it is middle C 261.625565 Hz.
      };
      SoundOutput.SecondaryBufferSize =
          SoundOutput.SamplePerSecond * SoundOutput.BytePerSample;
      SoundOutput.LatencySampleCount = SoundOutput.SamplePerSecond / 15;

      Win32InitDSound(
          Window, SoundOutput.SamplePerSecond, SoundOutput.SecondaryBufferSize);
      Win32ClearBuffer(&SoundOutput);
      GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
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
          .PermanentStorage =
              VirtualAlloc(BaseAddress,
                           (size_t)(GameMemory.PermanentStorageSize +
                                    GameMemory.TransientStorageSize),
                           MEM_RESERVE | MEM_COMMIT,
                           PAGE_READWRITE),
          .TransientStorage = (uint8_t *)GameMemory.PermanentStorage +
                              GameMemory.PermanentStorageSize,
      };
      if (!GameMemory.PermanentStorage || !Samples ||
          !GameMemory.TransientStorage) {
        // TODO: Logging
        return -1;
      }

      LARGE_INTEGER LastCounter;
      QueryPerformanceCounter(&LastCounter);
      // the RDTSC is an instruction that provides cpu cycle count in
      // currenttime.
      uint64_t LastCyclesCount = __rdtsc();
      game_input Input[2]{};
      game_input *OldInput = &Input[0];
      game_input *NewInput = &Input[1];
      while (GlobalRunning) {

        game_controller_input *KeyboardController = &NewInput->Controllers[0];
        game_controller_input ZeroController{};
        *KeyboardController = ZeroController;
        Win32MessageLoop(KeyboardController);

        // NOTE: HandleWindowMessage, don't need to pass Window handle,
        // it will get all message form this process.

        DWORD MaxControllerCount = XUSER_MAX_COUNT;
        if (MaxControllerCount > ArrayCount(NewInput->Controllers)) {
          MaxControllerCount = ArrayCount(NewInput->Controllers);
        }
        for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount;
             ++ControllerIndex) {
          game_controller_input *OldController =
              &OldInput->Controllers[ControllerIndex];
          game_controller_input *NewController =
              &NewInput->Controllers[ControllerIndex];
          XINPUT_STATE ControllerState;
          if (XInputGetState(ControllerIndex, &ControllerState) ==
              ERROR_SUCCESS) {
            // ControllerState.dwPacketNumber used to detect if the controller
            // state changed, it will be incremented by 1 every time the state
            // is updated.
            // And We cant use this to check is the pulling speed is fast
            // enough
            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
            // bool32_t Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
            // bool32_t Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
            // bool32_t Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
            // bool32_t Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

            real32_t X = (Pad->sThumbLX < 0) ? (Pad->sThumbLX / 32768.0f)
                                             : (Pad->sThumbLX / 32767.0f);

            real32_t Y = (Pad->sThumbLY < 0) ? (Pad->sThumbLY / 32768.0f)
                                             : (Pad->sThumbLY / 32767.0f);
            NewController->IsAnalog = true;
            NewController->StartX = OldController->EndX;
            NewController->StartY = OldController->EndY;
            NewController->MinX = NewController->MaxX = NewController->EndX = X;
            NewController->MinY = NewController->MaxY = NewController->EndY = Y;

            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->Down,
                                            XINPUT_GAMEPAD_A,
                                            &NewController->Down);

            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->Right,
                                            XINPUT_GAMEPAD_B,
                                            &NewController->Right);

            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->Left,
                                            XINPUT_GAMEPAD_X,
                                            &NewController->Left);
            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->Up,
                                            XINPUT_GAMEPAD_Y,
                                            &NewController->Up);

            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->LeftShoulder,
                                            XINPUT_GAMEPAD_LEFT_SHOULDER,
                                            &NewController->LeftShoulder);
            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                            &OldController->RightShoulder,
                                            XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                            &NewController->RightShoulder);

            // bool32_t Start = Pad->wButtons & XINPUT_GAMEPAD_START;
            // bool32_t Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;

          } else {
            // NOTE: If ControllerIndex is not connected, we should skip it.
          }
        }

        DWORD BytesToWrite = 0;
        DWORD BytesToLock = 0;
        DWORD TargetCursor;
        DWORD PlayCursor;
        DWORD WriteCursor;
        bool32_t SoundIsValid = false;
        if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(
                &PlayCursor, &WriteCursor))) {
          BytesToLock =
              (SoundOutput.RunningSampleIndex * SoundOutput.BytePerSample) %
              SoundOutput.SecondaryBufferSize;
          TargetCursor = (PlayCursor + SoundOutput.LatencySampleCount *
                                           SoundOutput.BytePerSample) %
                         SoundOutput.SecondaryBufferSize;
          if (BytesToLock > TargetCursor) {
            BytesToWrite = SoundOutput.SecondaryBufferSize - BytesToLock;
            BytesToWrite += TargetCursor;
          } else {
            BytesToWrite = TargetCursor - BytesToLock;
          }
          SoundIsValid = true;
        }

        game_sound_output_buffer SoundBuffer{
            .SamplePerSecond = SoundOutput.SamplePerSecond,
            .SamplesCount = (int32_t)BytesToWrite / SoundOutput.BytePerSample,
            .Samples = Samples,
        };
        game_offscreen_buffer Buffer{
            .Memory = GlobalBackbuffer.Memory,
            .Width = GlobalBackbuffer.Width,
            .Height = GlobalBackbuffer.Height,
            .Pitch = GlobalBackbuffer.Pitch,
        };
        GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);

        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackbuffer,
                                   DeviceContext,
                                   Dimension.Width,
                                   Dimension.Height);
        if (SoundIsValid) {
          Win32FillSoundBuffer(
              &SoundOutput, BytesToLock, BytesToWrite, &SoundBuffer);
        }

        uint64_t EndCyclesCount = __rdtsc();
        LARGE_INTEGER EndCounter;
        QueryPerformanceCounter(&EndCounter);

        uint64_t CyclesElapsedPerFrame = EndCyclesCount - LastCyclesCount;
        int64_t CounterElapsedPerFrame =
            EndCounter.QuadPart - LastCounter.QuadPart;
        real32_t MSPerFrame =
            (1000.f * CounterElapsedPerFrame) / PerfCountFrequencyPerSecond;
        real32_t FPS =
            (real32_t)PerfCountFrequencyPerSecond / CounterElapsedPerFrame;
        real32_t MCPF = (real32_t)CyclesElapsedPerFrame / (1000 * 1000);

#if 0
        char Buffer[256];
        sprintf(
            Buffer, "%.02fms/f, %.02ff/s, %.02fmc/s\n", MSPerFrame, FPS, MCPF);
        OutputDebugString(Buffer);
#endif
        LastCounter = EndCounter;
        LastCyclesCount = EndCyclesCount;

        game_input *Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;
      }
    } else {
      // TODO: Logging when failed
    }
  } else {
    // TODO: Logging when failed
  }

  return 0;
}
