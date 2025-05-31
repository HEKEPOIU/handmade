#include <cstdint>
#include <windows.h>

#define internal static
#define local_persist static
#define global_persist static

struct win32_offscreen_buffer {
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
  int BytePerPixel;
};
global_persist bool GlobalRunning;
global_persist win32_offscreen_buffer GlobalBackbuffer;

struct win32_window_dimension {
  int Width;
  int Height;
};

win32_window_dimension Win32GetWindowDimension(HWND Window) {
  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  LONG Width = ClientRect.right - ClientRect.left;
  LONG Height = ClientRect.bottom - ClientRect.top;
  return {Width, Height};
}

internal void RenderWeirdGradient(win32_offscreen_buffer buffer,
                                  int BlueOffset,
                                  int GreenOffset) {
  uint8_t *Row = (uint8_t *)buffer.Memory;
  for (int Y = 0; Y < buffer.Height; ++Y) {
    uint32_t *Pixel = (uint32_t *)Row;
    for (int X = 0; X < buffer.Width; ++X) {
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
    Row += buffer.Pitch;
  }
}

// DIB: Device Independent Bitmap
internal void
Win32ResizeDIBSection(win32_offscreen_buffer *buffer, int Width, int Height) {
  if (buffer->Memory) {
    VirtualFree(buffer->Memory, 0, MEM_RELEASE);
  }
  buffer->Width = Width;
  buffer->Height = Height;
  buffer->BytePerPixel = 4;

  buffer->Info.bmiHeader.biSize = sizeof(buffer->Info.bmiHeader);
  buffer->Info.bmiHeader.biWidth = buffer->Width;
  // if biHeight > 0 => bottom-up bitmap, origin from lower left corner.
  // if biHeight < 0 => top-down bitmap, origin from upper left corner.
  buffer->Info.bmiHeader.biHeight = -buffer->Height;
  buffer->Info.bmiHeader.biPlanes = 1;
  buffer->Info.bmiHeader.biBitCount = 32;
  buffer->Info.bmiHeader.biCompression = BI_RGB;

  int BitmapMemorySize = 4 * Width * Height;
  buffer->Memory =
      VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

  buffer->Pitch = Width * buffer->BytePerPixel;
}

internal void Win32DisplayBufferInWindow(HDC DeviceContext,
                                         int WindowWidth,
                                         int WindowHeight,
                                         win32_offscreen_buffer buffer) {
  // clang-format off
  StretchDIBits(DeviceContext,
                0, 0, WindowWidth, WindowHeight,
                0, 0, buffer.Width, buffer.Height,
                buffer.Memory,
                &buffer.Info,
                DIB_RGB_COLORS,
                SRCCOPY);
  // clang-format on
};

LRESULT WINAPI Win32MainWindowsCallback(HWND Window,
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

  case WM_ACTIVATEAPP: {
  } break;
  case WM_PAINT: {
    PAINTSTRUCT Paint;
    HDC DeviceContext = BeginPaint(Window, &Paint);
    LONG X = Paint.rcPaint.left;
    LONG Y = Paint.rcPaint.top;
    LONG Width = Paint.rcPaint.right - Paint.rcPaint.left;
    LONG Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
    Win32DisplayBufferInWindow(
        DeviceContext, Dimension.Width, Dimension.Height, GlobalBackbuffer);
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
      GlobalRunning = true;
      while (GlobalRunning) {
        MSG Message;
        while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
          if (Message.message == WM_QUIT) {
            GlobalRunning = false;
          }
          TranslateMessage(&Message);
          DispatchMessage(&Message);
        }
        RenderWeirdGradient(GlobalBackbuffer, XOffset, YOffset);

        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(
            DeviceContext, Dimension.Width, Dimension.Height, GlobalBackbuffer);

        ++XOffset;
      }
    } else {
      // TODO: Logging when failed
    }
  } else {
    // TODO: Logging when failed
  }

  return 0;
}
