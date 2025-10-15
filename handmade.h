#ifndef HANDMADE_H
#define HANDMADE_H

#include "common_used_defs.h"

// TODO: Services that the platform layer provides to the game
#if HANDMADE_INTERNAL
/*
 * CAUTION: The below File I/O is only mean to be used to unblock development
 *   and during development/debugging. This is blocking and should be replaced
 *   with a async non-blocking I/O for production use. The current
 *   implementation doesn't also handle corrupt writes, missing directory ...
 *   etc
 */
struct debug_read_file_result {
  uint32 ContentSize;
  void *Contents;
};

debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename);
void DEBUGPlatformFreeFileMemory(void *Memory);

bool DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize,
                                  void *Memory);
#endif

// TODO: In the future, rendering specifically will become a three-tiered
// abstraction!!
struct game_offscreen_buffer {
  // NOTE: Each Pixel is 32-bits wide. Memory Order is BB GG RR XX.
  void *Memory;
  int Width;
  int Height;
  int Pitch;
};

struct game_sound_output_buffer {
  int SamplesPerSecond;
  int SampleCount;
  int16 *Samples;
};

struct game_button_state {
  int HalfTransitionCount;
  bool EndedDown;
};

struct game_controller_input {
  bool IsAnalog;

  real32 StartX;
  real32 StartY;

  real32 MinX;
  real32 MinY;

  real32 MaxX;
  real32 MaxY;

  real32 EndX;
  real32 EndY;

  union {
    game_button_state Buttons[6];
    struct {
      game_button_state Up;
      game_button_state Down;
      game_button_state Left;
      game_button_state Right;
      game_button_state LeftShoulder;
      game_button_state RightShoulder;
    };
  };
};

struct game_memory {
  bool IsInitialized;
  uint64 PermanentStorageSize;
  void *PermanentStorage;
  uint64 TransientStorageSize;
  void *TransientStorage;
};

struct game_state {
  int ToneHz;
  int BlueOffset;
  int GreenOffset;
};

struct game_input {
  // TODO: Insert clock values here
  game_controller_input Controllers[4];
};

internal void GameUpdateAndRender(game_memory *Memory, game_input *Input,
                                  game_offscreen_buffer *Buffer,
                                  game_sound_output_buffer *SoundBuffer);

#endif
