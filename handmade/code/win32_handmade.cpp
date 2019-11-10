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



// TODO: Implement sinf ourselves.
#include <math.h>
#include <malloc.h>
#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>

#include "handmade.cpp"
#include "win32_handmade.h"

// Todo: This is a global for now.
global_variable bool GlobalRunning;
global_variable int64 GlobalPerfCountFrequency;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

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
        case WM_CLOSE:
        {
            // Todo: Handle this with a message to the user?
            GlobalRunning = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_DESTROY:
        {
            // Todo: Handle this as an error - recreate window?
            GlobalRunning = false;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert(!"Keyboard input came in through a non-dispath message!");
        } break;

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
        } break;

        default:
        {
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
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

internal real32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
    real32 Result = 0;
    if(Value < -DeadZoneThreshold)
    {
        Result = (real32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
        //Result = (real32)Value / 32768.0f;
    }
    else if(Value > DeadZoneThreshold)
    {
        Result = (real32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));
        //Result = (real32)Value / 32767.0f;
    }
    return Result;
}

internal void Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
    Assert(NewState->EndedDown != IsDown);
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

internal void Win32ProcessPendingMessages(game_controller_input *KeyboardController)
{
    MSG Message;
    while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch(Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            } break;
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
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if (VKCode == 'A')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    }
                    else if (VKCode == 'S')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if (VKCode == 'D')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
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
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                    }
                    else if (VKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                    }
                    else if (VKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                    }
                    else if (VKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                    }
                    else if (VKCode == VK_ESCAPE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
                    else if (VKCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
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
}

inline LARGE_INTEGER Win32GetWallClock()
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = (real32)(End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCountFrequency;
    return Result;
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

    // TODO: How do we realiably query this on Windows?
    int MonitorRefreshHz = 60;
    int GameUpdateHz = MonitorRefreshHz / 2;
    real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

    game_input Input[2] = {};
    game_input *NewInput = &Input[0];
    game_input *OldInput = &Input[1];

    // NOTE: Performance tracking.
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;
    LARGE_INTEGER LastCounter = Win32GetWallClock();
    uint64 LastCycleCount = __rdtsc();

    // NOTE: Set Windows schedular granularity to 1ms so that sleep() is more granular
    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR;

    GlobalRunning = true;
    while (GlobalRunning)
    {
        // TODO: Zeroing macro
        // TODO: We can't zero everything because the up/down state will be wrong.
        game_controller_input *OldKeyboardController = GetController(OldInput, 0);
        game_controller_input *NewKeyboardController = GetController(NewInput, 0);
        *NewKeyboardController = {};
        NewKeyboardController->IsConnected = true;

        for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
        {
            NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
        }

        Win32ProcessPendingMessages(NewKeyboardController);

        // TODO: Need to not poll disconnected controllers to avoid xinput frame hit on older libraries.
        // TODO: Should we poll this more frequently?
        // NOTE: Keyboard at index 0; gamepads 1-4.
        DWORD MaxControllerCount = XUSER_MAX_COUNT + 1; // pads plus 1 keyboard.
        if(MaxControllerCount > ArrayCount(NewInput->Controllers) - 1)
        {
            MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
        }
        for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex)
        {
            // NOTE: Keyboard at index 0; gamepads 1-4.
            DWORD OurControllerIndex = ControllerIndex + 1;
            game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
            game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

            XINPUT_STATE ControllerState;
            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
            {
                NewController->IsConnected = true;
                // NOTE: This controller is plugged in.
                // TODO: See if Controller.dwPacketNumber increments too rapidly.
                XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                // TODO: This is a square deadzone, might want to do a round deadzone instead.
                NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

                if(NewController->StickAverageX != 0.0f || NewController->StickAverageY != 0.0f)
                {
                    NewController->IsAnalog = true;
                }

                // TODO: DPad
                if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                {
                    NewController->StickAverageY = 1.0f;
                    NewController->IsAnalog = false;
                }
                if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                {
                    NewController->StickAverageY = -1.0f;
                    NewController->IsAnalog = false;
                }
                if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                {
                    NewController->StickAverageX = -1.0f;
                    NewController->IsAnalog = false;
                }
                if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                {
                    NewController->StickAverageX = +1.0f;
                    NewController->IsAnalog = false;
                }

                real32 Threshold = 0.5f;

                Win32ProcessXInputDigitalButton(
                    (NewController->StickAverageX < -Threshold) ? 1 : 0,
                    &OldController->MoveLeft, 1,
                    &NewController->MoveLeft);
                Win32ProcessXInputDigitalButton(
                    (NewController->StickAverageX > Threshold) ? 1 : 0,
                    &OldController->MoveRight, 1,
                    &NewController->MoveRight);
                Win32ProcessXInputDigitalButton(
                    (NewController->StickAverageY < -Threshold) ? 1 : 0,
                    &OldController->MoveDown, 1,
                    &NewController->MoveDown);
                Win32ProcessXInputDigitalButton(
                    (NewController->StickAverageY > Threshold) ? 1 : 0,
                    &OldController->MoveUp, 1,
                    &NewController->MoveUp);

                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionDown, XINPUT_GAMEPAD_A, &NewController->ActionDown);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionRight, XINPUT_GAMEPAD_B, &NewController->ActionRight);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionLeft, XINPUT_GAMEPAD_X, &NewController->ActionLeft);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionUp, XINPUT_GAMEPAD_Y, &NewController->ActionUp);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Start, XINPUT_GAMEPAD_START, &NewController->Start);
                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back, XINPUT_GAMEPAD_BACK, &NewController->Back);

                //bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                //bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
            }
            else
            {
                // NOTE: This controller is not available.
                NewController->IsConnected = false;
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

        // TODO: Sound is wrong now, because we haven't updated it to go with the new frame loop.
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

        LARGE_INTEGER WorkCounter = Win32GetWallClock();
        real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

        real32 SecondsElapsedForFrame = WorkSecondsElapsed;

        if(SecondsElapsedForFrame < TargetSecondsPerFrame)
        {
            if(SleepIsGranular)
            {
                DWORD SleepMS = DWORD(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
                if(SleepMS > 0)
                {
                    Sleep(SleepMS);
                }
            }

            while(SecondsElapsedForFrame < TargetSecondsPerFrame)
            {
                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
            }
        }
        else
        {
            // TODO: Missed Frame Rate. Logging.            
        }

        LARGE_INTEGER EndCounter = Win32GetWallClock();
        real32 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
        LastCounter = EndCounter;

        uint64 EndCycleCount = __rdtsc();
        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
        LastCycleCount = EndCycleCount;

    #if 1
        real64 FPS = 0.0f;//(real64)GlobalPerfCountFrequency / (real64)CounterElapsed;
        real64 MCPF = (real64)CyclesElapsed / (1000.0f * 1000.0f);

        char FPSBuffer[256];
        sprintf_s(FPSBuffer, "%.02f ms/f, %.02f f/s, %.02f Mc/f\n", MSPerFrame, FPS, MCPF);
        OutputDebugStringA(FPSBuffer);
    #endif

        win32_window_dimension Dimemsion = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimemsion.Width, Dimemsion.Height);

        game_input *Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;
        // TODO: should these be cleared here?
    }

    return 0;
}