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

internal debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename);
internal void DEBUGPlatformFreeFileMemory(void *Memory);

internal bool DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize,
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
  bool IsConnected;
  bool IsAnalog;
  real32 StickAverageX;
  real32 StickAverageY;

  union {
    game_button_state Buttons[13];
    struct {
      game_button_state MoveUp;
      game_button_state MoveDown;
      game_button_state MoveLeft;
      game_button_state MoveRight;

      game_button_state ActionUp;
      game_button_state ActionDown;
      game_button_state ActionLeft;
      game_button_state ActionRight;

      game_button_state LeftShoulder;
      game_button_state RightShoulder;

      game_button_state Back;
      game_button_state Start;

      /*
       * WARNING: All buttons must be added above the Terminator button
       * It is used to indicate the last button and used to assert memory size
       */
      game_button_state Terminator;
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
  game_controller_input Controllers[5];
};

inline game_controller_input *GetController(game_input *Input,
                                            int ControllerIndex) {
  Assert(ControllerIndex < ArrayCount(Input->Controllers));
  game_controller_input *Result = &Input->Controllers[ControllerIndex];

  return Result;
}

internal void GameUpdateAndRender(game_memory *Memory, game_input *Input,
                                  game_offscreen_buffer *Buffer,
                                  game_sound_output_buffer *SoundBuffer);

#endif
