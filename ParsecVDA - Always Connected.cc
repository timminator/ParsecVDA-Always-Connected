#include <windows.h>
#include <cfgmgr32.h>
#include <thread>
#include <chrono>
#include <vector>
#include <iostream>
#include <winevt.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <algorithm>
#include "parsec-vdd.h"

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
bool receivedeventdrivercrash = false;
bool ParsecVDAfound = true;
int MainLoopResult = 0;
HANDLE vdd = nullptr;
std::vector<int> displays;
std::thread updater;
std::thread CheckForParsecVDAThread;
std::string deviceInstanceId = "ROOT\\Display\\0000"; // Device instance ID of the Parsec Virtual Display Adapter
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

// Console control handler function for Ctrl+C event
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
            if (CheckForParsecVDAThread.joinable()) {
                CheckForParsecVDAThread.join();
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

// Function to check for system events signaling sleep, waking from sleep and a driver crash
void CheckSystemEventLog(void) {

    DWORD status = ERROR_SUCCESS;
    EVT_HANDLE hResults = NULL;
    
    // Events for going to sleep, waking from sleep and driver crash
    #define QUERY \
        L"<QueryList>" \
        L"  <Query Path='System'>" \
        L"    <Select>Event/System[EventID=506 or EventID=507 or EventID=42 or EventID=107 or EventID=187 or EventID=10111]</Select>" \
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
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
    } else if (EventID == 10111) {
        receivedeventdrivercrash = true;
    }

    if (hContext)
        EvtClose(hContext);

    if (pRenderedValues)
        free(pRenderedValues);

    return status;
}

// Helper function to log messages with error codes
void LogError(const char* message, CONFIGRET cr) {
    char logMessage[256];
    snprintf(logMessage, sizeof(logMessage), "%s (Error Code: %lu).", message, cr);
    Log(logMessage);
}

// Helper function to locate and check device node
bool LocateAndCheckDeviceNode(const std::string& deviceInstanceId, DEVINST& devInst, ULONG& status, ULONG& problemNumber) {
    CONFIGRET cr = CM_Locate_DevNodeA(&devInst, const_cast<DEVINSTID_A>(deviceInstanceId.c_str()), CM_LOCATE_DEVNODE_NORMAL);
    if (cr != CR_SUCCESS) {
        LogError("Failed to locate device node", cr);
        return false;
    }

    cr = CM_Get_DevNode_Status(&status, &problemNumber, devInst, 0);
    if (cr != CR_SUCCESS) {
        LogError("Failed to get device node status", cr);
        return false;
    }

    return true;
}

// Function to disable the device
bool DisableDevice(const std::string& deviceInstanceId) {
    DEVINST devInst;
    ULONG status, problemNumber;

    if (!LocateAndCheckDeviceNode(deviceInstanceId, devInst, status, problemNumber)) {
        return false;
    }

    CONFIGRET cr = CM_Disable_DevNode(devInst, 0);
    if (cr != CR_SUCCESS) {
        LogError("Failed to disable device node", cr);
        return false;
    }
 
    Log("Device disabled successfully.");
    return true;
}

// Function to enable the device
bool EnableDevice(const std::string& deviceInstanceId) {
    DEVINST devInst;
    ULONG status, problemNumber;

    if (!LocateAndCheckDeviceNode(deviceInstanceId, devInst, status, problemNumber)) {
        return false;
    }

    CONFIGRET cr = CM_Enable_DevNode(devInst, 0);
    if (cr != CR_SUCCESS) {
        LogError("Failed to enable device node", cr);
        return false;
    }

    Log("Device enabled successfully.");
    return true;
}

// Function to recover the driver after a driver crash
bool DriverRecover(const std::string& deviceInstanceId) {
    if (!DisableDevice(deviceInstanceId)) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (!EnableDevice(deviceInstanceId)) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return true;
}

// Helper function to convert std::string to std::wstring
std::wstring StringToWString(const std::string& str) {
    int len;
    int str_len = (int)str.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), str_len, 0, 0);
    std::wstring wstr(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), str_len, &wstr[0], len);
    return wstr;
}

// Helper function to convert std::string to BSTR
BSTR ConvertStringToBSTR(const std::string& str) {
    std::wstring wstr = StringToWString(str);
    return SysAllocString(wstr.c_str());
}

// Class for collecting info about connected monitors
class MonitorInfo {
public:
    std::vector<std::string> friendlyDisplayNames;
    bool success;

    MonitorInfo() : success(false) {}
};

// Function to retrieve friendly display names of all connected monitors
MonitorInfo GetMonitorFriendlyNamesUsingWMI() {
    MonitorInfo monitorInfo;

    HRESULT hr;
    IWbemLocator *pLoc = nullptr;
    IWbemServices *pSvc = nullptr;
    IEnumWbemClassObject* pEnumerator = nullptr;

    // Initialize COM
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        return monitorInfo;
    }

    // Initialize security
    hr = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL);

    if (FAILED(hr)) {
        CoUninitialize();
        return monitorInfo;
    }

    // Obtain the initial locator to WMI
    hr = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID *)&pLoc);

    if (FAILED(hr)) {
        CoUninitialize();
        return monitorInfo;
    }

    // Connect to the WMI namespace
    hr = pLoc->ConnectServer(
        ConvertStringToBSTR("ROOT\\WMI"), // WMI namespace
        NULL,                    // User name
        NULL,                    // User password
        NULL,                    // Locale
        0,                       // Security flags
        0,                       // Authority
        0,                       // Context object
        &pSvc);                  // IWbemServices proxy

    if (FAILED(hr)) {
        pLoc->Release();
        CoUninitialize();
        return monitorInfo;
    }

    // Set security levels on the proxy
    hr = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE);

    if (FAILED(hr)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return monitorInfo;
    }

    // Use WMI to retrieve the friendly names of monitors
    hr = pSvc->ExecQuery(
        ConvertStringToBSTR("WQL"),
        ConvertStringToBSTR("SELECT * FROM WmiMonitorID"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);

    if (FAILED(hr)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return monitorInfo;
    }

    // Get the data from the query
    IWbemClassObject *pclsObj = nullptr;
    ULONG uReturn = 0;

    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;
        hr = pclsObj->Get(L"UserFriendlyName", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr)) {
            if (vtProp.vt == (VT_ARRAY | VT_I4)) {
                SAFEARRAY* sa = vtProp.parray;
                LONG* pVals;
                SafeArrayAccessData(sa, (void**)&pVals);

                std::wstring ws;
                for (LONG i = 0; i < (LONG)sa->rgsabound[0].cElements; ++i) {
                    ws += static_cast<wchar_t>(pVals[i]);
                }
                std::string monitorName(ws.begin(), ws.end());
                monitorInfo.friendlyDisplayNames.push_back(monitorName);

                SafeArrayUnaccessData(sa);
                monitorInfo.success = true;
            } 
        }
        VariantClear(&vtProp);

        pclsObj->Release();
    }

    // Cleanup
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();

    return monitorInfo;
}

// Function to check if ParsecVDA is among the friendly display names
void CheckForParsecVDA() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Cannot be too long, otherwise joining the thread takes just as long aswell 

        MonitorInfo monitorInfo = GetMonitorFriendlyNamesUsingWMI();

        if (!monitorInfo.success) {
            ParsecVDAfound = true; // Added in the event the query fails, otherwise it would cause the program to always disconnect and reconnect the display
            return;
        }

        bool ParsecVDAresult = std::any_of(monitorInfo.friendlyDisplayNames.begin(), monitorInfo.friendlyDisplayNames.end(), [](const std::string& name) {
            return name.find("ParsecVDA") != std::string::npos;
        });

        if (!ParsecVDAresult) {
            std::this_thread::sleep_for(std::chrono::seconds(3)); // This is triggered before the driver crash event. Ensures that this is only logged if its not a driver crash
            ParsecVDAfound = false;
        }
    }
}

// Function for disconnecting the display or doing the cleanup procedure in case of a driver crash or a lost display
void SleepCrashAction() {
    while (!finalshutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        if (receivedeventsleep || receivedeventdrivercrash || !ParsecVDAfound) {
            if (!cleanupstarted) {
                if (receivedeventdrivercrash) {
                    Log("Driver crashed!");
                }
                else if (receivedeventsleep) {
                    Log("Cleanup code reached!");
                }
                else if (!ParsecVDAfound) {
                    Log("ParsecVDA not found!");
                }
                cleanupstarted = true; // Solves reaching the cleanup procedure twice, because it it quite common to receive EventID 187 and 42 right after each other
                running = false;
                suspend = true;
                if (receivedeventsleep) {
                    for (int index : displays) {
                        VddRemoveDisplay(vdd, index);
                    }
                }
                if (updater.joinable()) {
                    updater.join();
                }
                if (CheckForParsecVDAThread.joinable()) {
                    CheckForParsecVDAThread.join();
                }
                // Close the device handle.
                CloseDeviceHandle(vdd);
                Log("Cleanup done!");
                stopmainloop = true; // In the end, so that the main loop not finishes until cleanup routine is done
            }
        }
    }
}

// Function for connecting the monitor
int MainLoop() {
    Log("New Session started!");

    // Check driver status.
    bool firstattemptfail = false;
    DeviceStatus status = QueryDeviceStatus(&VDD_CLASS_GUID, VDD_HARDWARE_ID);
    if (status != DEVICE_OK) {
        Log("Parsec VDD device is not OK. Trying to recover the driver!");
        if (DriverRecover(deviceInstanceId)) {
            DeviceStatus status = QueryDeviceStatus(&VDD_CLASS_GUID, VDD_HARDWARE_ID);
            if (status != DEVICE_OK) {
                firstattemptfail = true;
                Log("Parsec VDD device is not OK. Trying a second time!");
            }
            else if (status == DEVICE_OK) {
                Log("Parsec VDD device recovered!");
            }
        }
        else {
            firstattemptfail = true;
        }
        if (firstattemptfail == true) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (DriverRecover(deviceInstanceId)) {
                DeviceStatus status = QueryDeviceStatus(&VDD_CLASS_GUID, VDD_HARDWARE_ID);
                if (status != DEVICE_OK) {
                    Log("Parsec VDD device is not OK. Exiting program!");
                    return 1;
                }
                Log("Parsec VDD device recovered!");
            }
            else { 
                return 1;
            }
        }
    }

    // Obtain device handle.
    vdd = OpenDeviceHandle(&VDD_ADAPTER_GUID);
    if (vdd == NULL || vdd == INVALID_HANDLE_VALUE) {
        Log("Failed to obtain the device handle. Trying a second time!");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (vdd == NULL || vdd == INVALID_HANDLE_VALUE) {
            Log("Failed to obtain the device handle. Exiting program!");
            return 1;
        }
    }

    // Side thread for updating vdd.
    updater = std::thread([] {
        while (running) {
            VddUpdate(vdd);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    //Ad the Display
    if (displays.size() < VDD_MAX_DISPLAYS) {
        int index = VddAddDisplay(vdd);
        displays.push_back(index);
        displays.resize(index+1); // Important! Without it, the size increases in every run of the MainLoop!
        char logMessageDisplay[100];
        snprintf(logMessageDisplay, sizeof(logMessageDisplay), "Added a new virtual display, index: %d.", index);
        Log(logMessageDisplay);
    }
    else {
        Log("Limit exceeded, could not add more virtual displays.");
    }

    // Start the thread to periodically check if display is still connected
    CheckForParsecVDAThread = std::thread(CheckForParsecVDA);

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
    std::thread SleepCrashActionThread(SleepCrashAction);

    // Start the main loop and manage the occurring states
    while (!finalshutdown) {
        if (!suspend) {
            MainLoopResult = MainLoop();
            if (MainLoopResult != 0) {
                finalshutdown = true;
            }
        }
        else if (suspend) {
            if (receivedeventsleep) {
                Log("Suspend stage reached.");
                while (!receivedeventwake) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                // Prevent race hazard between !receivedeventwake and systemalreadyawake
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            // Second waiting time is necessary for three reasons:
            // EventID 107 is received so early, that adding a virtual display right after receiving the message is in some cases not possible.
            // Furthermore, while systemalreadyawake is true, other EventIDs (506 and 507 after waking from hibernation with modern standby enabled)
            // can not be triggered, as intended, because we already received a wakeup event.
            // In the event of driver crash a waiting time is necessary before the Parsec Virtual Display Adapter can be disabled and re-enabled.
            if (systemalreadyawake || receivedeventdrivercrash || !ParsecVDAfound) {
                    std::this_thread::sleep_for(std::chrono::seconds(3));
            }
            receivedeventsleep = false;
            receivedeventwake = false;
            receivedeventdrivercrash = false;
            systemalreadyawake = false;
            ParsecVDAfound = true;
            suspend = false;
            running = true;
            cleanupstarted = false;
            stopmainloop = false;
        }
    }
    
    // Join the threads
    if (SystemEventThread.joinable()) {
        SystemEventThread.join();
    }
    if (SleepCrashActionThread.joinable()) {
        SleepCrashActionThread.join();
    }
    
    Log("Program is closing!");
    fclose(logfile); // Close log file

    return MainLoopResult;
}