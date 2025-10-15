#include "handmade.h"
#include <math.h>

internal void GameSoundOutput(game_sound_output_buffer *SoundBuffer,
                              int ToneHz) {
  local_variable real32 tSine;
  int16 ToneVolume = 2000;
  int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

  int16 *SampleOut = SoundBuffer->Samples;
  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount;
       SampleIndex++) {
    real32 SineValue = sinf(tSine);
    int16 SampleValue = (int16)(SineValue * ToneVolume);
    *SampleOut++ = SampleValue;
    *SampleOut++ = SampleValue;

    tSine += ((2.0f * PI32) / ((real32)WavePeriod));
  }
}

internal void RenderTestGradient(game_offscreen_buffer *Buffer, int BlueOffset,
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

internal void GameUpdateAndRender(game_input *Input,
                                  game_offscreen_buffer *Buffer,
                                  game_sound_output_buffer *SoundBuffer) {
  local_variable int BlueOffset = 0;
  local_variable int GreenOffset = 0;
  local_variable int ToneHz = 262;

  game_controller_input *Input0 = &Input->Controllers[0];
  if (Input0->IsAnalog) {
    // TODO: Use analog movement tuning
    BlueOffset -= (int)(4.0f * (Input0->EndX));
    ToneHz = 262 + (int)(128.0f * (Input0->EndY));
  } else {
    // TODO: Use digital movement tuning
  }

  if (Input0->Down.EndedDown) {
    GreenOffset += 1;
  }

  GameSoundOutput(SoundBuffer, ToneHz);
  RenderTestGradient(Buffer, BlueOffset, GreenOffset);
}
