
// windows.h fucking need to be included befoure xinput.h
// https://stackoverflow.com/questions/4845198/fatal-error-no-target-architecture-in-visual-studio
// There are a number of child header files that are automatically included with
// windows.h. Many of these files cannot simply be included by themselves (they
// are not self-contained), because of dependencies.

// clang-format off
#include <windows.h>
// clang-format on
#include "xinput.h"
#include <cstdint>
#include <dsound.h>

#define internal static
#define local_persist static
#define global_persist static

typedef int32_t bool32_t;

struct win32_offscreen_buffer {
  // Buffer pixel size always be 32 bit, little endian.
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
};

global_persist bool GlobalRunning;
global_persist win32_offscreen_buffer GlobalBackbuffer;

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
      LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);

typedef DIRECT_SOUND_CREATE(direct_sound_create);

global_persist x_input_get_state *XInputGetState_ = XInputGetStateStub;
global_persist x_input_set_state *XInputSetState_ = XInputSetStateStub;

// Self loading XInputGetState and XInputSetState,
// because if we just link xinput.lib, for some one don't have xinput.h,
// it will fail when open game, but the game are note depend on gamepad.
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

internal void Win32LoadXInput(void) {
  HMODULE XInputModule = LoadLibrary("xinput1_4.dll");
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
          .dwBufferBytes = (WORD)BufferSize,
          .lpwfxFormat = &WaveFormat,
      };

      // NOTE: And this place, SecondaryBuffer are used to actually play audio.
      LPDIRECTSOUNDBUFFER SecondaryBuffer;
      if (SUCCEEDED(DirectSound->CreateSoundBuffer(
              &BufferDescrption, &SecondaryBuffer, 0))) {
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

internal void RenderWeirdGradient(win32_offscreen_buffer *buffer,
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
      uint8_t Red = 0;
      uint8_t Green = (uint8_t)(Y + GreenOffset);
      uint8_t Blue = (uint8_t)(X + BlueOffset);

      *Pixel++ = ((Green << 8) | Blue);
    }
    // Pitch -> 4 * 8 * Width  -> 32 BitCount from top
    Row += buffer->Pitch;
  }
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

  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP: {
    uint32_t VkCode = WParam;

    WORD KeyFlags = HIWORD(LParam);

    // https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#keystroke-message-flags
    // previous key state flag.
    const bool WasDown = ((KeyFlags & KF_REPEAT) == KF_REPEAT);

    // transition state flag.
    const bool IsDown = ((KeyFlags & KF_UP) == 0);
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

int WINAPI WinMain(HINSTANCE Instance,
                   HINSTANCE PrevInstance,
                   LPSTR CommandLine,
                   int ShowCode) {
  Win32LoadXInput();
  WNDCLASS WindowsClass{};

  Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

  WindowsClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
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

      Win32InitDSound(Window, 48000, 48000 * sizeof(int16_t) * 2);
      GlobalRunning = true;
      while (GlobalRunning) {
        MSG Message;
        while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
          if (Message.message == WM_QUIT) { GlobalRunning = false; }
          TranslateMessage(&Message);
          DispatchMessage(&Message);
        }
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
            bool Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
            bool Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
            bool Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
            bool Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

            bool Start = Pad->wButtons & XINPUT_GAMEPAD_START;
            bool Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;

            bool LeftShoulder = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
            bool RightShoulder = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;

            bool ABottom = Pad->wButtons & XINPUT_GAMEPAD_A;
            bool BBottom = Pad->wButtons & XINPUT_GAMEPAD_B;
            bool XBottom = Pad->wButtons & XINPUT_GAMEPAD_X;
            bool YBottom = Pad->wButtons & XINPUT_GAMEPAD_Y;

            int16_t StickX = Pad->sThumbLX;
            int16_t StickY = Pad->sThumbLY;

            XOffset += StickX >> 10;
            YOffset += StickY >> 10;

          } else {
            // NOTE: If ControllerIndex is not connected, we should skip it.
          }
        }
        // XINPUT_VIBRATION Vibration{};
        // Vibration.wLeftMotorSpeed = 60000;
        // Vibration.wRightMotorSpeed = 60000;
        // XInputSetState_(0, &Vibration);
        RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);

        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackbuffer,
                                   DeviceContext,
                                   Dimension.Width,
                                   Dimension.Height);
      }
    } else {
      // TODO: Logging when failed
    }
  } else {
    // TODO: Logging when failed
  }

  return 0;
}
