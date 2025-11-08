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

struct thread_context {
  int PlaceHolder;
};

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name)                                  \
  void name(thread_context *Thread, void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name)                                  \
  debug_read_file_result name(thread_context *Thread, char *FileName)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name)                                 \
  bool name(thread_context *Thread, char *FileName, uint32 MemorySize,         \
            void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif

// TODO: In the future, rendering specifically will become a three-tiered
// abstraction!!
struct game_offscreen_buffer {
  // NOTE: Each Pixel is 32-bits wide. Memory Order is BB GG RR XX.
  void *Memory;
  int Width;
  int Height;
  int Pitch;
  int BytesPerPixel;
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
  debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
  debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
  debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
};

struct game_state {
  real32 PlayerX;
  real32 PlayerY;
};

struct game_input {
  union {
    game_button_state MouseButtons[5];
    struct {
      game_button_state MouseLeft;
      game_button_state MouseRight;
      game_button_state MouseMiddle;
      game_button_state MouseXButton1;
      game_button_state MouseXButton2;
    };
  };
  int32 MouseX, MouseY, MouseZ;

  real32 dtForFrame;
  game_controller_input Controllers[5];
};

inline game_controller_input *GetController(game_input *Input,
                                            int ControllerIndex) {
  Assert(ControllerIndex < ArrayCount(Input->Controllers));
  game_controller_input *Result = &Input->Controllers[ControllerIndex];

  return Result;
}

#define GAME_UPDATE_AND_RENDER(name)                                           \
  void name(thread_context *Thread, game_memory *Memory, game_input *Input,    \
            game_offscreen_buffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

// NOTE: The current expectation is for this function to be very fast ( < 1ms )
// TODO: Reduce the pressure on this function's performance via profiling &
// optimization
#define GAME_GET_SOUND_SAMPLES(name)                                           \
  void name(thread_context *Thread, game_memory *Memory,                       \
            game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#endif
