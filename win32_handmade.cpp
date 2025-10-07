#include <windows.h>

#define internal static
#define local_persist static
#define global_persist static

// TODO: This is global for now; Need a proper solution for this;
global_persist boolean MessageLoopRunning = true;

LRESULT CALLBACK MainWindowCallback(HWND Window, UINT Message, WPARAM WParam,
                                    LPARAM LParam) {
  LRESULT Result = 0;

  switch (Message) {
  case WM_SIZE: {
    OutputDebugStringA("WM_SIZE\n");
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
    local_persist DWORD ColorCode = WHITENESS;
    if (ColorCode == WHITENESS) {
      ColorCode = BLACKNESS;
    } else {
      ColorCode = WHITENESS;
    }
    PatBlt(DeviceContext, X, Y, Width, Height, ColorCode);
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

WNDCLASSEXA ConstructMainWindowClass(HINSTANCE Instance) {
  WNDCLASSEXA WindowClass = {};
  WindowClass.cbSize = sizeof(WNDCLASSEXA);
  WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  WindowClass.lpfnWndProc = MainWindowCallback;
  WindowClass.hInstance = Instance;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  // TODO: configure hIcon
  // WindowClass.hIcon = ;

  return WindowClass;
}

HWND RegisterAndCreateWindow(HINSTANCE Instance, PWNDCLASSEXA WindowClassPtr) {
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

void MessageLoop() {
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
  WNDCLASSEXA WindowClass = ConstructMainWindowClass(Instance);
  HWND Window = RegisterAndCreateWindow(Instance, &WindowClass);

  if (Window != NULL) {
    MessageLoop();
  }

  return 0;
}
