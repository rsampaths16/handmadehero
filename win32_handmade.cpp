#include "win32_handmade.h"
#include "common_used_defs.h"
#include "handmade.cpp"

#include <dsound.h>
#include <malloc.h>
#include <stdio.h>

// TODO: Implement sine ourselves
#include <math.h>
#include <stdint.h>
#include <windows.h>
#include <xinput.h>

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
global_variable boolean GlobalRunning = true;
global_variable win32_offscreen_buffer GlobalBackBuffer = {};
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryAudioBuffer = {};
global_variable int64 GlobalPerfCountFrequency = 0;

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

internal debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename) {
  debug_read_file_result Result = {};
  HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                                  OPEN_EXISTING, 0, 0);

  if (Filename != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER FileSize;
    if (GetFileSizeEx(FileHandle, &FileSize)) {
      uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
      Result.Contents =
          VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      if (Result.Contents) {
        DWORD BytesRead;
        if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
            (FileSize32 && BytesRead)) {
          // NOTE: Successfully read the file
          Result.ContentSize = FileSize32;
        } else {
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
}

internal void DEBUGPlatformFreeFileMemory(void *Memory) {
  VirtualFree(Memory, 0, MEM_RELEASE);
}

internal bool DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize,
                                           void *Memory) {
  bool Result = false;

  HANDLE FileHandle =
      CreateFileA(Filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
  if (Filename != INVALID_HANDLE_VALUE) {
    DWORD BytesWritten;
    if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0)) {
      Result = (BytesWritten == MemorySize);
    } else {
      // TODO: Logging
    }
    CloseHandle(FileHandle);
  } else {
    // TODO: Logging
  }

  return Result;
}

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

internal void Win32ClearSoundBuffer(win32_sound_output *SoundOutput) {
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  if (SUCCEEDED(GlobalSecondaryAudioBuffer->Lock(
          0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2,
          &Region2Size, 0))) {

    // TODO: assert that Region1Size & Region2Size are valid
    uint8 *DestSample1 = (uint8 *)Region1;
    for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ByteIndex++) {
      *DestSample1++ = 0;
    }

    uint8 *DestSample2 = (uint8 *)Region2;
    for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ByteIndex++) {
      *DestSample2++ = 0;
    }

    GlobalSecondaryAudioBuffer->Unlock(Region1, Region1Size, Region2,
                                       Region2Size);
  }
}

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput,
                                   DWORD ByteToLock, DWORD BytesToWrite,
                                   game_sound_output_buffer *SourceBuffer) {
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  // TODO: More testing needed to make sure this is working
  if (SUCCEEDED(GlobalSecondaryAudioBuffer->Lock(ByteToLock, BytesToWrite,
                                                 &Region1, &Region1Size,
                                                 &Region2, &Region2Size, 0))) {

    // TODO: assert that Region1Size & Region2Size are valid
    DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
    int16 *DestSample1 = (int16 *)Region1;
    for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount;
         SampleIndex++) {
      *DestSample1++ = *SourceBuffer->Samples++;
      *DestSample1++ = *SourceBuffer->Samples++;
      SoundOutput->RunningSampleIndex++;
    }

    DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
    int16 *DestSample2 = (int16 *)Region2;
    for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount;
         SampleIndex++) {
      *DestSample2++ = *SourceBuffer->Samples++;
      *DestSample2++ = *SourceBuffer->Samples++;
      SoundOutput->RunningSampleIndex++;
    }

    GlobalSecondaryAudioBuffer->Unlock(Region1, Region1Size, Region2,
                                       Region2Size);
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

internal void Win32ProcessKeyboardMessage(game_button_state *NewState,
                                          bool IsDown) {
  Assert(NewState->EndedDown != IsDown);
  NewState->EndedDown = IsDown;
  NewState->HalfTransitionCount++;
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                              game_button_state *OldState,
                                              DWORD ButtonBit,
                                              game_button_state *NewState) {
  NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
  NewState->HalfTransitionCount =
      (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message,
                                                  WPARAM WParam,
                                                  LPARAM LParam) {
  LRESULT Result = 0;

  switch (Message) {
  case WM_DESTROY: {
    // TODO: Handle this as an error - recreate window?
    GlobalRunning = false;
    break;
  }
  case WM_CLOSE: {
    // TODO: Handle this with a message to the users?
    GlobalRunning = false;
    break;
  }
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP: {
    Assert(!"Keyboard input came in through a non-dispatch message!");
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

internal real32 Win32ProcessXInputStickValue(SHORT Value,
                                             SHORT DeadZoneThreshold) {
  real32 Result = 0;

  if (Value < -DeadZoneThreshold) {
    Result =
        (real32)(Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);
  } else if (Value > DeadZoneThreshold) {
    Result =
        (real32)(Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold);
  }

  return Result;
}

internal void
Win32ProcessPendingMessages(game_controller_input *KeyboardController) {
  MSG Message;
  while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
    switch (Message.message) {
    case WM_QUIT: {
      GlobalRunning = false;
      break;
    }

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
      uint32 VKCode = (uint32)Message.wParam;

      const int WasKeyDownBit = (1 << 30);
      bool WasKeyDown = ((Message.lParam & WasKeyDownBit) != 0);
      const int IsKeyDownBit = (1 << 31);
      bool IsKeyDown = ((Message.lParam & IsKeyDownBit) == 0);

      const int AltKeyWasDownBit = (1 << 29);
      bool AltKeyWasDown = ((Message.lParam & AltKeyWasDownBit) != 0);

      if (WasKeyDown != IsKeyDown) {
        if (VKCode == 'W') {
          Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsKeyDown);
        } else if (VKCode == 'A') {
          Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsKeyDown);
        } else if (VKCode == 'S') {
          Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsKeyDown);
        } else if (VKCode == 'D') {
          Win32ProcessKeyboardMessage(&KeyboardController->MoveRight,
                                      IsKeyDown);
        } else if (VKCode == 'Q') {
          Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder,
                                      IsKeyDown);
        } else if (VKCode == 'E') {
          Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder,
                                      IsKeyDown);
        } else if (VKCode == VK_UP) {
          Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsKeyDown);
        } else if (VKCode == VK_LEFT) {
          Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft,
                                      IsKeyDown);
        } else if (VKCode == VK_DOWN) {
          Win32ProcessKeyboardMessage(&KeyboardController->ActionDown,
                                      IsKeyDown);
        } else if (VKCode == VK_RIGHT) {
          Win32ProcessKeyboardMessage(&KeyboardController->ActionRight,
                                      IsKeyDown);
        } else if (VKCode == VK_ESCAPE) {
          Win32ProcessKeyboardMessage(&KeyboardController->Start, IsKeyDown);
        } else if (VKCode == VK_SPACE) {
          Win32ProcessKeyboardMessage(&KeyboardController->Back, IsKeyDown);
        } else if (VKCode == VK_RETURN) {
        } else if ((VKCode == VK_F4) && AltKeyWasDown) {
          GlobalRunning = false;
        }
      }
      break;
    }

    default: {
      TranslateMessage(&Message);
      DispatchMessage(&Message);
      break;
    }
    }
  }
}

inline LARGE_INTEGER Win32GetWallClock() {
  LARGE_INTEGER Result;
  QueryPerformanceCounter(&Result);

  return Result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
  real32 Result = ((real32)(End.QuadPart - Start.QuadPart) /
                   (real32)GlobalPerfCountFrequency);

  return Result;
}

internal void Win32ProcessLoop(HWND Window) {
  LARGE_INTEGER QueryPerformanceFrequencyResult;
  QueryPerformanceFrequency(&QueryPerformanceFrequencyResult);
  GlobalPerfCountFrequency = QueryPerformanceFrequencyResult.QuadPart;

  /*
   * NOTE: Set the Windows scheduler granularity to 1ms
   * So that the game loop sleep can be more granular.
   */
  UINT DesiredSchedulerMS = 1;
  bool SleepIsGranular =
      (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

  // TODO: How to reliably query monitor refresh-rate on Windows?
  int MonitorRefreshHz = 60;
  int GameUpdateHz = MonitorRefreshHz / 2;
  real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

  // NOTE: Graphics Test
  int XOffset = 0;
  int YOffset = 0;

  // NOTE: Sound Test
  win32_sound_output SoundOutput;
  SoundOutput.SamplesPerSecond = 48000;
  SoundOutput.RunningSampleIndex = 0;
  SoundOutput.BytesPerSample = sizeof(int16) * 2;
  SoundOutput.SecondaryBufferSize =
      SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
  SoundOutput.WriteAheadSamples = SoundOutput.SamplesPerSecond / 15;

  Win32InitDSound(Window, SoundOutput.SamplesPerSecond,
                  SoundOutput.SecondaryBufferSize);
  Win32ClearSoundBuffer(&SoundOutput);
  GlobalSecondaryAudioBuffer->Play(0, 0, DSBPLAY_LOOPING);

  int16 *Samples =
      (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#if HANDMADE_INTERNAL
  LPVOID BaseAddress = (void *)Terabytes(2);
#else
  LPVOID BaseAddress = 0;
#endif

  game_memory GameMemory = {};
  GameMemory.IsInitialized = false;
  GameMemory.PermanentStorageSize = Megabytes(64);
  GameMemory.TransientStorageSize = Gigabytes(4);

  uint64 TotalSize =
      GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;

  GameMemory.PermanentStorage = (void *)VirtualAlloc(
      BaseAddress, TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  GameMemory.TransientStorage =
      ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

  if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage) {

    game_input Input[2] = {};
    game_input *NewInput = &Input[0];
    game_input *OldInput = &Input[1];

    uint64 LastCycleCount = __rdtsc();
    LARGE_INTEGER LastCounter = Win32GetWallClock();

    while (GlobalRunning) {

      game_controller_input *OldKeyboardController = GetController(OldInput, 0);
      game_controller_input *NewKeyboardController = GetController(NewInput, 0);
      // TODO: Add a Zeroing macro
      // TODO: We can't zero everything because the up/down state will be wrong
      *NewKeyboardController = {};
      NewKeyboardController->IsConnected = true;

      for (int ButtonIndex = 0;
           ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
           ButtonIndex++) {
        NewKeyboardController->Buttons[ButtonIndex].EndedDown =
            OldKeyboardController->Buttons[ButtonIndex].EndedDown;
      }

      Win32ProcessPendingMessages(NewKeyboardController);

      /*
       * TODO: Need to not poll disconnected controllers to avoid x input
       * frame rate hit on older libraries.
       */

      DWORD MaxControllerCount = XUSER_MAX_COUNT;
      if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1)) {
        MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
      }

      for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount;
           ControllerIndex++) {
        DWORD OurControllerIndex = ControllerIndex + 1;
        game_controller_input *OldController =
            GetController(OldInput, OurControllerIndex);
        game_controller_input *NewController =
            GetController(NewInput, OurControllerIndex);

        XINPUT_STATE ControllerState;
        DWORD ResponseStatus =
            XInputGetState(ControllerIndex, &ControllerState);

        if (ResponseStatus == ERROR_SUCCESS) {
          NewController->IsConnected = true;

          // TODO: See if ControllerState.dwPacketNumber increments too rapidly
          XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

          /*
           * TODO: This is handling a "square" deadzone. check XInput to verify
           * that the deadzone is "round". If yes, handle round deadzone.
           */
          NewController->StickAverageX = Win32ProcessXInputStickValue(
              Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
          NewController->StickAverageY = Win32ProcessXInputStickValue(
              Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

          if ((NewController->StickAverageX != 0.0f) ||
              (NewController->StickAverageY)) {
            NewController->IsAnalog = true;
          }

          if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
            NewController->StickAverageY = 1.0f;
            NewController->IsAnalog = false;
          }
          if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
            NewController->StickAverageY = -1.0f;
            NewController->IsAnalog = false;
          }
          if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
            NewController->StickAverageX = -1.0f;
            NewController->IsAnalog = false;
          }
          if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
            NewController->StickAverageX = 1.0f;
            NewController->IsAnalog = false;
          }

          real32 Threshold = 0.5f;
          Win32ProcessXInputDigitalButton(
              (NewController->StickAverageX < -Threshold) ? 1 : 0,
              &OldController->MoveLeft, 1, &NewController->MoveLeft);
          Win32ProcessXInputDigitalButton(
              (NewController->StickAverageX > Threshold) ? 1 : 0,
              &OldController->MoveRight, 1, &NewController->MoveRight);
          Win32ProcessXInputDigitalButton(
              (NewController->StickAverageY < -Threshold) ? 1 : 0,
              &OldController->MoveDown, 1, &NewController->MoveDown);
          Win32ProcessXInputDigitalButton(
              (NewController->StickAverageY > Threshold) ? 1 : 0,
              &OldController->MoveUp, 1, &NewController->MoveUp);

          Win32ProcessXInputDigitalButton(
              Pad->wButtons, &OldController->ActionDown, XINPUT_GAMEPAD_A,
              &NewController->ActionDown);
          Win32ProcessXInputDigitalButton(
              Pad->wButtons, &OldController->ActionRight, XINPUT_GAMEPAD_B,
              &NewController->ActionRight);
          Win32ProcessXInputDigitalButton(
              Pad->wButtons, &OldController->ActionLeft, XINPUT_GAMEPAD_X,
              &NewController->ActionLeft);
          Win32ProcessXInputDigitalButton(
              Pad->wButtons, &OldController->ActionUp, XINPUT_GAMEPAD_Y,
              &NewController->ActionUp);

          Win32ProcessXInputDigitalButton(
              Pad->wButtons, &OldController->LeftShoulder,
              XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
          Win32ProcessXInputDigitalButton(
              Pad->wButtons, &OldController->RightShoulder,
              XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);

          Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Start,
                                          XINPUT_GAMEPAD_START,
                                          &NewController->Start);
          Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back,
                                          XINPUT_GAMEPAD_BACK,
                                          &NewController->Back);

        } else {
          NewController->IsConnected = false;
        }
      }

      DWORD ByteToLock = 0;
      DWORD TargetCursor = 0;
      DWORD BytesToWrite = 0;
      DWORD PlayCursor = 0;
      DWORD WriteCursor = 0;
      bool SoundIsValid = false;
      if (SUCCEEDED(GlobalSecondaryAudioBuffer->GetCurrentPosition(
              &PlayCursor, &WriteCursor))) {

        ByteToLock =
            (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
            SoundOutput.SecondaryBufferSize;
        TargetCursor = (PlayCursor + (SoundOutput.WriteAheadSamples *
                                      SoundOutput.BytesPerSample)) %
                       SoundOutput.SecondaryBufferSize;

        if (ByteToLock > TargetCursor) {
          BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
          BytesToWrite += TargetCursor;
        } else {
          BytesToWrite = TargetCursor - ByteToLock;
        }
        SoundIsValid = true;
      }

      /*
       * TODO: Sound is wrong now, because we haven't upgraded to sync
       * with the new framerate logic
       */
      game_sound_output_buffer SoundBuffer = {};
      SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
      SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
      SoundBuffer.Samples = Samples;

      game_offscreen_buffer Buffer = {};
      Buffer.Memory = GlobalBackBuffer.Memory;
      Buffer.Width = GlobalBackBuffer.Width;
      Buffer.Height = GlobalBackBuffer.Height;
      Buffer.Pitch = GlobalBackBuffer.Pitch;

      GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);

      if (SoundIsValid) {
        Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite,
                             &SoundBuffer);
      }

      LARGE_INTEGER WorkCounter = Win32GetWallClock();
      real32 WorkSecondsElapsed =
          Win32GetSecondsElapsed(LastCounter, WorkCounter);

      // TODO: Need more tuning/testing, probably buggy!
      real32 SecondsElapsedForFrame = WorkSecondsElapsed;
      if (SecondsElapsedForFrame < TargetSecondsPerFrame) {
        if (SleepIsGranular) {
          DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame -
                                             SecondsElapsedForFrame));
          if (SleepMS > 0) {
            Sleep(SleepMS);
          }
        }

        Assert(Win32GetSecondsElapsed(LastCounter, Win32GetWallClock()) <
               TargetSecondsPerFrame);
        while (SecondsElapsedForFrame < TargetSecondsPerFrame) {
          SecondsElapsedForFrame =
              Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
        }
      } else {
        // TODO: MISSED FRAME RATE!
        // TODO: Logging
      }

      HDC DeviceContext = GetDC(Window);
      win32_window_dimension Dimension = Win32GetWindowDimension(Window);
      Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
                                 Dimension.Width, Dimension.Height);
      ReleaseDC(Window, DeviceContext);

      // TODO: Should I clear these here?
      game_input *Temp = NewInput;
      NewInput = OldInput;
      OldInput = Temp;

      LARGE_INTEGER EndCounter = Win32GetWallClock();

      uint64 EndCycleCount = __rdtsc();
      int64 CyclesElapsed = EndCycleCount - LastCycleCount;

      real32 MSPerFrame =
          1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
      // real32 FPS = (((real32)GlobalPerfCountFrequency) /
      // ((real32)CounterElapsed));
      real32 FPS = 0.0f;
      real32 MCPF = (real32)(((real32)CyclesElapsed) / (1000.0f * 1000.0f));

      char StringBuffer[256];
      sprintf_s(StringBuffer, "%0.2fms/f,  %0.2ff/s, %0.2fmc/f\n", MSPerFrame,
                FPS, MCPF);
      OutputDebugStringA(StringBuffer);

      LastCounter = EndCounter;
      LastCycleCount = EndCycleCount;
    }
  }
}

int APIENTRY WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR Cmdline,
                     int ShowCode) {
  Win32LoadXInput();
  Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
  WNDCLASSEXA WindowClass = Win32ConstructMainWindowClass(Instance);
  HWND Window = Win32RegisterAndCreateWindow(Instance, &WindowClass);

  if (Window != NULL) {
    Win32ProcessLoop(Window);
  }

  return 0;
}
