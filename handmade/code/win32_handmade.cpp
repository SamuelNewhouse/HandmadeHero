// INFO: Watch lectures by Chandler Carruth.

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

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

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

// TODO: Implement sinf ourselves.
#include <math.h>
#include <malloc.h>


#include "handmade.cpp"

#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>


struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

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

internal void Win32LoadXInput(void)
{
    // TODO: Test this on Windows 8.
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");

    if (!XInputLibrary)
    {
        // TODO: Diagnostic
        HMODULE XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
    }

    if (!XInputLibrary)
    {
        // TODO: Diagnostic
        HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
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
    if (DSoundLibrary)
    {
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        // NOTE: Get a DirectSound object.
        // NOTE: Double-check this works on XP.
        IDirectSound *DirectSound;
        if (DirectSoundCreate && DS_OK == DirectSoundCreate(0, &DirectSound, 0))
        {
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
        else
        {
            // TODO: Diagnostic
        }
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
        uint32 VKCode = WParam;
        bool32 AltKeyIsDown = LParam & (1 << 29);
        bool WasDown = ((LParam & (1 << 30)) != 0);
        bool IsDown = ((LParam & (1 << 31)) == 0);

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
            }
            else if (VKCode == VK_UP)
            {
            }
            else if (VKCode == VK_LEFT)
            {
            }
            else if (VKCode == VK_DOWN)
            {
            }
            else if (VKCode == VK_RIGHT)
            {
            }
            else if (VKCode == VK_ESCAPE)
            {
                OutputDebugStringA("ESCAPE: ");
                if (IsDown)
                {
                    OutputDebugStringA("IsDown ");
                }
                if (WasDown)
                {
                    OutputDebugStringA("WasDown ");
                }
                OutputDebugStringA("\n");
            }
            else if (VKCode == VK_SPACE)
            {
            }
        }

        if ((VKCode == VK_F4) && AltKeyIsDown)
        {
            GlobalRunning = false;
        }
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        int X = Paint.rcPaint.left;
        int Y = Paint.rcPaint.top;
        int Width = Paint.rcPaint.right - Paint.rcPaint.left;
        int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

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

struct win32_sound_output
{
    int SamplesPerSecond;
    int ToneVolume;
    int ToneHz;
    uint32 RunningSampleIndex;
    int WavePeriod;
    int BytesPerSample;
    int SecondaryBufferSize;
    real32 tSine;
    int LatencySampleCount; // How far ahead to write into buffer.
};

internal void Win32ClearBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    if (GlobalSecondaryBuffer->Lock(
        0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0) == DS_OK)
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

internal void Win32FillSoundBuffer(
    win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    if (GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0) == DS_OK)
    {
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
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    LARGE_INTEGER PerfCounterFrequencyResult;
    QueryPerformanceFrequency(&PerfCounterFrequencyResult);
    int64 PerfCounterFrequency = PerfCounterFrequencyResult.QuadPart;

    Win32LoadXInput();

    WNDCLASSA WindowClass = {}; // = {} to initialize everything to zero.
    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClassA(&WindowClass))
    {
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

        if (Window)
        {
            // NOTE: Since we specified CS_OWNDC, we can just get one device context and use it forever
            // since we aren't sharing it with anyone.
            HDC DeviceContext = GetDC(Window);

            // NOTE: Graphics test variables
            int XOffset = 0;
            int YOffset = 0;

            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.ToneVolume = 1000;
            SoundOutput.ToneHz = 262;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
            SoundOutput.BytesPerSample = sizeof(int16) * 2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;

            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            // TODO: Pool with bitmap alloc
            int16 *Samples = (int16 *)VirtualAlloc(0, 48000 * 2 * sizeof(int16), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            //int16 *Samples = (int16 *)_alloca(48000 * 2 * sizeof(int16));
            
            LARGE_INTEGER LastCounter;
            QueryPerformanceCounter(&LastCounter);
            uint64 LastCycleCount = __rdtsc();
            GlobalRunning = true;
            while (GlobalRunning)
            {
                MSG Message;

                while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }

                // TODO: Should we poll this more frequently?
                for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ControllerIndex++)
                {
                    XINPUT_STATE ControllerState;
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // NOTE: This controller is plugged in.
                        // TODO: See if Controller.dwPacketNumber increments too rapidly.
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

                        int16 StickX = Pad->sThumbLX;
                        int16 StickY = Pad->sThumbLY;

                        // TODO: do deadzone handling properly
                        XOffset += StickX / 4096;
                        YOffset += StickY / 4096;

                        if (AButton)
                        {
                            SoundOutput.ToneHz += 10;
                            SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
                        }
                        if (BButton)
                        {
                            SoundOutput.ToneHz -= 10;
                            SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
                        }
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

                    if (ByteToLock > TargetCursor)
                    {
                        BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                        BytesToWrite += TargetCursor;
                    }
                    else
                    {
                        BytesToWrite = TargetCursor - ByteToLock;
                    }
                    SoundIsValid = true;
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

                GameUpdateAndRender(&Buffer, XOffset, YOffset, &SoundBuffer, SoundOutput.ToneHz);
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

                // TODO: wsprintFA should be replaced eventually.
            #if 0
                char Buffer[256];
                sprintf(Buffer, "%.02f ms/f, %.02f f/s, %.02f Mc/f\n", MSPerFrame, FPS, MCPF);
                OutputDebugStringA(Buffer);
            #endif

                LastCounter = EndCounter;
                LastCycleCount = EndCycleCount;
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
    return 0;
}