#ifndef WIN32_HANDMADE_H
#define WIN32_HANDMADE_H

#include "common_used_defs.h"
#include "handmade.h"
#include <windows.h>
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
  int WriteAheadSamples;
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
  game_update_and_render *UpdateAndRender;
  game_get_sound_samples *GetSoundSamples;
  bool IsValid;
};

#endif
