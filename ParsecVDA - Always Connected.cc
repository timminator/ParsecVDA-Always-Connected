#ifndef UNICODE
#define UNICODE
#define UNICODE_WAS_UNDEFINED
#endif
#include <Windows.h>
#ifdef UNICODE_WAS_UNDEFINED
#undef UNICODE
#endif
#include <stdio.h>
#include <thread>
#include <chrono>
#include <vector>
#include <ctime> // For timestamp
#include <string>  // Include the <string> header for std::string
#include "parsec-vdd.h"

using namespace std::chrono_literals;
using namespace parsec_vdd;

// Global variables
bool running = true;
HANDLE vdd = nullptr;
std::vector<int> displays;
std::thread updater;
FILE *logfile = nullptr;

// Function to log messages with timestamp
void Log(const char* message) {
    if (logfile) {
        // Get current timestamp
        std::time_t now = std::time(nullptr);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

        // Write timestamp and message to logfile
        fprintf(logfile, "[%s] %s\n", timestamp, message);
        fflush(logfile); // Flush the stream to ensure data is written to the file immediately
    }
}

// Function to get the directory of the executable
std::string GetExecutableDirectory() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    return std::string(buffer).substr(0, pos);
}

// Console control handler function
BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
        case CTRL_CLOSE_EVENT:
            running = false;
            // Cleanup code
            Log("Cleanup code reached (CTRL_CLOSE_EVENT)!");
            for (int index : displays) {
                VddRemoveDisplay(vdd, index);
            }
            if (updater.joinable()) {
                updater.join();
            }
            // Close the device handle.
            CloseDeviceHandle(vdd);
            Log("Cleanup done!");
            fclose(logfile); // Close log file
            return TRUE; // Indicate that we've handled the event
        default:
            return FALSE;
    }
}

// Function to periodically check for termination signal
void CheckTerminationSignal() {
    while (running) {
        // Check for termination signal every second
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Check if termination signal is received
        if (!running) {
            // Cleanup code
            Log("Cleanup code reached (termination signal)!");
            for (int index : displays) {
                VddRemoveDisplay(vdd, index);
            }
            if (updater.joinable()) {
                updater.join();
            }
            // Close the device handle.
            CloseDeviceHandle(vdd);
            Log("Cleanup done!");
            fclose(logfile); // Close log file
            break;
        }
    }
}

// Window procedure for the hidden window
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_QUERYENDSESSION:
            running = false;
            return TRUE; // Allow the system to shut down
        case WM_ENDSESSION:
            // Cleanup code for system shutdown
            Log("Cleanup code reached (WM_ENDSESSION)!");
            // Cleanup code
            for (int index : displays) {
                VddRemoveDisplay(vdd, index);
            }
            if (updater.joinable()) {
                updater.join();
            }
            // Close the device handle.
            CloseDeviceHandle(vdd);
            Log("Cleanup done!");
            fclose(logfile); // Close log file
            return 0; // Allow the system to shut down
        case WM_CLOSE:
            running = false;
            return 0;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int main() {
    // Get the directory of the executable
    std::string executableDirectory = GetExecutableDirectory();

    // Create log file path
    std::string logFilePath = executableDirectory + "\\Logfile ParsecVDA - Always Connected.txt";

    // Open log file in append mode if it already exists
    logfile = fopen(logFilePath.c_str(), "a");
    if (!logfile) {
        // If file doesn't exist, create it
        logfile = fopen(logFilePath.c_str(), "w");
        if (!logfile) {
            printf("Error opening log file.\n");
            return 1;
        }
    }

    Log("New Session started!");

    // Set console control handler to listen for CTRL_CLOSE_EVENT
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        Log("Error setting console control handler.");
        return 1;
    }

    // Create a hidden window
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"HiddenWindowClass";
    RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(
        0,                          // Optional window styles
        L"HiddenWindowClass",       // Window class
        L"HiddenWindow",            // Window text
        0,                          // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, // Position and size
        NULL,                       // Parent window
        NULL,                       // Menu
        GetModuleHandle(NULL),      // Instance handle
        NULL                        // Additional application data
    );
    ShowWindow(hwnd, SW_HIDE);

    // Check driver status.
    DeviceStatus status = QueryDeviceStatus(&VDD_CLASS_GUID, VDD_HARDWARE_ID);
    if (status != DEVICE_OK) {
        Log("Parsec VDD device is not OK.");
        return 1;
    }

    // Obtain device handle.
    vdd = OpenDeviceHandle(&VDD_ADAPTER_GUID);
    if (vdd == NULL || vdd == INVALID_HANDLE_VALUE) {
        Log("Failed to obtain the device handle.");
        return 1;
    }

    // Side thread for updating vdd.
    updater = std::thread([&running, vdd] {
        while (running) {
            VddUpdate(vdd);
            std::this_thread::sleep_for(100ms);
        }
    });

    //Ads the Display
    if (displays.size() < VDD_MAX_DISPLAYS) {
        int index = VddAddDisplay(vdd);
        displays.push_back(index);
        char logMessage[100];
        sprintf(logMessage, "Added a new virtual display, index: %d.", index);
        Log(logMessage);
    }
    else {
        Log("Limit exceeded, could not add more virtual displays.");
    }

    // Start the thread to periodically check for termination signal
    std::thread terminationThread(CheckTerminationSignal);

    // Lets the program run indefinitely
    MSG msg;
    while (running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Join the termination thread
    if (terminationThread.joinable()) {
        terminationThread.join();
    }

    // This part shouldn't be necessary anymore, because the Cleanup Routine was moved into each case that indicates a closure of the Program.
    // Thereforce this part is never reached.
    // Cleanup code
    for (int index : displays) {
        VddRemoveDisplay(vdd, index);
    }
    if (updater.joinable()) {
        updater.join();
    }
    // Close the device handle.
    CloseDeviceHandle(vdd);

    // Close log file
    fclose(logfile);

    return 0;
}