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
#include "xinput.h"
#include <dsound.h>
#include <malloc.h>

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
global_persist bool32_t GlobalRunning;
global_persist win32_offscreen_buffer GlobalBackbuffer;
global_persist LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// Self loading XInputGetState and XInputSetState,
// because if we just link xinput.lib, for some one don't have xinput.h,
// it will fail when open game, but the game are note depend on gamepad.
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

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
    uint32_t VkCode = WParam;

    WORD KeyFlags = HIWORD(LParam);

    // https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#keystroke-message-flags
    // previous key state flag.
    const bool32_t WasDown = ((KeyFlags & KF_REPEAT) == KF_REPEAT);

    // transition state flag.
    const bool32_t IsDown = ((KeyFlags & KF_UP) == 0);
    // We use bool32_t to avoid warning.
    // because we don't need to compare with other, so we dont need to convert
    // it to bool.
    // we only need to know if it 0 or not.
    const bool32_t AltDown = (KeyFlags & KF_ALTDOWN);
    if (WasDown != IsDown) {
      if (VkCode == 'W') {
      } else if (VkCode == 'S') {
      } else if (VkCode == 'A') {
      } else if (VkCode == 'D') {
      } else if (VkCode == 'Q') {
      } else if (VkCode == 'E') {
      } else if (VkCode == VK_UP) {
      } else if (VkCode == VK_DOWN) {
      } else if (VkCode == VK_LEFT) {
      } else if (VkCode == VK_RIGHT) {
      } else if (VkCode == VK_ESCAPE) {
        if (WasDown) { OutputDebugString("WasDown"); }
        if (IsDown) { OutputDebugString("IsDown"); }
        OutputDebugString("\n");
      } else if (VkCode == VK_SPACE) {
      }
    }
    if (AltDown && VkCode == VK_F4) { GlobalRunning = false; }

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

struct win32_sound_output {
  uint32_t RunningSampleIndex;
  real32_t tSine;
  int SamplePerSecond;
  int ToneHz;
  // LEFT RIGHT LEFT RIGHT LEFT RIGHT
  // the sample contains left and right channel.
  int BytePerSample;
  int WavePeriod;
  int SecondaryBufferSize;
  int LatencySampleCount;
  int16_t ToneVolume;
};

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

      int XOffset = 0;
      int YOffset = 0;

      win32_sound_output SoundOutput{
          .RunningSampleIndex = 0,
          .tSine = 0,
          .SamplePerSecond = 48000,
          .ToneHz = 256,
          .BytePerSample = sizeof(int16_t) * 2,
          .ToneVolume = 1000,
          // it is middle C 261.625565 Hz.
      };
      SoundOutput.WavePeriod = SoundOutput.SamplePerSecond / SoundOutput.ToneHz;
      SoundOutput.SecondaryBufferSize =
          SoundOutput.SamplePerSecond * SoundOutput.BytePerSample;
      SoundOutput.LatencySampleCount = SoundOutput.SamplePerSecond / 15;

      bool32_t SoundIsPlaying = false;
      Win32InitDSound(
          Window, SoundOutput.SamplePerSecond, SoundOutput.SecondaryBufferSize);
      Win32ClearBuffer(&SoundOutput);
      GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
      SoundIsPlaying = true;
      GlobalRunning = true;

      int16_t *Samples =
          (int16_t *)VirtualAlloc(0,
                                  SoundOutput.SecondaryBufferSize,
                                  MEM_RESERVE | MEM_COMMIT,
                                  PAGE_READWRITE);

      LARGE_INTEGER LastCounter;
      QueryPerformanceCounter(&LastCounter);
      // the RDTSC is an instruction that provides cpu cycle count in
      // currenttime.
      uint64_t LastCyclesCount = __rdtsc();
      while (GlobalRunning) {

        MSG Message;
        // NOTE: HandleWindowMessage, don't need to pass Window handle,
        // it will get all message form this process.
        while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
          if (Message.message == WM_QUIT) { GlobalRunning = false; }
          TranslateMessage(&Message);
          // NOTE: It will dispatch to Upper Registered Windows Message Handler.
          DispatchMessage(&Message);
        }

        // NOTE: ControllerInput.
        for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT;
             ++ControllerIndex) {
          XINPUT_STATE ControllerState;
          if (XInputGetState(ControllerIndex, &ControllerState) ==
              ERROR_SUCCESS) {
            // ControllerState.dwPacketNumber used to detect if the controller
            // state changed, it will be incremented by 1 every time the state
            // is updated.
            // And We cant use this to check is the pulling speed is fast enough
            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
            bool32_t Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
            bool32_t Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
            bool32_t Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
            bool32_t Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

            bool32_t Start = Pad->wButtons & XINPUT_GAMEPAD_START;
            bool32_t Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;

            bool32_t LeftShoulder =
                Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
            bool32_t RightShoulder =
                Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;

            bool32_t ABottom = Pad->wButtons & XINPUT_GAMEPAD_A;
            bool32_t BBottom = Pad->wButtons & XINPUT_GAMEPAD_B;
            bool32_t XBottom = Pad->wButtons & XINPUT_GAMEPAD_X;
            bool32_t YBottom = Pad->wButtons & XINPUT_GAMEPAD_Y;

            int16_t StickX = Pad->sThumbLX;
            int16_t StickY = Pad->sThumbLY;

            // the stick value are signed, so when it shift, it only shift the
            // number that not sign bit. it will cause the negative value will
            // end up with -1.
            // XOffset += StickX >> 10;
            // YOffset += StickY >> 10;

            XOffset += StickX / 2048;
            YOffset += StickY / 2048;

            SoundOutput.ToneHz = 512 + (int)(256.f * (StickY / 32768.f));
            SoundOutput.WavePeriod =
                SoundOutput.SamplePerSecond / SoundOutput.ToneHz;

          } else {
            // NOTE: If ControllerIndex is not connected, we should skip it.
          }
        }

        DWORD BytesToWrite;
        DWORD BytesToLock;
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
        GameUpdateAndRender(
            &Buffer, XOffset, YOffset, &SoundBuffer, SoundOutput.ToneHz);

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
      }
    } else {
      // TODO: Logging when failed
    }
  } else {
    // TODO: Logging when failed
  }

  return 0;
}
