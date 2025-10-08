#include <windows.h>

#define internal static
#define local_persist static
#define global_persist static

// TODO: This is global for now; Need a proper solution for this;
global_persist boolean MessageLoopRunning = true;
global_persist BITMAPINFO BitmapInfo = {};
global_persist void *BitmapMemory = NULL;
global_persist HBITMAP BitmapHandle = NULL;
global_persist HDC BitmapDeviceContext = NULL;

internal void Win32ResizeDIBSection(int Width, int Height) {

  if (BitmapHandle != NULL) {
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
    DeleteObject(BitmapHandle);
  }

  if (BitmapDeviceContext == NULL) {
    /*
     * TODO: Decide if these should be created instead
     *   We're not sure what happens in cases when monitors are
     * disconnected/connected window is dragged across monitors ... etc and
     * other cases.
     *
     *   We're reusing the context for now, but will need to decide if it should
     * be recreated instead
     */
    BitmapDeviceContext = CreateCompatibleDC(0);
  }

  BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
  BitmapInfo.bmiHeader.biWidth = Width;
  BitmapInfo.bmiHeader.biHeight = Height;
  BitmapInfo.bmiHeader.biPlanes = 1;
  /*
   * NOTE: Ideally only needs 24 (3*8bits for RGB)
   * We're requesting 32bits for DWORD alignment in CPUs
   */
  BitmapInfo.bmiHeader.biBitCount = 32;
  BitmapInfo.bmiHeader.biCompression = BI_RGB;

  BitmapHandle = CreateDIBSection(BitmapDeviceContext, &BitmapInfo,
                                  DIB_RGB_COLORS, &BitmapMemory, NULL, NULL);
}

internal void Win32UpdateWindow(HDC DeviceContext, int X, int Y, int Width,
                                int Height) {
  StretchDIBits(DeviceContext, X, Y, Width, Height, X, Y, Width, Height,
                BitmapMemory, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
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
    Win32ResizeDIBSection(Width, Height);

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
    Win32UpdateWindow(DeviceContext, X, Y, Width, Height);
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
  WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
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

void Win32MessageLoop() {
  MSG Message;
  while (MessageLoopRunning) {
    BOOL MessageResult = GetMessage(&Message, 0, 0, 0);
    if (MessageResult > 0) {
      TranslateMessage(&Message);
      DispatchMessage(&Message);
    } else {
      // TODO: Handle / log message retrival failures
      OutputDebugStringA("Failure in message retrival\n");
      break;
    }
  }
}

int APIENTRY WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR Cmdline,
                     int ShowCode) {
  WNDCLASSEXA WindowClass = Win32ConstructMainWindowClass(Instance);
  HWND Window = Win32RegisterAndCreateWindow(Instance, &WindowClass);

  if (Window != NULL) {
    Win32MessageLoop();
  }

  return 0;
}
