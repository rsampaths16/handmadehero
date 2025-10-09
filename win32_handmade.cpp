#include <stdint.h>
#include <windows.h>

#define internal static
#define local_persist static
#define global_persist static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer {
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
  int BytesPerPixel;
};

// TODO: This is global for now; Need a proper solution for this;
global_persist boolean MessageLoopRunning = true;
global_persist win32_offscreen_buffer GlobalBackBuffer = {};

internal void RenderTestGradient(win32_offscreen_buffer Buffer, int BlueOffset,
                                 int GreenOffset) {
  uint8 *Row = (uint8 *)Buffer.Memory;
  for (int Y = 0; Y < Buffer.Height; Y++) {

    uint32 *Pixel = (uint32 *)Row;
    for (int X = 0; X < Buffer.Width; X++) {
      uint8 Blue = (X + BlueOffset);
      uint8 Green = (Y + GreenOffset);

      *Pixel++ = ((Green << 8) | Blue);
    }
    Row += Buffer.Pitch;
  }
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width,
                                    int Height) {

  if (Buffer->Memory != NULL) {
    /*
     * TODO: Improve handling for Bitmap buffer memory allocation.
     *   We can either free the old buffer and wait for new allocation (or)
     *   Request new buffer, while using the old one and then free it.
     *
     *   The former approch would release memory resource to be used in
     * reallocation, while the latter is faster - but could fail if enough
     * memory isn't available.
     *
     *   For now we're using the former, as we're not sure if the latter would
     * fail or succeed. Should revisit this later for optimisation.
     */
    VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
  }

  Buffer->Width = Width;
  Buffer->Height = Height;
  Buffer->BytesPerPixel = 4;

  Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
  Buffer->Info.bmiHeader.biWidth = Buffer->Width;
  Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
  Buffer->Info.bmiHeader.biPlanes = 1;
  /*
   * NOTE: Ideally only needs 24 (3*8bits for RGB)
   * We're requesting 32bits for DWORD alignment in CPUs
   */
  Buffer->Info.bmiHeader.biBitCount = 32;
  Buffer->Info.bmiHeader.biCompression = BI_RGB;

  int BitmapMemorySize =
      (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
  Buffer->Memory =
      VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
  Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;

  // TODO: Might want to clear screen to black
}

internal void Win32DisplayBufferInWindow(HDC DeviceContext, RECT ClientRect,
                                         win32_offscreen_buffer Buffer, int X,
                                         int Y, int Width, int Height) {
  int WindowWidth = ClientRect.right - ClientRect.left;
  int WindowHeight = ClientRect.bottom - ClientRect.top;
  StretchDIBits(DeviceContext, 0, 0, Buffer.Width, Buffer.Height, 0, 0,
                WindowWidth, WindowHeight, Buffer.Memory, &Buffer.Info,
                DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message,
                                         WPARAM WParam, LPARAM LParam) {
  LRESULT Result = 0;

  switch (Message) {
  case WM_SIZE: {
    OutputDebugStringA("WM_SIZE\n");

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    LONG Height = ClientRect.bottom - ClientRect.top;
    LONG Width = ClientRect.right - ClientRect.left;
    Win32ResizeDIBSection(&GlobalBackBuffer, Width, Height);

    break;
  }
  case WM_DESTROY: {
    OutputDebugStringA("WM_DESTROY\n");

    // TODO: Handle this as an error - recreate window?
    MessageLoopRunning = false;
    break;
  }
  case WM_CLOSE: {
    OutputDebugStringA("WM_CLOSE\n");

    // TODO: Handle this with a message to the users?
    MessageLoopRunning = false;
    break;
  }
  case WM_ACTIVATEAPP: {
    OutputDebugStringA("WM_ACTIVATEAPP\n");
    break;
  }
  case WM_PAINT: {
    OutputDebugStringA("WM_PAINT\n");
    PAINTSTRUCT Paint;
    HDC DeviceContext = BeginPaint(Window, &Paint);
    LONG Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
    LONG Width = Paint.rcPaint.right - Paint.rcPaint.left;
    LONG X = Paint.rcPaint.left;
    LONG Y = Paint.rcPaint.top;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);

    Win32DisplayBufferInWindow(DeviceContext, ClientRect, GlobalBackBuffer, X,
                               Y, Width, Height);
    EndPaint(Window, &Paint);
    break;
  }
  default: {
    // OutputDebugStringA("default\n");
    Result = DefWindowProc(Window, Message, WParam, LParam);
    break;
  }
  }

  return Result;
}

WNDCLASSEXA Win32ConstructMainWindowClass(HINSTANCE Instance) {
  WNDCLASSEXA WindowClass = {};
  WindowClass.cbSize = sizeof(WNDCLASSEXA);
  WindowClass.style = CS_HREDRAW | CS_VREDRAW;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = Instance;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  // TODO: configure hIcon
  // WindowClass.hIcon = ;

  return WindowClass;
}

HWND Win32RegisterAndCreateWindow(HINSTANCE Instance,
                                  PWNDCLASSEXA WindowClassPtr) {
  ATOM WindowClassAtom = RegisterClassExA(WindowClassPtr);
  if (WindowClassAtom != 0) {
    HWND Window = CreateWindowExA(0, "HandmadeHeroWindowClass", "HandemadeHero",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                  CW_USEDEFAULT, 0, 0, Instance, 0);
    if (Window == NULL) {
      // TODO: Handle / log window creation failures
      OutputDebugStringA("Failure in window creation\n");

      return NULL;
    }

    return Window;
  } else {
    // TODO: Handle / log window registration failures
    OutputDebugStringA("Failure in window registration\n");

    return NULL;
  }
}

void Win32MessageLoop(HWND Window) {
  MSG Message;
  int XOffset = 0;
  int YOffset = 0;

  while (MessageLoopRunning) {
    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
      if (Message.message == WM_QUIT) {
        MessageLoopRunning = false;
      }

      TranslateMessage(&Message);
      DispatchMessage(&Message);
    }

    RenderTestGradient(GlobalBackBuffer, XOffset, YOffset);

    HDC DeviceContext = GetDC(Window);
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    int WindowWidth = ClientRect.right - ClientRect.left;
    int WindowHeight = ClientRect.bottom - ClientRect.top;
    Win32DisplayBufferInWindow(DeviceContext, ClientRect, GlobalBackBuffer, 0,
                               0, WindowWidth, WindowHeight);
    ReleaseDC(Window, DeviceContext);

    ++XOffset;
  }
}

int APIENTRY WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR Cmdline,
                     int ShowCode) {
  WNDCLASSEXA WindowClass = Win32ConstructMainWindowClass(Instance);
  HWND Window = Win32RegisterAndCreateWindow(Instance, &WindowClass);

  if (Window != NULL) {
    Win32MessageLoop(Window);
  }

  return 0;
}
