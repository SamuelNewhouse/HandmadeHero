#include <windows.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

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

// Todo: This is a global for now.
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;

win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result;
}

internal void RenderWeirdGradient(win32_offscreen_buffer Buffer, int XOffset, int YOffset)
{
    // TODO: Let's see what the optimizer does.
    uint8 *Row = (uint8 *)Buffer.Memory; // Individual pixel component (R, G, B, Padding) pointer

    for(int y = 0; y < Buffer.Height; y++)
    {
        uint32 *Pixel = (uint32 *)Row; // Full pixel (R + G + B + Padding) pointer
        for(int x = 0; x < Buffer.Width; x++)
        {
            /*
				Little Endian Architecture
                In memory:		BB GG RR xx
				In register:	xx RR GG BB (Windows has defined RGB to be in order in the register)
            */
            uint8 Blue = (x + XOffset);
            uint8 Green = (y + YOffset);

            *Pixel++ = (Green << 8) | Blue;
        }
        Row += Buffer.Pitch;
    }
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    // TODO: Bulletproof this. Maybe don't free first, free after, then free first if that fails.

    if(Buffer->Memory)
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
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width * Buffer->BytesPerPixel;

    // TODO: Probably clear this to black.
}

internal void Win32DisplayBufferInWindow(
    HDC DeviceContext, int WindowWidth, int WindowHeight,
    win32_offscreen_buffer Buffer, int X, int Y, int Width, int Height)
{
    // TODO: Aspect ratio correction.
    StretchDIBits(
        DeviceContext,
        /*X, Y, Width, Height,
        X, Y, Width, Height,*/
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer.Width, Buffer.Height,
        Buffer.Memory,
        &Buffer.Info,
        DIB_RGB_COLORS, SRCCOPY
    );
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        /*
        case WM_SIZE:
        {
            //win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            //Win32ResizeDIBSection(&GlobalBackBuffer, Dimension.Width, Dimension.Height);
        } break;
        */

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

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackBuffer, X, Y, Width, Height);

            EndPaint(Window, &Paint);
        } break;

        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return Result;
}

int CALLBACK WinMain (HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    WNDCLASSA WindowClass = {}; // = {} to initialize everything to zero.
    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if(RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowA(
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0
        );

        if(Window)
        {
            GlobalRunning = true;
            int XOffset = 0;
            int YOffset = 0;

            while(GlobalRunning)
            {
                MSG Message;
                while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }

                RenderWeirdGradient(GlobalBackBuffer, XOffset, YOffset);
                HDC DeviceContext = GetDC(Window);

                win32_window_dimension Dimemsion = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(DeviceContext, Dimemsion.Width, Dimemsion.Height, GlobalBackBuffer, 0, 0, Dimemsion.Width, Dimemsion.Height);
                ReleaseDC(Window, DeviceContext);

                ++XOffset;
                ++YOffset;
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