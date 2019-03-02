/*
 * @file main.c
 * @author Joseph Miles <josephmiles2015@gmail.com>
 * @date 2019-03-02
 *
 * This is is the main driver file for the control appliction.
 *
 * TODO[joe]
 *  -  Create a way to draw to the window.
 *      -  Figure out how to draw text to the screen.
 *  -  Create a way to access BlueTooth devices.
 *      -  Figure out how to access list of BlueTooth devices.
 *      -  Figure out how to search that list for a particular device.
 *      -  Figure out how to open a port to that device.
 *      -  Figure out how to set port profile.
 *      -  Figure out how to send data to connected device.
 *  -  Create a way to use keyboard input.
 */

// Define the Unicode macro so that Windows knows what mode we're in.
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>

// NOTE[joe] These globals are temporary!
static int ApplicationQuit;


/** This is our window procedure, used by Windows whenever it wants us to stop
 * what we're doing and do something for it. */
LRESULT CALLBACK WindowProcedure(HWND Window,
                                 UINT Message,
                                 WPARAM wParameter,
                                 LPARAM lParameter)
{
    switch (Message)
    {
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

        ApplicationQuit = 0;

        while (!ApplicationQuit)
        {
            MSG Message = { 0 };
            while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            }
        }
    }

    return 0;
}
