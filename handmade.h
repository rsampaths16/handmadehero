#ifndef HANDMADE_H
#define HANDMADE_H

#include "common_used_defs.h"

// TODO: Services that the platform layer provides to the game

// TODO: In the future, rendering specifically will become a three-tiered
// abstraction!!
struct game_offscreen_buffer {
  // NOTE: Each Pixel is 32-bits wide. Memory Order is BB GG RR XX.
  void *Memory;
  int Width;
  int Height;
  int Pitch;
};

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset,
                                  int GreenOffset);
#endif
