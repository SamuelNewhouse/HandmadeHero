#if !defined(HANDMADE_H) // Include guard to avoid redeclaration

/*
HANDMADE_INTERNAL
    0 - Build for public release
    1 - Build for developer only

HANDMADE_SLOW
    0 - No slow code allowed
    1 - Slow code welcome.
*/

#if HANDMADE_SLOW
// TODO: Complete assertion macro
#define Assert(Expression) if(!(Expression)) { *(int *)0 = 0; }
#else
#define Assert(Expression)
#endif

// TODO: Should these always use 64-bit?
#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.1415926535897932384626433832795028841971693993751f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

/* ============================================================================
    NOTE: Services that the game provides to the platform layer.
*/
#if HANDMADE_INTERNAL
/*  IMPORTANT:
    This are NOT for doing anything in the shipping game - they are blocking and the write
    doesn't protect against lost data!
*/
struct debug_read_file_result
{
    uint32 ContentSize;
    void *Contents;
};
internal debug_read_file_result DEBUG_PlatformReadEntireFile(char *FileName);
internal void DEBUG_PlatformFreeFileMemory(void *Memory);

internal bool32 DEBUG_PlatformWriteEntireFile(char *FileName, uint32 MemorySize, void *Memory);
#endif
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

struct game_button_state
{
    int HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input
{
    bool32 IsConnected;
    bool32 IsAnalog;
    real32 StickAverageX;
    real32 StickAverageY;

    real32 EndX;
    real32 EndY;

    union
    {
        game_button_state Buttons[12];
        struct
        {
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

            // NOTE: Fake Button. Must be last button.
            game_button_state Terminator;
        };
    };
};

struct game_input
{
    // TODO: Insert clock values here.
    game_controller_input Controllers[5];
};

inline game_controller_input *GetController(game_input *Input, unsigned int ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    return &Input->Controllers[ControllerIndex];
}

struct game_memory
{
    bool32 IsInitialized;

    uint64 PermanentStorageSize;
    void *PermanentStorage; // NOTE: REQUIRED to be cleared to zero at startup.

    uint64 TransientStorageSize;
    void *TransientStorage; // NOTE: REQUIRED to be cleared to zero at startup.
};

internal void GameUpdateAndRender(
    game_memory *Memory,
    game_input *Input,
    game_offscreen_buffer *Buffer,
    game_sound_output_buffer *SoundBuffer);

//
//
//

struct game_state
{
    int BlueOffset;
    int GreenOffset;
    int ToneHz;
};

#define HANDMADE_H
#endif
