#include "handmade.h"
#include <math.h>

internal void GameSoundOutput(game_state *GameState,
                              game_sound_output_buffer *SoundBuffer,
                              int ToneHz) {
  int16 ToneVolume = 2000;
  int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

  int16 *SampleOut = SoundBuffer->Samples;
  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount;
       SampleIndex++) {
#if 0
    real32 SineValue = sinf(GameState->tSine);
    int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
    int16 SampleValue = 0;
#endif
    *SampleOut++ = SampleValue;
    *SampleOut++ = SampleValue;

#if 0
    GameState->tSine += ((2.0f * PI32) / ((real32)WavePeriod));

    if (GameState->tSine > (2.0f * PI32)) {
      GameState->tSine -= (2.0f * PI32);
    }
#endif
  }
}

inline internal int32 RoundReal32ToInt32(real32 Real32) {
  int32 Result = (int32)(Real32 + 0.5f);

  // TODO: Is there an intristic that can be used?
  return Result;
}
/**
 * TODO: Handle floating point color, and color checks
 */
internal void DrawRectangle(game_offscreen_buffer *Buffer, real32 RealMinX,
                            real32 RealMinY, real32 RealMaxX, real32 RealMaxY,
                            uint32 Color) {
  int32 MinX = RoundReal32ToInt32(RealMinX);
  int32 MaxX = RoundReal32ToInt32(RealMaxX);
  int32 MinY = RoundReal32ToInt32(RealMinY);
  int32 MaxY = RoundReal32ToInt32(RealMaxY);

  if (MinX < 0) {
    MinX = 0;
  }
  if (MinY < 0) {
    MinY = 0;
  }
  if (MaxX > Buffer->Width) {
    MaxX = Buffer->Width;
  }
  if (MaxY > Buffer->Height) {
    MaxY = Buffer->Height;
  }

  uint8 *EndOfBuffer =
      (uint8 *)Buffer->Memory + (Buffer->Pitch * Buffer->Height);

  uint8 *Row = ((uint8 *)Buffer->Memory + (MinX * Buffer->BytesPerPixel) +
                (MinY * Buffer->Pitch));
  for (int Y = MinY; Y < MaxY; Y++) {
    uint32 *Pixel = (uint32 *)Row;
    for (int X = MinX; X < MaxX; X++) {
      *Pixel++ = Color;
    }
    Row += Buffer->Pitch;
  }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert(
      (&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
      (ArrayCount(Input->Controllers[0].Buttons) - 1));
  Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
  game_state *GameState = (game_state *)Memory->PermanentStorage;
  if (!Memory->IsInitialized) {
    // TODO: Should this be done in the platform layer instead?
    Memory->IsInitialized = true;
  }

  for (int ControllerIndex = 0;
       ControllerIndex < ArrayCount(Input->Controllers); ControllerIndex++) {
    game_controller_input *Controller = GetController(Input, ControllerIndex);
    if (Controller->IsAnalog) {
      // TODO: Use analog movement tuning
    } else {
      // TODO: Use digital movement tuning
    }
  }
  DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width,
                (real32)Buffer->Height, (uint32)0xFFFF00FF);
  DrawRectangle(Buffer, 10.0f, 10.0f, 30.f, 30.f, (uint32)0xFF0000FF);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
  game_state *GameState = (game_state *)Memory->PermanentStorage;
  GameSoundOutput(GameState, SoundBuffer, 262);
}

/*
internal void RenderTestGradient(game_offscreen_buffer *Buffer, int BlueOffset,
                                 int GreenOffset) {
  uint8 *Row = (uint8 *)Buffer->Memory;
  for (int Y = 0; Y < Buffer->Height; Y++) {

    uint32 *Pixel = (uint32 *)Row;
    for (int X = 0; X < Buffer->Width; X++) {
      uint8 Blue = (uint8)(X + BlueOffset);
      uint8 Green = (uint8)(Y + GreenOffset);

      *Pixel++ = ((Green << 8) | Blue);
    }
    Row += Buffer->Pitch;
  }
}
*/
