#include <windows.h>
#include <thread>
#include <chrono>
#include <vector>
#include <iostream>
#include <winevt.h>
#include "parsec-vdd.h"

using namespace std::chrono_literals;
using namespace parsec_vdd;

// Global variables
bool running = true;
bool cleanupstarted = false;
bool finalshutdown = false;
bool suspend = false;
bool stopmainloop = false;
bool receivedeventsleep = false;
bool receivedeventwake = false;
bool systemalreadyawake = false;
int MainLoopResult = 0;
HANDLE vdd = nullptr;
std::vector<int> displays;
std::thread updater;
FILE *logfile = nullptr;
DWORD PrintEventSystemData(EVT_HANDLE hEvent);
DWORD WINAPI SubscriptionCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE hEvent);

// Function to open the log file
bool OpenLogFile() {
    // Get the directory of the executable
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    std::string executableDirectory = std::string(buffer).substr(0, pos);

    // Create log file path
    std::string logFilePath = executableDirectory + "\\Logfile ParsecVDA - Always Connected.txt";

    // Open log file in append mode if it already exists
    logfile = fopen(logFilePath.c_str(), "a");
    if (!logfile) {
        // If file doesn't exist, create it
        logfile = fopen(logFilePath.c_str(), "w");
        if (!logfile) {
            printf("Error opening log file.\n");
            return false;
        }
    }
    return true;
}

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

// Console control handler function for Ctrl+Close and Ctrl+C event
BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
        case CTRL_C_EVENT:
            // Cleanup code for Ctrl+C event
            running = false;
            finalshutdown = true;
            cleanupstarted = true;
            Log("Cleanup code reached (Ctrl+C)!");
            for (int index : displays) {
                VddRemoveDisplay(vdd, index);
            }
            if (updater.joinable()) {
                updater.join();
            }
            // Close the device handle.
            CloseDeviceHandle(vdd);
            stopmainloop = true;
            Log("Cleanup done!");
            return TRUE; // Indicate that we've handled the event
        default:
            return FALSE;
    }
}

// Function to check for system events signaling sleep or waking from sleep
void CheckSystemEventLog(void) {

    DWORD status = ERROR_SUCCESS;
    EVT_HANDLE hResults = NULL;
    
    // Events for going to sleep and waking from sleep
    #define QUERY \
        L"<QueryList>" \
        L"  <Query Path='System'>" \
        L"    <Select>Event/System[EventID=506 or EventID=507 or EventID=42 or EventID=107 or EventID=187]</Select>" \
        L"  </Query>" \
        L"</QueryList>"

    hResults = EvtSubscribe(NULL, NULL, NULL, QUERY, NULL, NULL, (EVT_SUBSCRIBE_CALLBACK)SubscriptionCallback,  EvtSubscribeToFutureEvents);
    if (NULL == hResults)
    {
        status = GetLastError();

        if (ERROR_EVT_CHANNEL_NOT_FOUND == status)
            wprintf(L"The channel was not found.\n");
        else if (ERROR_EVT_INVALID_QUERY == status)
            wprintf(L"The query is not valid.\n");
        else
            wprintf(L"EvtQuery failed with %lu.\n", status);

        goto cleanup;
    }
    while(!finalshutdown) {
    Sleep(50);
    }

cleanup:

    if (hResults)
        EvtClose(hResults);
    
}

// The callback that receives the events that match the query criteria. 
DWORD WINAPI SubscriptionCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE hEvent) {

    UNREFERENCED_PARAMETER(pContext);

    DWORD status = ERROR_SUCCESS;

    switch (action)
    {
    case EvtSubscribeActionDeliver:
        if (ERROR_SUCCESS != (status = PrintEventSystemData(hEvent)))
        {
            Log("Error while printing the system event data!");
            return status; // The service ignores the returned status.
        }
        break;

    default:
        Log("SubscriptionCallback: Unknown action. Could not correctly receive the system event!");
    }

    return status; // The service ignores the returned status.
}

// Render the system EventID
DWORD PrintEventSystemData(EVT_HANDLE hEvent) {

    DWORD status = ERROR_SUCCESS;
    EVT_HANDLE hContext = NULL;
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;
    PEVT_VARIANT pRenderedValues = NULL;
    
    hContext = EvtCreateRenderContext(0, NULL, EvtRenderContextSystem);
    if (NULL == hContext)
    {
        wprintf(L"EvtCreateRenderContext failed with %lu\n", status = GetLastError());
        if (hContext)
            EvtClose(hContext);

        if (pRenderedValues)
            free(pRenderedValues);

        return status;
    }

    if (!EvtRender(hContext, hEvent, EvtRenderEventValues, dwBufferSize, pRenderedValues, &dwBufferUsed, &dwPropertyCount))
    {
        if (ERROR_INSUFFICIENT_BUFFER == (status = GetLastError()))
        {
            dwBufferSize = dwBufferUsed;
            pRenderedValues = (PEVT_VARIANT)malloc(dwBufferSize);
            if (pRenderedValues)
            {
                EvtRender(hContext, hEvent, EvtRenderEventValues, dwBufferSize, pRenderedValues, &dwBufferUsed, &dwPropertyCount);
            }
            else
            {
                wprintf(L"malloc failed\n");
                status = ERROR_OUTOFMEMORY;
                if (hContext)
                    EvtClose(hContext);

                if (pRenderedValues)
                    free(pRenderedValues);

                return status;
            }
        }

        if (ERROR_SUCCESS != (status = GetLastError()))
        {
            wprintf(L"EvtRender failed with %d\n", GetLastError());
            if (hContext)
                EvtClose(hContext);

            if (pRenderedValues)
                free(pRenderedValues);

            return status;
        }
    }

    // Print the values from the System section of the element.
    DWORD EventID = pRenderedValues[EvtSystemEventID].UInt16Val;
    if (EvtVarTypeNull != pRenderedValues[EvtSystemQualifiers].Type)
    {
        EventID = MAKELONG(pRenderedValues[EvtSystemEventID].UInt16Val, pRenderedValues[EvtSystemQualifiers].UInt16Val);
    }

    if (EventID == 506 || EventID == 42 || EventID == 187) {
        if (!systemalreadyawake) {
            receivedeventsleep = true;
        }
    } else if (EventID == 507 || EventID == 107) {
        if (!systemalreadyawake) {
            receivedeventwake = true;
        } 
        if (EventID == 107) {
            // EventID 506 can be received right after an EventID 107 (happens on Laptops with Modern Standby enabled after Hibernation).
            // This variable is needed to not trigger the cleanup procedure again.
            systemalreadyawake = true;
        }
    }

    if (hContext)
        EvtClose(hContext);

    if (pRenderedValues)
        free(pRenderedValues);

    return status;
}

// Function for disconnecting display
void SleepAction() {
    while (!finalshutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        if (receivedeventsleep) {
            if (!cleanupstarted) {
                Log("Cleanup code reached!");
                cleanupstarted = true; // Solves reaching the cleanup procedure twice, because it it quite common to receive EventID 187 and 42 right after each other
                running = false;
                suspend = true;
                for (int index : displays) {
                    VddRemoveDisplay(vdd, index);
                }
                if (updater.joinable()) {
                    updater.join();
                }
                // Close the device handle.
                CloseDeviceHandle(vdd);
                Log("Cleanup done!");
                stopmainloop = true; // In the end, so that the main loop not finishes until cleanup routine is done
            }
        }
    }
}

// Fuction for connecting the monitor
int MainLoop() {
    Log("New Session started!");

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
    updater = std::thread([] {
        while (running) {
            VddUpdate(vdd);
            std::this_thread::sleep_for(100ms);
        }
    });

    //Ad the Display
    if (displays.size() < VDD_MAX_DISPLAYS) {
        int index = VddAddDisplay(vdd);
        displays.push_back(index);
        displays.resize(index+1); // Important! Without it, the size increases in every run of the MainLoop!
        char logMessage[100];
        sprintf(logMessage, "Added a new virtual display, index: %d.", index);
        Log(logMessage);
    }
    else {
        Log("Limit exceeded, could not add more virtual displays.");
    }

    // Let the program run indefinitely
    while (running || !stopmainloop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Sleep briefly to avoid high CPU usage
    }
    
    return 0;
}

int main() {

    // Open log file
    if (!OpenLogFile()) {
        printf("Error opening log file.\n");
        return 1;
    }

    // Set console control handler to listen for CTRL_C_EVENT
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        Log("Error setting console control handler.");
        return 1;
    }

    // Start the threads to periodically check for system events and execute sleep action
    std::thread SystemEventThread(CheckSystemEventLog);
    std::thread SleepActionThread(SleepAction);

    // Start the main loop and manage the occurring states
    while (!finalshutdown) {
        if (!suspend) {
            MainLoopResult = MainLoop();
        }
        else if (suspend) {
            Log("Suspend stage reached.");
            while (!receivedeventwake) {
                Sleep(50);
            }
            // Prevent race hazard between !receivedeventwake and systemalreadyawake
            Sleep(50);
            // Second Loop is necessary for two reasons:
            // EventID 107 is received so early, that adding a virtual display right after receiving the message is in some cases not possible.
            // Furthermore, while systemalreadyawake is true, other EventIDs (506 and 507 after waking from hibernation with modern standby enabled)
            // can not be triggered, as intended, because we already received a wakeup event.
            if (systemalreadyawake) {
                Sleep(3000);
            } 
            receivedeventsleep = false;
            receivedeventwake = false;
            systemalreadyawake = false;
            suspend = false;
            running = true;
            cleanupstarted = false;
            stopmainloop = false;
            
            MainLoopResult = MainLoop();
        }
    }
    
    // Join the threads
    if (SystemEventThread.joinable()) {
        SystemEventThread.join();
    }
    if (SleepActionThread.joinable()) {
        SleepActionThread.join();
    }

    Log("Program is closing!");
    fclose(logfile); // Close log file

    return MainLoopResult;
}