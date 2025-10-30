#ifndef WIN32_HANDMADE_H
#define WIN32_HANDMADE_H

#include "common_used_defs.h"
#include "handmade.h"
#include <windows.h>

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH

struct win32_offscreen_buffer {
  /*
   * NOTE: Each Pixel is 32-bits wide. Memory Order is BB GG RR XX.
   */
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
  int BytesPerPixel;
};

struct win32_window_dimension {
  int Width;
  int Height;
};

struct win32_sound_output {
  int SamplesPerSecond;
  uint32 RunningSampleIndex;
  int BytesPerSample;
  int SecondaryBufferSize;
  DWORD SafetyBytes;
  float tSine;
};

struct win32_debug_time_marker {
  DWORD OutputPlayCursor;
  DWORD OutputWriteCursor;
  DWORD OutputLocation;
  DWORD OutputByteCount;

  DWORD ExpectedFlipPlayCursor;
  DWORD FlipPlayCursor;
  DWORD FlipWriteCursor;
};

struct win32_game_code {
  HMODULE GameCodeDLL;
  FILETIME DLLLastWriteTime;

  // IMPORTANT: The callbacks can be NULL, and must be checked before calling
  game_update_and_render *UpdateAndRender;
  game_get_sound_samples *GetSoundSamples;
  bool IsValid;
};

struct win32_replay_buffer {
  HANDLE FileHandle;
  HANDLE MemoryMap;
  char FileName[WIN32_STATE_FILE_NAME_COUNT];
  void *MemoryBlock;
};

struct win32_state {
  uint64 TotalSize;
  void *GameMemoryBlock;
  win32_replay_buffer ReplayBuffers[4];

  HANDLE RecordingHandle;
  int InputRecordingIndex;

  HANDLE PlayBackHandle;
  int InputPlayingIndex;

  char EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
  char *OnePastLastEXEFileNameSlash;
};

#endif
