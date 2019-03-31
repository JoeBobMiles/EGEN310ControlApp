/*
 * @file main.c
 * @author Joseph Miles <josephmiles2015@gmail.com>
 * @date 2019-03-02
 *
 * This is is the main driver file for the control appliction.
 *
 * TODO[joe]
 *  -  Figure out how to draw text to the screen.
 *  -  Limit framerate.
 *  -  Bluetooth:
 *      -  Attempt to reconnect if connection is lost.
 *      -  Indicate that we are connecting.
 *      -  Indicate that we have connected.
 *      -  Receive data via connection.
 */

// Define the Unicode macro so that Windows knows what mode we're in.
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <winerror.h>
#include <bthsdpdef.h>
#include <bluetoothapis.h>
#include <ws2bth.h>

typedef struct backbuffer_s {
    BITMAPINFO Info;
    LONG       Width;
    LONG       Height;
    int       *Memory;
} backbuffer;

// NOTE[joe] These globals are temporary!
static int ApplicationQuit;
static backbuffer BackBuffer;
static HDC DeviceContext;

static SOCKET BTSocket;

static int XOffset;
static int YOffset;
static char LEDState;

/** This function writes our "structured art" background to the backbuffer that
 * we blit to the window that Windows gives us. */
static
void DrawGradient(int dX, int dY)
{
    for (unsigned int Y = 0; Y < BackBuffer.Height; Y++)
    {
        for (unsigned int X = 0; X < BackBuffer.Width; X++)
        {
            int R = 0;
            int G = ((X + dX) & 0xff);
            int B = ((Y + dY) & 0xff);

            // NOTE[joe] Pixel memory looks like XXRRGGBB.
            BackBuffer.Memory[(Y * BackBuffer.Width) + X] = (R << 16) |
                                                            (G << 8) | B;
        }
    }
}

/** This function is a convience function that gives us our backbuffer memory
 * so we don't have to think about it. */
static
void GetBackBuffer(LONG Width, LONG Height)
{
    if (BackBuffer.Memory != NULL)
        VirtualFree(BackBuffer.Memory, 0, MEM_RELEASE);

    BITMAPINFOHEADER InfoHeader = { 0 };
    InfoHeader.biSize = sizeof(BITMAPINFOHEADER);
    InfoHeader.biWidth = Width;
    /** NOTE[joe] Negative height => bitmap stored top-down.
     * If the height is positive, Windows would default to treating
     * the bitmap as if it were stored bottom-up. */
    InfoHeader.biHeight = -Height;
    InfoHeader.biBitCount = 32;
    InfoHeader.biPlanes = 1;
    InfoHeader.biSizeImage = Height * Width *
                               (InfoHeader.biBitCount / 8);
    InfoHeader.biCompression = BI_RGB;

    BackBuffer.Info.bmiHeader = InfoHeader;

    BackBuffer.Width = Width;
    BackBuffer.Height = Height;

    BackBuffer.Memory = (int *) VirtualAlloc(0,
                                             InfoHeader.biSizeImage,
                                             MEM_COMMIT | MEM_RESERVE,
                                             PAGE_READWRITE);
}

static
int UpdateSlave(const char* Data, int DataSize)
{
    return send(BTSocket, Data, DataSize, 0);
}

/** This is our window procedure, used by Windows whenever it wants us to stop
 * what we're doing and do something for it. */
LRESULT CALLBACK WindowProcedure(HWND Window,
                                 UINT Message,
                                 WPARAM wParameter,
                                 LPARAM lParameter)
{
    switch (Message)
    {
        case WM_SIZE:
        {
            RECT WindowRect = { 0 };
            GetWindowRect(Window, &WindowRect);

            LONG WindowWidth = WindowRect.right - WindowRect.left;
            LONG WindowHeight = WindowRect.bottom - WindowRect.top;

            GetBackBuffer(WindowWidth, WindowHeight);

            DrawGradient(XOffset, YOffset);

            StretchDIBits(DeviceContext,
                          // Destination location and size
                          0, 0, BackBuffer.Width, BackBuffer.Height,
                          // Source location and size
                          0, 0, BackBuffer.Width, BackBuffer.Height,
                          // Source memory
                          BackBuffer.Memory,
                          &BackBuffer.Info,
                          DIB_RGB_COLORS,
                          SRCCOPY);
        } break;

        case WM_PAINT:
        {
            if (!DeviceContext)
                DeviceContext = GetDC(Window);

            RECT WindowRect = { 0 };
            GetWindowRect(Window, &WindowRect);

            LONG WindowWidth = WindowRect.right - WindowRect.left;
            LONG WindowHeight = WindowRect.bottom - WindowRect.top;

            DrawGradient(XOffset, YOffset);

            StretchDIBits(DeviceContext,
                          // Destination location and size
                          0, 0, BackBuffer.Width, BackBuffer.Height,
                          // Source location and size
                          0, 0, BackBuffer.Width, BackBuffer.Height,
                          // Source memory
                          BackBuffer.Memory,
                          &BackBuffer.Info,
                          DIB_RGB_COLORS,
                          SRCCOPY);
        } break;

        case WM_KEYDOWN:
        {
            switch (wParameter)
            {
                case VK_SPACE:
                {
                    XOffset += 5;
                    LEDState = 1;
                } break;
            }
        } break;

        case WM_KEYUP:
        {
            switch (wParameter)
            {
                case VK_SPACE:
                {
                    LEDState = 0;
                } break;
            }
        } break;

        case WM_CLOSE:
        case WM_DESTROY:
        {
            ApplicationQuit = 1;
        } break;
    }

    return DefWindowProc(Window, Message, wParameter, lParameter);
}

/** This is our main entry point used by Windows to start our app. */
int WINAPI wWinMain(HINSTANCE Instance,        // Current instance handle.
                    HINSTANCE PrevInstance,    // Prev. instance handle (unused)
                    PWSTR     CommandLineArgs, // Command line arguments
                    int       ShowCommand)     // Undocumented feature
{
    const wchar_t WindowClassName[] = L"Control Application Window";

    WNDCLASS WindowClass = { 0 };
    // A pointer to the message handling procedure for this window.
    WindowClass.lpfnWndProc = WindowProcedure;
    // A pointer to the application instance this window is associated with.
    WindowClass.hInstance = Instance;
    // A pointer to the memory that stores the name of the window class.
    WindowClass.lpszClassName = WindowClassName;

    RegisterClass(&WindowClass);

    HWND Window = CreateWindowEx(0,
                                 WindowClassName,
                                 L"Control App",      // Window title.
                                 WS_OVERLAPPEDWINDOW, // Window style.
                                 CW_USEDEFAULT, CW_USEDEFAULT, // (X, Y)
                                 CW_USEDEFAULT, CW_USEDEFAULT, // Width, Height
                                 0,
                                 0,
                                 Instance,
                                 0);

    if (Window != NULL)
    {
        ShowWindow(Window, ShowCommand);

        /** This is where we get Bluetooth setup before running the application
         * in its fullest. */

        WSADATA WSAData = { 0 };
        WSAStartup(MAKEWORD(2,2), &WSAData);

        BLUETOOTH_SELECT_DEVICE_PARAMS SelectDeviceParams = { 0 };
        SelectDeviceParams.dwSize = sizeof(BLUETOOTH_SELECT_DEVICE_PARAMS);
        SelectDeviceParams.hwndParent = Window;
        SelectDeviceParams.fShowAuthenticated = 1;
        SelectDeviceParams.fShowRemembered = 1;
        SelectDeviceParams.fShowUnknown = 1;

        if (!BluetoothSelectDevices(&SelectDeviceParams))
            OutputDebugString(L"No device selected!\n");

        else
        {
            if (!SelectDeviceParams.pDevices)
                OutputDebugString(L"No devices found!\n");

            else if (SelectDeviceParams.cNumDevices != 1)
                OutputDebugString(L"Please only select one device.\n");

            else
            {
                // Output the name of the BlueTooth device we've connected to.
                OutputDebugString(SelectDeviceParams.pDevices[0].szName);

                BLUETOOTH_DEVICE_INFO_STRUCT Device =
                    SelectDeviceParams.pDevices[0];

                // TODO[joe] Maybe spin up a new thread?
                BTSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);

                SOCKADDR_BTH BTAddress = { 0 };
                BTAddress.addressFamily = AF_BTH;
                BTAddress.btAddr = Device.Address.ullLong;
                BTAddress.port = 1;

                int ErrorOccured = connect(BTSocket,
                                           (SOCKADDR *)(&BTAddress),
                                           sizeof(SOCKADDR_BTH));

                if (ErrorOccured)
                {
                    int ErrorCode = WSAGetLastError();

                    switch (ErrorCode)
                    {
                        case 10048: // WSAEADDRINUSE
                        {
                            OutputDebugString(L"Address/port is already in use!\n");
                        } break;

                        case 10049: // WSAEADDRNOTAVAIL
                        {
                            OutputDebugString(L"Address/port is not valid!\n");
                        } break;

                        case 10064: // WSAEHOSTDOWN
                        {
                            OutputDebugString(L"Host is down!\n");
                        } break;

                        default:
                        {
                            OutputDebugString(L"Erm, that was unexpected...\n");
                        } break;
                    }
                }

                else
                {
                    OutputDebugString(L"Connection successful!\n");
                }
            }
        }

        /** Begin the main GUI thread's loop. */
        while (!ApplicationQuit)
        {
            MSG Message = { 0 };
            while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            }

            UpdateSlave(&LEDState, 1);

            DrawGradient(XOffset, YOffset);

            StretchDIBits(DeviceContext,
                          // Destination location and size
                          0, 0, BackBuffer.Width, BackBuffer.Height,
                          // Source location and size
                          0, 0, BackBuffer.Width, BackBuffer.Height,
                          // Source memory
                          BackBuffer.Memory,
                          &BackBuffer.Info,
                          DIB_RGB_COLORS,
                          SRCCOPY);
        }

        WSACleanup();
    }

    return 0;
}
