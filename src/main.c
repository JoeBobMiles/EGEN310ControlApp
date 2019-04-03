/*
 * @file main.c
 * @author Joseph Miles <josephmiles2015@gmail.com>
 * @date 2019-03-02
 *
 * This is is the main driver file for the control appliction.
 *
 * TODO[joe]
 *  -  Limit framerate.
 *  -  Figure out latency issues.
 *      - Might be due to data loss over network.
 *  -  Bluetooth:
 *      -  Attempt to reconnect if connection is lost.
 *      -  Indicate that we are connecting.
 *      -  Indicate that we have connected.
 *      -  Receive data via connection.
 *  -  Figure out how to draw text to the screen.
 */

// Define the Unicode macro so that Windows knows what mode we're in.
#ifndef UNICODE
#define UNICODE
#endif

// Generic Windows headers.
#include <windows.h>
// Windows networking headers.
#include <winerror.h>
#include <bthsdpdef.h>
#include <bluetoothapis.h>
#include <ws2bth.h>
// Windows XInput header.
#include <xinput.h>


#define DEADZONE 100


typedef struct backbuffer_s {
    BITMAPINFO Info;
    LONG       Width;
    LONG       Height;
    int       *Memory;
} backbuffer;

typedef struct instructions_s {
    char MotorDirection;
    char ServoDirection;
    char MotorSpeed;
    char Padding;
} instructions;

// NOTE[joe] These globals are temporary!
static int ApplicationQuit;
static backbuffer BackBuffer;
static HDC DeviceContext;

static SOCKET BTSocket;

static int MaxOffset = 5;
static int XOffset;
static int YOffset;
static char MotorState;

/** Stubbing XInputGetState() */
typedef DWORD xinput_get_state(DWORD ControllerIndex, XINPUT_STATE *State);

static
DWORD XInputGetStateStub(DWORD ControllerIndex, XINPUT_STATE *State)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

static xinput_get_state *_XInputGetState = XInputGetStateStub;
#define XInputGetState _XInputGetState

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

/** This is sends the given data over our BlueTooth connection to our slave
 * device. DataSize is the size of the data in bytes. */
static
int UpdateSlave(const char* Data, int DataSize)
{
    // NOTE[joe] The packet is the data + start/end bytes and a byte for size.
    int  BufferSize = DataSize + 3;
    char Buffer[DataSize+3];

    Buffer[0] = '[';
    // NOTE[joe] Due to sending this as a char, payload is limited to 255B.
    // TODO[joe] Error if DataSize > 255?
    Buffer[1] = (char) DataSize;

    for (int i = 0; i < DataSize; i++)
        Buffer[i+2] = Data[i];

    Buffer[BufferSize-1] = ']';

    return send(BTSocket, Buffer, BufferSize, 0);
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
                } break;
            }
        } break;

        case WM_KEYUP:
        {
            switch (wParameter)
            {
                case VK_SPACE:
                {
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
            }
        }
        /** END of BlueTooth setup. */

        /** BEGIN XInput setup */

        HINSTANCE XInputHandle = LoadLibrary(L"Xinput1_4");

        if (XInputHandle == NULL)
        {
            // TODO[joe] Set flag indicating that there is no XInput?
            OutputDebugString(L"Could not load XInput1_4.dll!\n");
        }
        else
        {
            OutputDebugString(L"XInput loaded!\n");

            // Load the functions we need from the XInput DLL.
            XInputGetState = (xinput_get_state *)
                             GetProcAddress(XInputHandle, "XInputGetState");
        }

        /** END XInput setup */

        ShowWindow(Window, ShowCommand);

        /** Begin the main GUI thread's loop. */
        while (!ApplicationQuit)
        {
            // Handle Windows system messages.
            // NOTE[joe] Is this what is causing us latency issues?
            MSG Message = { 0 };
            while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            }

            instructions Instructions = { 0 };

            // Handle controller state changes for all connected controllers.
            for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
            {
                XINPUT_STATE ControllerState = { 0 };
                DWORD ConnectionStatus = XInputGetState(i, &ControllerState);

                // TODO[joe] Handle this inside a funtion?
                if (ConnectionStatus == ERROR_SUCCESS) {
                    SHORT RightStickX = ControllerState.Gamepad.sThumbRX;
                    SHORT RightStickY = ControllerState.Gamepad.sThumbRY;

                    XOffset += (int)
                               (((float)RightStickX/32767.0f)*(float)MaxOffset);
                    YOffset += (int)
                               (((float)RightStickY/32767.0f)*(float)MaxOffset);

                    if (RightStickY > DEADZONE)
                        Instructions.MotorDirection = 1;

                    else if (RightStickY < -DEADZONE)
                        Instructions.MotorDirection = 2;

                    if (RightStickX > DEADZONE || RightStickX < -DEADZONE)
                    {
                        Instructions.ServoDirection = (char)
                                        (((float)RightStickX/32767.0f)*180.0f);
                    }

                    // TODO[joe] Figure out why this is fluxuating wildly.
                    // The microcontroller sometimes receives 255 when the
                    // trigger is all the way down, but there will be
                    // interjections of 1's in the middle that make the motors
                    // jump and skip.
                    Instructions.MotorSpeed = (char)
                                        ControllerState.Gamepad.bRightTrigger;
                }
            }

            // Perform application tasks.

            UpdateSlave((char *) &Instructions, sizeof(Instructions));

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

        /** BEGIN pre-exit maintenance.
         * TODO[joe] The Casey on my shoulder nags that this is unnecissary. */

        FreeLibrary(XInputHandle);
        WSACleanup();

        /** END pre-exit maintenance. */
    }

    return 0;
}
