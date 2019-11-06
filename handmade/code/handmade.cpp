#include "handmade.h"

internal void GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz)
{
    local_persist real32 tSine;
    int16 ToneVolume = 3000;
    //int ToneHz = 256;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;
    for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        real32 SinValue = sinf(tSine);
        int16 SampleValue = (int16)(SinValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        tSine += 2.0f * Pi32 / (real32)WavePeriod;
    }
}

internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    // TODO: Let's see what the optimizer does.
    uint8 *Row = (uint8 *)Buffer->Memory; // Individual pixel component (R, G, B, Padding) pointer

    for (int y = 0; y < Buffer->Height; y++)
    {
        uint32 *Pixel = (uint32 *)Row; // Full pixel (R + G + B + Padding) pointer
        for (int x = 0; x < Buffer->Width; x++)
        {
            /*
				Little Endian Architecture
                In memory:		BB GG RR xx
				In register:	xx RR GG BB (Windows has defined RGB to be in order in the register)
            */
            uint8 Blue = (x + BlueOffset);
            uint8 Green = (y + GreenOffset);

            *Pixel++ = (Green << 8) | Blue;
        }
        Row += Buffer->Pitch;
    }
}

internal void GameUpdateAndRender(
    game_memory *Memory,
    game_input* Input,
    game_offscreen_buffer *Buffer,
    game_sound_output_buffer *SoundBuffer)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if(!Memory->IsInitialized)
    {
        char *Filename = __FILE__;
        debug_read_file_result File = DEBUG_PlatformReadEntireFile(Filename);
        if(File.Contents)
        {
            DEBUG_PlatformWriteEntireFile("test.out", File.ContentSize, File.Contents);
            DEBUG_PlatformFreeFileMemory(File.Contents);
        }
        

        GameState->ToneHz = 256;
        // TODO: This may be more appropiate to do in the platform layer.
        Memory->IsInitialized = true;
    }

    game_controller_input *Input0 = &Input->Controllers[0];
    if(Input0->IsAnalog)
    {
        // NOTE: Use analog movement tuning.
        GameState->ToneHz = 256 + (int)(128.0f * Input0->EndY);
        GameState->BlueOffset += (int)(4.0f * Input0->EndX);
    }
    else
    {
        // NOTE: Use digital movement tuning.
    }

    // Input.AButtonEndedDown;
    // Input.AButtonHalfTransitionCount;
    if(Input0->Down.EndedDown)
    {
        GameState->GreenOffset++;
    }

    // TODO: Allow sample offsets here for more robust platform options.
    GameOutputSound(SoundBuffer, GameState->ToneHz);
    RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}