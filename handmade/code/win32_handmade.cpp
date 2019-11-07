// INFO: Watch lectures by Chandler Carruth.

// TODO: swap, min, max ... macros?

/* TODO: THIS IS NOT A FINAL PLATFORM LAYER!!!
    - Saved game locations
    - Getting a handle to our own exectuable file
    - Asset loading path
    - Threading (launch a thread)
    - Raw input (support for multiple keyboards)
    - Sleep/timeBeginPeriod
    - ClipCursor() (for multimonitor support)
    - WM_SETCURSOR (control cursor visibility)
    - QueryCancelAutoplay
    - WM_ACTIVATEAPP (BitBlt)
    - Hardware acceleration (OpenGL or Direct3D or both?)
    - GetKeyboardLayout (for Frencch keyboards, international WASD)

    Just a partial list of stuff!!
*/

#include <stdint.h>

// TODO: Implement sinf ourselves.
#include <math.h>
#include <malloc.h>
#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>

#include "handmade.cpp"
#include "win32_handmade.h"

/* ==========================================================================================================
   XInput function pointers and loading
   --------------------------------------------------------------------------------------------------------*/
// Declare macro for creating function prototype.
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)

// Define a type for the prototypes so they can be used as pointers.
typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

// Define function stubs to be used by default.
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

// Create global function pointers to the stub functions
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub; // & operator implied for functions.
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub; // & operator implied for functions.

// Redefine function to use the function pointers.
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

inline uint32 SafeTruncateUInt64(uint64 Value)
{
    // TODO: Defines for maximum value UInt32MAx
    Assert(Value <= 0xFFFFFFFF);
    return (uint32)Value;
}

internal debug_read_file_result DEBUG_PlatformReadEntireFile(char *FileName)
{
    debug_read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if(GetFileSizeEx(FileHandle, &FileSize))
        {
            uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if(Result.Contents)
            {
                DWORD BytesRead;
                if(ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead))
                {
                    // NOTE: File read successfully
                    Result.ContentSize = FileSize32;
                }
                else
                {
                    DEBUG_PlatformFreeFileMemory(Result.Contents);
                    Result.Contents = 0;
                }
            }
            else
            {
                // TODO: Logging
            }
        }
        else
        {
            // TODO: Logging
        }
        CloseHandle(FileHandle);
    }
    else
    {
        // TODO: Logging
    }

    return Result;
}
internal bool32 DEBUG_PlatformWriteEntireFile(char *FileName, uint32 MemorySize, void *Memory)
{
    bool32 Result = false;

    HANDLE FileHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if(WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            // NOTE: File read successfully
            Result = (MemorySize == BytesWritten);
        }
        else
        {
        }
        CloseHandle(FileHandle);
    }
    else
    {
        // TODO: Logging
    }

    return Result;
}

internal void DEBUG_PlatformFreeFileMemory(void *Memory)
{
    if(Memory)
    {
        VirtualFree(Memory, 0,  MEM_RELEASE);
    }
}

internal void Win32LoadXInput(void)
{
    // TODO: Test this on Windows 8.
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");

    if (!XInputLibrary)
    {
        // TODO: Diagnostic
        XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
    }

    if (!XInputLibrary)
    {
        // TODO: Diagnostic
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }

    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");

        if (!XInputGetState)
        {
            XInputGetState = XInputGetStateStub; /* TODO: Diagnostic */
        }
        if (!XInputSetState)
        {
            XInputSetState = XInputSetStateStub; /* TODO: Diagnostic */
        }
    }
    else
    {
        // TODO: Diagnostic
    }
}
/* ------------------------------------------------------------------------------------------------------*/

// Todo: This is a global for now.
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    // NOTE: Load the library.
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if(!DSoundLibrary)
    {
        // TODO: Logging and diagnostic
        return;
    }

    direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
    IDirectSound *DirectSound;

    if(!DirectSoundCreate || DS_OK != DirectSoundCreate(0, &DirectSound, 0))
    {
        // TODO: Logging and Diagnostic
        return;
    }

    WAVEFORMATEX WaveFormat = {};
    WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    WaveFormat.nChannels = 2;
    WaveFormat.nSamplesPerSec = SamplesPerSecond;
    WaveFormat.wBitsPerSample = 16;
    WaveFormat.nBlockAlign = WaveFormat.nChannels * WaveFormat.wBitsPerSample / 8;
    WaveFormat.nAvgBytesPerSec = WaveFormat.nBlockAlign * WaveFormat.nSamplesPerSec;
    WaveFormat.cbSize = 0;

    if (DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY) == DS_OK)
    {
        // TODO: DSBCAPS_GLOBALFOCUS?
        DSBUFFERDESC BufferDescription = {};
        BufferDescription.dwSize = sizeof(BufferDescription);
        BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

        // NOTE: Create a primary buffer.
        LPDIRECTSOUNDBUFFER PrimaryBuffer;
        HRESULT CSBResult = DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0);
        if (CSBResult == DS_OK || CSBResult == DS_NO_VIRTUALIZATION)
        {
            // TODO: DS_NO_VIRTUALIZATION Diagnostic and log.
            if (PrimaryBuffer->SetFormat(&WaveFormat) == DS_OK)
            {
                // NOTE: We have finally set the format.
                OutputDebugStringA("Primary buffer format was set.\n");
            }
            else
            {
                // TODO: Diagnostic
            }
        }
        else
        {
            // TODO: Diagnostic
        }
    }
    else
    {
        // TODO: Diagnostic
    }

    // NOTE: Might or might not want DSBCAPS_GETCURRENTPOSITION2 flag.
    DSBUFFERDESC BufferDescription = {};
    BufferDescription.dwSize = sizeof(BufferDescription);
    BufferDescription.dwFlags = 0;
    BufferDescription.dwBufferBytes = BufferSize;
    BufferDescription.lpwfxFormat = &WaveFormat;

    // NOTE: Create a secondary buffer.
    HRESULT CSBResult = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
    if (CSBResult == DS_OK || CSBResult == DS_NO_VIRTUALIZATION)
    {
        // TODO: DS_NO_VIRTUALIZATION Diagnostic and log.
        OutputDebugStringA("Secondary buffer was created.\n");
    }
    else
    {
        // TODO: Diagnostic
    }
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result;
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    // TODO: Bulletproof this. Maybe don't free first, free after, then free first if that fails.

    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Width;
    Buffer->Info.bmiHeader.biHeight = -Height; // NOTE: Negative for top-down DIB with upper-left origin
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    // NOTE: Align on 4 byte boundaries.
    int BitmapMemorySize = (Width * Height) * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width * Buffer->BytesPerPixel;

    // TODO: Probably clear this to black.
}

internal void Win32DisplayBufferInWindow(
    win32_offscreen_buffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    // TODO: Aspect ratio correction.
    StretchDIBits(
        DeviceContext,
        /*X, Y, Width, Height,
        X, Y, Width, Height,*/
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer->Width, Buffer->Height,
        Buffer->Memory,
        &Buffer->Info,
        DIB_RGB_COLORS, SRCCOPY);
}

internal LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch (Message)
    {
        /*
        case WM_SIZE:
        {
        } break;
        */

    case WM_CLOSE:
    {
        // Todo: Handle this with a message to the user?
        GlobalRunning = false;
    }
    break;

    case WM_ACTIVATEAPP:
    {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    }
    break;

    case WM_DESTROY:
    {
        // Todo: Handle this as an error - recreate window?
        GlobalRunning = false;
    }
    break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        Assert(!"Keyboard input came in through a non-dispath message!");
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        //int X = Paint.rcPaint.left;
        //int Y = Paint.rcPaint.top;
        //int Width = Paint.rcPaint.right - Paint.rcPaint.left;
        //int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);

        EndPaint(Window, &Paint);
    }
    break;

    default:
    {
        Result = DefWindowProcA(Window, Message, WParam, LParam);
    }
    break;
    }

    return Result;
}

internal void Win32ClearBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    if (GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0) == DS_OK)
    {
        uint8 *DestSample = (uint8 *)Region1;
        for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        DestSample = (uint8 *)Region2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void Win32ProcessXInputDigitalButton(
    DWORD XInputButtonState, game_button_state *OldState, DWORD ButtonBit, game_button_state *NewState)
{
    NewState->EndedDown = (XInputButtonState & ButtonBit) == ButtonBit;
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
}

internal void Win32FillSoundBuffer(
    win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    HRESULT Result = GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0);

    if (DS_OK != Result)
    {
        return;
    }
    //OutputDebugStringA("Locked GlobalSecondaryBuffer \n");
    // TODO: assert that Region1Size/Region2Size is valid

    // NOTE: buffer layout
    //  int16 int16   int16 int16   ...
    // [LEFT  RIGHT] [LEFT  RIGHT] [LEFT RIGHT]

    // Fill in region 1
    DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
    int16 *DestSample = (int16 *)Region1;
    int16 *SourceSample = SourceBuffer->Samples;
    for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
    {
        *DestSample++ = *SourceSample++;
        *DestSample++ = *SourceSample++;
        ++SoundOutput->RunningSampleIndex;
    }

    // Fill in region 2
    DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
    DestSample = (int16 *)Region2;
    for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
    {
        *DestSample++ = *SourceSample++;
        *DestSample++ = *SourceSample++;
        ++SoundOutput->RunningSampleIndex;
    }

    GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    WNDCLASSA WindowClass = {}; // = {} to initialize everything to zero.
    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if(!RegisterClassA(&WindowClass))
    {
        // TODO: Logging
        return 0;
    }

    HWND Window = CreateWindowA(
        WindowClass.lpszClassName,
        "Handmade Hero",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        Instance,
        0);

    if(!Window) {
        // TODO: Logging
        return 0;
    }

    // NOTE: Since we specified CS_OWNDC, we can just get one device context and use it forever
    // since we aren't sharing it with anyone.
    HDC DeviceContext = GetDC(Window);
    Win32LoadXInput();

    win32_sound_output SoundOutput = {};
    SoundOutput.SamplesPerSecond = 48000;
    SoundOutput.RunningSampleIndex = 0;
    SoundOutput.BytesPerSample = sizeof(int16) * 2;
    SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
    SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
    Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
    Win32ClearBuffer(&SoundOutput);
    GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
    // TODO: Pool with bitmap alloc
    int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
    LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
    LPVOID BaseAddress = 0;
#endif

    game_memory GameMemory = {};
    GameMemory.PermanentStorageSize = Megabytes(64);
    GameMemory.TransientStorageSize = Gigabytes(1);

    uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
    GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

    if(!Samples || !GameMemory.PermanentStorage || !GameMemory.TransientStorage)
    {
        // TODO: Logging
        return 0;
    }

    // NOTE: Performance tracking.
    LARGE_INTEGER PerfCounterFrequencyResult;
    QueryPerformanceFrequency(&PerfCounterFrequencyResult);
    int64 PerfCounterFrequency = PerfCounterFrequencyResult.QuadPart;
    LARGE_INTEGER LastCounter;
    QueryPerformanceCounter(&LastCounter);
    uint64 LastCycleCount = __rdtsc();

    game_input Input[2] = {};
    game_input *NewInput = &Input[0];
    game_input *OldInput = &Input[1];

    GlobalRunning = true;
    while (GlobalRunning)
    {
        MSG Message;

        game_controller_input *KeyboardController = &NewInput->Controllers[0];
        // TODO: Zeroing macro
        // TODO: We can't zero everything because the up/down state will be wrong.
        game_controller_input ZeroController = {};
        *KeyboardController = ZeroController;

        while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
        {
            if (Message.message == WM_QUIT)
            {
                GlobalRunning = false;
            }

            switch(Message.message)
            {
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                case WM_KEYDOWN:
                case WM_KEYUP:
                {
                    uint32 VKCode = (uint32)Message.wParam;
                    bool32 AltKeyIsDown = Message.lParam & (1 << 29);
                    bool WasDown = ((Message.lParam & (1 << 30)) != 0);
                    bool IsDown = ((Message.lParam & (1 << 31)) == 0);

                    if (WasDown != IsDown)
                    {
                        if (VKCode == 'W')
                        {
                        }
                        else if (VKCode == 'A')
                        {
                        }
                        else if (VKCode == 'S')
                        {
                        }
                        else if (VKCode == 'D')
                        {
                        }
                        else if (VKCode == 'Q')
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                        }
                        else if (VKCode == 'E')
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                        }
                        else if (VKCode == VK_UP)
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Up, IsDown);
                        }
                        else if (VKCode == VK_LEFT)
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Left, IsDown);
                        }
                        else if (VKCode == VK_DOWN)
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Down, IsDown);
                        }
                        else if (VKCode == VK_RIGHT)
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Right, IsDown);
                        }
                        else if (VKCode == VK_ESCAPE)
                        {
                            GlobalRunning = false;
                        }
                        else if (VKCode == VK_SPACE)
                        {
                        }
                    }

                    if ((VKCode == VK_F4) && AltKeyIsDown)
                    {
                        GlobalRunning = false;
                    }
                } break;
                default:
                {
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                } break;
            }
        }

        // TODO: Should we poll this more frequently?
        int MaxControllerCount = XUSER_MAX_COUNT;
        if(MaxControllerCount > ArrayCount(NewInput->Controllers))
        {
            MaxControllerCount = ArrayCount(NewInput->Controllers);
        }
        for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex)
        {
            game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
            game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];

            XINPUT_STATE ControllerState;
            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
            {
                // NOTE: This controller is plugged in.
                // TODO: See if Controller.dwPacketNumber increments too rapidly.
                XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                // TODO: DPad
                bool32 Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                bool32 Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                bool32 Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                bool32 Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

                NewController->IsAnalog = true;
                NewController->StartX = OldController->EndX;
                NewController->StartY = OldController->EndY;

                // TODO: Dead zone processing.
                //       min max macros
                real32 X;
                if(Pad->sThumbLX < 0) // TODO: Collapse to single function
                {
                    X = (real32)Pad->sThumbLX / 32768.0f;
                }
                else
                {
                    X = (real32)Pad->sThumbLX / 32767.0f;
                }
                NewController->MinX = OldController->MaxX = NewController->EndX = X;

                real32 Y;
                if(Pad->sThumbLY < 0)
                {
                    Y = (real32)Pad->sThumbLY / 32768.0f;
                }
                else
                {
                    Y = (real32)Pad->sThumbLY / 32767.0f;
                }

                NewController->MinY = OldController->MaxY = NewController->EndY = Y;

                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Down, XINPUT_GAMEPAD_A, &NewController->Down);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Right, XINPUT_GAMEPAD_B, &NewController->Right);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Left, XINPUT_GAMEPAD_X, &NewController->Left);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Up, XINPUT_GAMEPAD_Y, &NewController->Up);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);

                //bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                //bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
            }
            else
            {
                // NOTE: This controller is not available.
            }
        }

        // TODO: Make sure this is guarded entirely.
        DWORD PlayCursor = 0;
        DWORD WriteCursor = 0;
        DWORD ByteToLock = 0;
        DWORD TargetCursor = 0;
        DWORD BytesToWrite = 0;
        bool32 SoundIsValid = false;
        // TODO: Tighten up sound logic so that we know where we should be writing to and can
        //       anticipate the time spend in the game update
        if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
        {
            ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
            TargetCursor = (PlayCursor + SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;

            if(ByteToLock < TargetCursor)
            {
                BytesToWrite = TargetCursor - ByteToLock;
                SoundIsValid = true;
            }
            else if (ByteToLock > TargetCursor)
            {
                BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                BytesToWrite += TargetCursor;
                SoundIsValid = true;
            }
        }
        else {
            OutputDebugStringA("GetCurrentPosition failed.");
        }

        game_sound_output_buffer SoundBuffer = {};
        SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
        SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
        SoundBuffer.Samples = Samples;

        game_offscreen_buffer Buffer = {};
        Buffer.Memory = GlobalBackBuffer.Memory;
        Buffer.Width =  GlobalBackBuffer.Width;
        Buffer.Height = GlobalBackBuffer.Height;
        Buffer.Pitch =  GlobalBackBuffer.Pitch;

        GameUpdateAndRender(&GameMemory, Input, &Buffer, &SoundBuffer);
        if(SoundIsValid)
        {
            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
        }

        win32_window_dimension Dimemsion = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimemsion.Width, Dimemsion.Height);

        uint64 EndCycleCount = __rdtsc();
        LARGE_INTEGER EndCounter;
        QueryPerformanceCounter(&EndCounter);
        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
        int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
        real32 MSPerFrame = 1000.0f * (real32)CounterElapsed / (real32)PerfCounterFrequency;
        real32 FPS = (real32)PerfCounterFrequency / (real32)CounterElapsed;
        real32 MCPF = (real32)CyclesElapsed / (1000.0f * 1000.0f);

    #if 0
        char Buffer[256];
        sprintf(Buffer, "%.02f ms/f, %.02f f/s, %.02f Mc/f\n", MSPerFrame, FPS, MCPF);
        OutputDebugStringA(Buffer);
    #endif

        LastCounter = EndCounter;
        LastCycleCount = EndCycleCount;

        game_input *Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;
        // TODO: should these be cleared here?
    }

    return 0;
}