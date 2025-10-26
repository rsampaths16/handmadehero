#include "win32_handmade.h"
#include "common_used_defs.h"
#include "handmade.h"

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
global_variable boolean GlobalPause = false;
global_variable win32_offscreen_buffer GlobalBackBuffer = {};
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryAudioBuffer = {};
global_variable int64 GlobalPerfCountFrequency = 0;

inline FILETIME Win32GetLastWriteTime(char *FileName) {
  FILETIME LastWriteTime = {};

  WIN32_FILE_ATTRIBUTE_DATA FileInfo = {};
  if (GetFileAttributesExA(FileName, GetFileExInfoStandard, &FileInfo) != 0) {
    LastWriteTime = FileInfo.ftLastWriteTime;
  }

  return LastWriteTime;
}

internal win32_game_code Win32LoadGameCode(char *SourceDLLName,
                                           char *TempDLLName) {
  win32_game_code Result = {};

  Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);

  CopyFile(SourceDLLName, TempDLLName, FALSE);
  Result.GameCodeDLL = LoadLibraryA(TempDLLName);
  if (Result.GameCodeDLL != NULL) {
    Result.UpdateAndRender = (game_update_and_render *)GetProcAddress(
        Result.GameCodeDLL, "GameUpdateAndRender");
    Result.GetSoundSamples = (game_get_sound_samples *)GetProcAddress(
        Result.GameCodeDLL, "GameGetSoundSamples");

    Result.IsValid =
        (Result.UpdateAndRender != NULL) && (Result.GetSoundSamples != NULL);
  }

  if (!Result.IsValid) {
    // TODO: log diagnostic
    Result.UpdateAndRender = GameUpdateAndRenderStub;
    Result.GetSoundSamples = GameGetSoundSamplesStub;
  }

  return Result;
}

internal void Win32UnloadGameCode(win32_game_code *GameCode) {
  if (GameCode->GameCodeDLL) {
    FreeLibrary(GameCode->GameCodeDLL);
    GameCode->GameCodeDLL = NULL;
  }

  GameCode->UpdateAndRender = GameUpdateAndRenderStub;
  GameCode->GetSoundSamples = GameGetSoundSamplesStub;
  GameCode->IsValid = false;
}

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

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory) {
  VirtualFree(Memory, 0, MEM_RELEASE);
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
  debug_read_file_result Result = {};
  HANDLE FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                                  OPEN_EXISTING, 0, 0);

  if (FileHandle != INVALID_HANDLE_VALUE) {
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

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile) {
  bool Result = false;

  HANDLE FileHandle =
      CreateFileA(FileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
  if (FileHandle != INVALID_HANDLE_VALUE) {
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
  Buffer->BytesPerPixel = BytesPerPixel;

  int BitmapMemorySize =
      (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT,
                                PAGE_READWRITE);
  Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;

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

internal void Win32BeginRecordingInput(win32_state *Win32State,
                                       int InputRecordingIndex) {
  Win32State->InputRecordingIndex = InputRecordingIndex;

  char *FileName = "foo.hmi";
  Win32State->RecordingHandle =
      CreateFileA(FileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);

  DWORD BytesToWrite = (DWORD)Win32State->TotalSize;
  Assert(Win32State->TotalSize == BytesToWrite);
  DWORD BytesWritten;
  WriteFile(Win32State->RecordingHandle, Win32State->GameMemoryBlock,
            BytesToWrite, &BytesWritten, 0);
}

internal void Win32EndRecordingInput(win32_state *Win32State) {
  CloseHandle(Win32State->RecordingHandle);
  Win32State->InputRecordingIndex = 0;
}

internal void Win32BeginInputPlayBack(win32_state *Win32State,
                                      int InputPlayingIndex) {
  Win32State->InputPlayingIndex = InputPlayingIndex;

  char *FileName = "foo.hmi";
  Win32State->PlayBackHandle = CreateFileA(
      FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);

  DWORD BytesToRead = (DWORD)Win32State->TotalSize;
  Assert(Win32State->TotalSize == BytesToRead);
  DWORD BytesRead;
  ReadFile(Win32State->PlayBackHandle, Win32State->GameMemoryBlock, BytesToRead,
           &BytesRead, 0);
}

internal void Win32EndInputPlayBack(win32_state *Win32State) {
  CloseHandle(Win32State->PlayBackHandle);
  Win32State->InputPlayingIndex = 0;
}

internal void Win32RecordInput(win32_state *Win32State, game_input *NewInput) {
  DWORD BytesWritten;
  WriteFile(Win32State->RecordingHandle, NewInput, sizeof(*NewInput),
            &BytesWritten, 0);
}

internal void Win32PlayBackInput(win32_state *Win32State,
                                 game_input *NewInput) {
  DWORD BytesRead;
  if (ReadFile(Win32State->PlayBackHandle, NewInput, sizeof(*NewInput),
               &BytesRead, 0)) {
    if (BytesRead == 0) {
      int PlayingIndex = Win32State->InputPlayingIndex;
      Win32EndInputPlayBack(Win32State);
      Win32BeginInputPlayBack(Win32State, PlayingIndex);

      // NOTE: Retry reading game input after reloading state
      ReadFile(Win32State->PlayBackHandle, NewInput, sizeof(*NewInput),
               &BytesRead, 0);
    }
  }
}

internal void
Win32ProcessPendingMessages(win32_state *Win32State,
                            game_controller_input *KeyboardController) {
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
#if HANDMADE_INTERNAL
        else if (VKCode == 'P') {
          if (IsKeyDown) {
            GlobalPause = !GlobalPause;
          }
        } else if (VKCode == 'L') {
          if (IsKeyDown) {
            if (Win32State->InputRecordingIndex == 0 &&
                Win32State->InputPlayingIndex == 0) {
              Win32BeginRecordingInput(Win32State, 1);
            } else if (Win32State->InputRecordingIndex != 0 &&
                       Win32State->InputPlayingIndex == 0) {
              Win32EndRecordingInput(Win32State);
              Win32BeginInputPlayBack(Win32State, 1);
            } else {
              Win32EndInputPlayBack(Win32State);
            }
          }
        }
#endif
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

internal void Win32DebugDrawVertical(win32_offscreen_buffer *BackBuffer, int X,
                                     int Top, int Bottom, uint32 Color) {
  if (Top <= 0) {
    Top = 0;
  }

  if (Bottom > BackBuffer->Height) {
    Bottom = BackBuffer->Height;
  }

  if ((X >= 0) && (X < BackBuffer->Width)) {
    uint8 *Pixel =
        ((uint8 *)BackBuffer->Memory + (X * BackBuffer->BytesPerPixel) +
         (Top * BackBuffer->Pitch));

    for (int Y = Top; Y < Bottom; Y++) {
      *(uint32 *)Pixel = Color;
      Pixel += BackBuffer->Pitch;
    }
  }
}

inline void Win32DrawSoundBufferMarker(win32_offscreen_buffer *BackBuffer,
                                       win32_sound_output *SoundOutput,
                                       real32 C, int PadX, int Top, int Bottom,
                                       DWORD Value, uint32 Color) {
  real32 XReal32 = (C * (real32)Value);
  int X = PadX + (int)XReal32;
  Win32DebugDrawVertical(BackBuffer, X, Top, Bottom, Color);
}

internal void Win32DebugSyncDisplay(win32_offscreen_buffer *BackBuffer,
                                    int MarkerCount,
                                    win32_debug_time_marker *Markers,
                                    int CurrentMarkerIndex,
                                    win32_sound_output *SoundOutput,
                                    real32 TargetSecondsPerFrame) {
  int PadX = 16;
  int PadY = 16;

  int LineHeight = 64;

  real32 C = ((real32)BackBuffer->Width - (2 * PadX)) /
             (real32)SoundOutput->SecondaryBufferSize;
  for (int MarkerIndex = 0; MarkerIndex < MarkerCount; MarkerIndex++) {
    win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
    Assert(ThisMarker->OutputPlayCursor <
           (DWORD)SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->OutputWriteCursor <
           (DWORD)SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->OutputLocation <
           (DWORD)SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->OutputByteCount <
           (DWORD)SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->FlipPlayCursor <
           (DWORD)SoundOutput->SecondaryBufferSize);
    Assert(ThisMarker->FlipWriteCursor <
           (DWORD)SoundOutput->SecondaryBufferSize);

    DWORD PlayColor = 0xFFFFFFFF;
    DWORD WriteColor = 0xFFFF0000;
    DWORD ExpectedFlipColor = 0xFFFFFF00;
    DWORD PlayWindowColor = 0xFFFF00FF;

    int Top = PadY;
    int Bottom = LineHeight + PadY;

    if (MarkerIndex == CurrentMarkerIndex) {
      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      int FirstTop = Top;

      Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                                 ThisMarker->OutputPlayCursor, PlayColor);
      Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                                 ThisMarker->OutputWriteCursor, WriteColor);

      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;
      Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                                 ThisMarker->OutputLocation, PlayColor);
      Win32DrawSoundBufferMarker(
          BackBuffer, SoundOutput, C, PadX, Top, Bottom,
          ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);

      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, FirstTop,
                                 Bottom, ThisMarker->ExpectedFlipPlayCursor,
                                 ExpectedFlipColor);
    }

    Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                               ThisMarker->FlipPlayCursor, PlayColor);
    Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                               ThisMarker->FlipPlayCursor +
                                   (480 * SoundOutput->BytesPerSample),
                               PlayWindowColor);
    Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                               ThisMarker->FlipWriteCursor, WriteColor);
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

void CatStrings(size_t SourceACount, char *SourceA, size_t SourceBCount,
                char *SourceB, size_t DestCount, char *Dest) {

  // TODO: Need better handling for out of bounds condition
  if ((SourceACount + SourceBCount + 1) > DestCount) {
    *Dest = 0;
    return;
  }

  for (size_t Index = 0; Index < SourceACount; Index++) {
    *Dest++ = *SourceA++;
  }

  for (size_t Index = 0; Index < SourceBCount; Index++) {
    *Dest++ = *SourceB++;
  }

  *Dest++ = '\0';
}

internal void Win32ProcessLoop(HWND Window) {
  /*
   * NOTE: Don't use MAX_PATH in code that is user-facing, because it can be
   * dangerous and lead to bad results
   */
  char EXEFileName[MAX_PATH];
  DWORD SizeOfFileName =
      GetModuleFileNameA(0, EXEFileName, sizeof(EXEFileName));

  /*
   * TODO: Use OS/Std provided libraries to handle path related operations. It
   * is not reliable to use simple c-string manipulations as paths support
   * unicode characters. The code used now is only for development stages.
   */
  char *OnePastLastSlash = EXEFileName;
  for (char *Scan = EXEFileName; *Scan; Scan++) {
    if (*Scan == '\\') {
      OnePastLastSlash = Scan + 1;
    }
  }

  char SourceGameCodeDLLFileName[] = "handmade.dll";
  char SourceGameCodeDLLFullPath[MAX_PATH];
  CatStrings(OnePastLastSlash - EXEFileName, EXEFileName,
             sizeof(SourceGameCodeDLLFileName) - 1, SourceGameCodeDLLFileName,
             sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);

  char TempGameCodeDLLFileName[] = "handmade_temp.dll";
  char TempGameCodeDLLFullPath[MAX_PATH];
  CatStrings(OnePastLastSlash - EXEFileName, EXEFileName,
             sizeof(TempGameCodeDLLFileName) - 1, TempGameCodeDLLFileName,
             sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);

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
  const int MonitorRefreshHz = 60;
  const int GameUpdateHz = MonitorRefreshHz / 2;
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
  SoundOutput.WriteAheadSamples =
      3 * (SoundOutput.SamplesPerSecond / GameUpdateHz);

  // TODO: Find out the lowest reasonable value for the safety bytes is, if
  // needed compute it dynamically
  SoundOutput.SafetyBytes =
      (((SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample)) /
       GameUpdateHz) /
      3;

  Win32InitDSound(Window, SoundOutput.SamplesPerSecond,
                  SoundOutput.SecondaryBufferSize);
  Win32ClearSoundBuffer(&SoundOutput);
  GlobalSecondaryAudioBuffer->Play(0, 0, DSBPLAY_LOOPING);

#if 0
  // NOTE: This prints the PlayCursor/WriteCursor update frequency for handy information
  while (true) {
    DWORD PlayCursor;
    DWORD WriteCursor;
    GlobalSecondaryAudioBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
    char StringBuffer[256];
    sprintf_s(StringBuffer, "%u, %u\n", PlayCursor, WriteCursor);
    OutputDebugStringA(StringBuffer);
  }
#endif

  int16 *Samples =
      (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#if HANDMADE_INTERNAL
  LPVOID BaseAddress = (void *)Terabytes(2);
#else
  LPVOID BaseAddress = 0;
#endif

  win32_state Win32State = {};
  Win32State.InputRecordingIndex = 0;
  Win32State.InputPlayingIndex = 0;

  game_memory GameMemory = {};

  GameMemory.IsInitialized = false;
  GameMemory.PermanentStorageSize = Megabytes(64);
  GameMemory.TransientStorageSize = Gigabytes(4);
  GameMemory.DEBUGPlatformFreeFileMemory = &DEBUGPlatformFreeFileMemory;
  GameMemory.DEBUGPlatformReadEntireFile = &DEBUGPlatformReadEntireFile;
  GameMemory.DEBUGPlatformWriteEntireFile = &DEBUGPlatformWriteEntireFile;

  Win32State.TotalSize =
      GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;

  Win32State.GameMemoryBlock =
      (void *)VirtualAlloc(BaseAddress, Win32State.TotalSize,
                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

  GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
  GameMemory.TransientStorage =
      ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

  if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage) {

    game_input Input[2] = {};
    game_input *NewInput = &Input[0];
    game_input *OldInput = &Input[1];

    uint64 LastCycleCount = __rdtsc();
    LARGE_INTEGER LastCounter = Win32GetWallClock();
    LARGE_INTEGER FlipWallClock = Win32GetWallClock();

    int DebugTimeMarkerIndex = 0;
    win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {};

    DWORD AudioLatencyBytes = 0;
    real32 AudioLatencySeconds = 0.0f;
    bool SoundIsValid = false;

    win32_game_code Game =
        Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);

    while (GlobalRunning) {
      FILETIME NewDLLWriteTime =
          Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
      if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) == 1) {
        Win32UnloadGameCode(&Game);
        Game = Win32LoadGameCode(SourceGameCodeDLLFullPath,
                                 TempGameCodeDLLFullPath);
      }

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

      Win32ProcessPendingMessages(&Win32State, NewKeyboardController);

      if (!GlobalPause) {
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

            // TODO: See if ControllerState.dwPacketNumber increments too
            // rapidly
            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

            /*
             * TODO: This is handling a "square" deadzone. check XInput to
             * verify that the deadzone is "round". If yes, handle round
             * deadzone.
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

            Win32ProcessXInputDigitalButton(
                Pad->wButtons, &OldController->Start, XINPUT_GAMEPAD_START,
                &NewController->Start);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back,
                                            XINPUT_GAMEPAD_BACK,
                                            &NewController->Back);

          } else {
            NewController->IsConnected = false;
          }
        }

        game_offscreen_buffer Buffer = {};
        Buffer.Memory = GlobalBackBuffer.Memory;
        Buffer.Width = GlobalBackBuffer.Width;
        Buffer.Height = GlobalBackBuffer.Height;
        Buffer.Pitch = GlobalBackBuffer.Pitch;
        Buffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;

        if (Win32State.InputRecordingIndex) {
          Win32RecordInput(&Win32State, NewInput);
        }

        if (Win32State.InputPlayingIndex) {
          Win32PlayBackInput(&Win32State, NewInput);
        }

        Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);

        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
        real32 FromBeginToAudioSeconds =
            Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

        DWORD PlayCursor;
        DWORD WriteCursor;
        if (SUCCEEDED(GlobalSecondaryAudioBuffer->GetCurrentPosition(
                &PlayCursor, &WriteCursor))) {
          /*
           * NOTE: The following is how sound computation works
           *
           * We define a safety value, which is the number of samples the game
           * update loop might vary by (ex: could vary by +/- 2ms)
           *
           * When we plan to write the audio, we'll look and note the current
           * play cursor position, and estimate where it'll be during the next
           * frame boundary.
           *
           * To determine if the audio is low latency or high latency, we'll
           * check to see if the current write cursor is before that boundary +
           * the safety value. If it is, then it's a low latency audio
           * device/driver, else it's a high latency device/driver.
           *
           * For low latency cases, we'll write till the next immediate frame
           * boundary.
           *
           * For high latency cases, we'll not be able to sync our audio, so
           * we'll write one frame + safety value bytes worth of sound.
           */
          if (!SoundIsValid) {
            SoundOutput.RunningSampleIndex =
                WriteCursor / SoundOutput.BytesPerSample;
            SoundIsValid = true;
          }

          DWORD ByteToLock =
              (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
              SoundOutput.SecondaryBufferSize;

          DWORD ExpectedSoundBytesPerFrame =
              (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) /
              GameUpdateHz;
          real32 ExpectedSecondsLeftUntilFlip =
              (TargetSecondsPerFrame - FromBeginToAudioSeconds);
          DWORD ExpectedBytesUntilFlip =
              (DWORD)((ExpectedSecondsLeftUntilFlip / TargetSecondsPerFrame) *
                      ExpectedSoundBytesPerFrame);
          DWORD ExpectedFrameBoundaryByte =
              PlayCursor + ExpectedSoundBytesPerFrame;

          DWORD SafeWriteCursor = WriteCursor + SoundOutput.SafetyBytes;
          if (SafeWriteCursor < PlayCursor) {
            SafeWriteCursor += SoundOutput.SecondaryBufferSize;
          }
          bool AudioCardIsLowLatency =
              (SafeWriteCursor < ExpectedFrameBoundaryByte);

          DWORD TargetCursor = 0;
          if (AudioCardIsLowLatency) {
            TargetCursor =
                ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;
          } else {
            TargetCursor = WriteCursor + ExpectedSoundBytesPerFrame +
                           SoundOutput.SafetyBytes;
          }
          TargetCursor = TargetCursor % SoundOutput.SecondaryBufferSize;

          // TODO: Validate this assumptions
          DWORD BytesToWrite = 0;
          if (ByteToLock > TargetCursor) {
            BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
            BytesToWrite += TargetCursor;
          } else {
            BytesToWrite = TargetCursor - ByteToLock;
          }

          game_sound_output_buffer SoundBuffer = {};
          SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
          SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
          SoundBuffer.Samples = Samples;

          Game.GetSoundSamples(&GameMemory, &SoundBuffer);

#if HANDMADE_INTERNAL
          win32_debug_time_marker *Marker =
              &DebugTimeMarkers[DebugTimeMarkerIndex];
          Marker->OutputPlayCursor = PlayCursor;
          Marker->OutputWriteCursor = WriteCursor;
          Marker->OutputLocation = ByteToLock;
          Marker->OutputByteCount = BytesToWrite;
          Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;
          DWORD UnwrappedWriteCursor = WriteCursor;
          if (WriteCursor < PlayCursor) {
            WriteCursor += SoundOutput.SecondaryBufferSize;
          }

          AudioLatencyBytes = WriteCursor - PlayCursor;
          AudioLatencySeconds = (((real32)AudioLatencyBytes /
                                  (real32)SoundOutput.BytesPerSample) /
                                 (real32)SoundOutput.SamplesPerSecond);

          // NOTE: Custom Debug Code For % Cursor Positions
          char TextBuffer[256];
          sprintf_s(TextBuffer,
                    "BTL:%u TC:%u BTW:%u - PC:%u WC:%u DELTA:%u (%fs)\n",
                    ByteToLock, TargetCursor, BytesToWrite, PlayCursor,
                    WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
          OutputDebugStringA(TextBuffer);
#endif
          Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite,
                               &SoundBuffer);

        } else {
          SoundIsValid = false;
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

          real32 TestSecondsElapsedForFrame =
              Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
          if (TestSecondsElapsedForFrame < TargetSecondsPerFrame) {
            // TODO: LOG MISSED FRAMES here
          }

          while (SecondsElapsedForFrame < TargetSecondsPerFrame) {
            SecondsElapsedForFrame =
                Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
          }
        } else {
          // TODO: MISSED FRAME RATE!
          // TODO: Logging
        }

        LARGE_INTEGER EndCounter = Win32GetWallClock();

        uint64 EndCycleCount = __rdtsc();
        int64 CyclesElapsed = EndCycleCount - LastCycleCount;

        FlipWallClock = Win32GetWallClock();
        real32 MSPerFrame =
            1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);

        HDC DeviceContext = GetDC(Window);
        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if HANDMADE_INTERNAL
        // TODO: this is wrong on the 0th index
        Win32DebugSyncDisplay(&GlobalBackBuffer, ArrayCount(DebugTimeMarkers),
                              DebugTimeMarkers, DebugTimeMarkerIndex - 1,
                              &SoundOutput, TargetSecondsPerFrame);
#endif
        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
                                   Dimension.Width, Dimension.Height);
        ReleaseDC(Window, DeviceContext);

#if HANDMADE_INTERNAL
        // NOTE: This is debug code
        {
          DWORD PlayCursor = 0;
          DWORD WriteCursor = 0;
          if (SUCCEEDED(GlobalSecondaryAudioBuffer->GetCurrentPosition(
                  &PlayCursor, &WriteCursor))) {
            Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
            win32_debug_time_marker *Marker =
                &DebugTimeMarkers[DebugTimeMarkerIndex];
            Marker->FlipPlayCursor = PlayCursor;
            Marker->FlipWriteCursor = WriteCursor;
          }
        }
#endif

        // TODO: Should I clear these here?
        game_input *Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;

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
#if HANDMADE_INTERNAL
        DebugTimeMarkerIndex++;
        if (DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers)) {
          DebugTimeMarkerIndex = 0;
        }
#endif
      }
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
