#if !defined(HANDMADE_H) // Include guard to avoid redeclaration

/*
    TODO: Services that the game provides to the platform layer.
*/

/*
    NOTE: Services that the game platform layer provides to the game.
    This may expand in the future - sound on seperate thread, etc.
*/

// FOUR THINGS - timing, controller/keyboard input, bitmap to use, sound to use
// TODO: In the future, rendering specifically, will become a three-tiered abstraction!!!

struct game_offscreen_buffer
{    
    void *Memory;
    int Width;
    int Height;
    int Pitch;    
};

struct game_sound_output_buffer
{
    int SamplesPerSecond;
    int SampleCount;
    int16 *Samples;
};

internal void GameUpdateAndRender(
    game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset, game_sound_output_buffer *SoundBuffer, int ToneHz);

#define HANDMADE_H
#endif
