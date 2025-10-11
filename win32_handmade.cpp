#include <dsound.h>
#include <stdint.h>
#include <windows.h>
#include <xinput.h>

#define internal static
#define local_variable static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer {
  /*
   * NOTE: Each Pixel is 32-bits wide. Memory Order is BB GG RR XX.
   */
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

// NOTE: Support for XInputGetState
#define X_INPUT_GET_STATE(name)                                                \
  DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE: Support for XInputSetState
#define X_INPUT_SET_STATE(name)                                                \
  DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// TODO: This is global for now; Need a proper solution for this;
global_variable boolean MessageLoopRunning = true;
global_variable win32_offscreen_buffer GlobalBackBuffer = {};
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryAudioBuffer = {};

internal void Win32LoadXInput(void) {
  // TODO: diagnostic log - which version was used
  HMODULE XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
  if (XInputLibrary == NULL) {
    XInputLibrary = LoadLibraryA("xinput1_4.dll");
  }
  if (XInputLibrary == NULL) {
    XInputLibrary = LoadLibraryA("xinput1_3.dll");
  }

  if (XInputLibrary != NULL) {
    XInputGetState =
        (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState =
        (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
  } else {
    // TODO: log diagnostic
  }
}

#define DIRECT_SOUND_CREATE(name)                                              \
  HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS,               \
                      LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond,
                              int32 BufferSize) {
  HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
  if (DSoundLibrary != NULL) {
    direct_sound_create *DirectSoundCreate =
        (direct_sound_create *)GetProcAddress(DSoundLibrary,
                                              "DirectSoundCreate");

    LPDIRECTSOUND DirectSound;
    if (SUCCEEDED(DirectSoundCreate(0, &DirectSound, NULL))) {

      WAVEFORMATEX WaveFormat = {};
      WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
      WaveFormat.nChannels = 2;
      WaveFormat.nSamplesPerSec = SamplesPerSecond;
      WaveFormat.wBitsPerSample = 16;
      WaveFormat.nBlockAlign =
          (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
      WaveFormat.nAvgBytesPerSec =
          WaveFormat.nBlockAlign * WaveFormat.nSamplesPerSec;
      WaveFormat.cbSize = 0;

      if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
        DSBUFFERDESC BufferDescription = {};
        BufferDescription.dwSize = sizeof(BufferDescription);
        BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

        LPDIRECTSOUNDBUFFER PrimaryBuffer;
        if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription,
                                                     &PrimaryBuffer, NULL))) {
          if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
            OutputDebugStringA("Primary buffer format was created & set\n");
          } else {
            // TODO: log diagnostic
          }
        } else {
          // TODO: log diagnostic
        }
      } else {
        // TODO: log diagnostic
      }

      DSBUFFERDESC BufferDescription = {};
      BufferDescription.dwSize = sizeof(BufferDescription);
      BufferDescription.dwFlags = 0;
      BufferDescription.dwBufferBytes = BufferSize;
      BufferDescription.lpwfxFormat = &WaveFormat;
      if (SUCCEEDED(DirectSound->CreateSoundBuffer(
              &BufferDescription, &GlobalSecondaryAudioBuffer, NULL))) {
        OutputDebugStringA("Secondary buffer format was created & set\n");
      } else {
        // TODO: log diagnostic
      }
    } else {
      // TODO: log diagnostic
    }

  } else {
    // TODO: log diagnostic
  }
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window) {
  win32_window_dimension Result;

  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  Result.Height = ClientRect.bottom - ClientRect.top;
  Result.Width = ClientRect.right - ClientRect.left;

  return Result;
}

internal void RenderTestGradient(win32_offscreen_buffer *Buffer, int BlueOffset,
                                 int GreenOffset) {
  uint8 *Row = (uint8 *)Buffer->Memory;
  for (int Y = 0; Y < Buffer->Height; Y++) {

    uint32 *Pixel = (uint32 *)Row;
    for (int X = 0; X < Buffer->Width; X++) {
      uint8 Blue = (X + BlueOffset);
      uint8 Green = (Y + GreenOffset);

      *Pixel++ = ((Green << 8) | Blue);
    }
    Row += Buffer->Pitch;
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

  /*
   * NOTE: Negative Height is hint to windows on the orientation of the buffer.
   * A negative height means the buffer renders the screen from the top left
   * i.e., the first 4 bytes represent the top left pixel, and so on row by row.
   *
   * REFERENCE:
   * https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader
   */
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

  int BytesPerPixel = 4;
  int BitmapMemorySize = (Buffer->Width * Buffer->Height) * BytesPerPixel;
  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT,
                                PAGE_READWRITE);
  Buffer->Pitch = Buffer->Width * BytesPerPixel;

  // TODO: Might want to clear screen to black
}

internal void Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
                                         HDC DeviceContext, int WindowWidth,
                                         int WindowHeight) {
  // TODO: Handle aspect ratio correction
  StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0,
                Buffer->Width, Buffer->Height, Buffer->Memory, &Buffer->Info,
                DIB_RGB_COLORS, SRCCOPY);
}

internal LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message,
                                                  WPARAM WParam,
                                                  LPARAM LParam) {
  LRESULT Result = 0;

  switch (Message) {
  case WM_DESTROY: {
    // TODO: Handle this as an error - recreate window?
    MessageLoopRunning = false;
    break;
  }
  case WM_CLOSE: {
    // TODO: Handle this with a message to the users?
    MessageLoopRunning = false;
    break;
  }
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP: {
    uint32 VKCode = WParam;

    const int WasKeyDownBit = (1 << 30);
    bool WasKeyDown = ((LParam & WasKeyDownBit) != 0);
    const int IsKeyDownBit = (1 << 31);
    bool IsKeyDown = ((LParam & IsKeyDownBit) == 0);

    const int AltKeyWasDownBit = (1 << 29);
    bool AltKeyWasDown = ((LParam & AltKeyWasDownBit) != 0);

    if (WasKeyDown != IsKeyDown) {
      if (VKCode == 'W') {
      } else if (VKCode == 'A') {
      } else if (VKCode == 'S') {
      } else if (VKCode == 'D') {
      } else if (VKCode == 'Q') {
      } else if (VKCode == 'E') {
      } else if (VKCode == VK_UP) {
      } else if (VKCode == VK_LEFT) {
      } else if (VKCode == VK_DOWN) {
      } else if (VKCode == VK_RIGHT) {
      } else if (VKCode == VK_ESCAPE) {
        OutputDebugStringA("ESCAPE: ");
        if (IsKeyDown) {
          OutputDebugStringA("IsDown ");
        }
        if (WasKeyDown) {
          OutputDebugStringA("WasDown");
        }
        OutputDebugStringA("\n");
      } else if (VKCode == VK_SPACE) {
      } else if (VKCode == VK_RETURN) {
      } else if ((VKCode == VK_F4) && AltKeyWasDown) {
        MessageLoopRunning = false;
      }
    }
    break;
  }
  case WM_PAINT: {
    PAINTSTRUCT Paint;
    HDC DeviceContext = BeginPaint(Window, &Paint);
    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
    Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
                               Dimension.Width, Dimension.Height);
    EndPaint(Window, &Paint);
    break;
  }
  default: {
    Result = DefWindowProc(Window, Message, WParam, LParam);
    break;
  }
  }

  return Result;
}

internal WNDCLASSEXA Win32ConstructMainWindowClass(HINSTANCE Instance) {
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

internal HWND Win32RegisterAndCreateWindow(HINSTANCE Instance,
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

internal void Win32MessageLoop(HWND Window) {
  int XOffset = 0;
  int YOffset = 0;

  const int SamplingFrequenceHz = 48000;
  const int BufferLengthInSeconds = 2;
  Win32InitDSound(Window, SamplingFrequenceHz,
                  SamplingFrequenceHz * sizeof(int16) * BufferLengthInSeconds);

  while (MessageLoopRunning) {
    MSG Message;

    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
      if (Message.message == WM_QUIT) {
        MessageLoopRunning = false;
      }

      TranslateMessage(&Message);
      DispatchMessage(&Message);
    }

    for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT;
         ControllerIndex++) {
      XINPUT_STATE ControllerState;
      DWORD ResponseStatus = XInputGetState(ControllerIndex, &ControllerState);

      if (ResponseStatus == ERROR_SUCCESS) {
        // NOTE: Controller is connected
        // TODO: See if ControllerState.dwPacketNumber increments too rapidly
        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
        bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
        bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
        bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
        bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
        bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
        bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
        bool LeftThumb = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
        bool RightThumb = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
        bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
        bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
        bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
        bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
        bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
        bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

        int16 LeftStickX = Pad->sThumbLX;
        int16 LeftStickY = Pad->sThumbLY;

        int16 RightStickX = Pad->sThumbRX;
        int16 RightStickY = Pad->sThumbRY;

        XOffset -= LeftStickX >> 12;
        YOffset += LeftStickY >> 12;
      } else {
        // NOTE: Controller is not connected
      }
    }

    RenderTestGradient(&GlobalBackBuffer, XOffset, YOffset);

    HDC DeviceContext = GetDC(Window);
    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
    Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
                               Dimension.Width, Dimension.Height);
    ReleaseDC(Window, DeviceContext);
  }
}

int APIENTRY WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR Cmdline,
                     int ShowCode) {
  Win32LoadXInput();
  Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
  WNDCLASSEXA WindowClass = Win32ConstructMainWindowClass(Instance);
  HWND Window = Win32RegisterAndCreateWindow(Instance, &WindowClass);

  if (Window != NULL) {
    Win32MessageLoop(Window);
  }

  return 0;
}
