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
            uint8 Blue = (uint8)(x + BlueOffset);
            uint8 Green = (uint8)(y + GreenOffset);

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
    Assert(
        &Input->Controllers[0].Terminator -
        &Input->Controllers[0].Buttons[0] ==
        ArrayCount(Input->Controllers[0].Buttons)
    );
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

    for(int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        if(Controller->IsAnalog)
        {
            // NOTE: Use analog movement tuning.
            GameState->BlueOffset += (int)(4.0f * Controller->StickAverageX);
            GameState->ToneHz = 256 + (int)(128.0f * Controller->StickAverageY);
        }
        else
        {
            // NOTE: Use digital movement tuning.
            if(Controller->MoveLeft.EndedDown)
            {
                GameState->BlueOffset += 1;
            }
            if(Controller->MoveRight.EndedDown)
            {
                GameState->BlueOffset -= 1;
            }

        }

        // Input.AButtonEndedDown;
        // Input.AButtonHalfTransitionCount;
        if(Controller->ActionDown.EndedDown)
        {
            GameState->GreenOffset++;
        }
    }

    // TODO: Allow sample offsets here for more robust platform options.
    GameOutputSound(SoundBuffer, GameState->ToneHz);
    RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}