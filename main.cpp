#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>
#include <comutil.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <iostream>

#pragma comment(lib, "comsuppw.lib")

IUIAutomation *g_pAutomation;

BOOL initializeUIAutomation()
{
    CoInitialize(NULL);
    HRESULT hr = CoCreateInstance(__uuidof(CUIAutomation), NULL, CLSCTX_INPROC_SERVER,
                                  __uuidof(IUIAutomation), (void**)&g_pAutomation);
    return (SUCCEEDED(hr));
}

std::wstring getProcessName(const DWORD processID)
{
    HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    if (!handle) return {};

    std::wstring processName(MAX_PATH, L'\0');
    const DWORD nameSize = GetModuleBaseNameW(handle, 0, &processName[0], processName.length());
    CloseHandle(handle);
    if (!nameSize) return {};

    processName.resize(nameSize);
    return processName;
}

DWORD getProcessIdByName(const std::wstring &name)
{
    DWORD processes[1024], bytesReturned;
    if (!EnumProcesses(processes, sizeof(processes), &bytesReturned))
    {
        std::wcerr << "Could not get process ids" << std::endl;
        return 0;
    }

    DWORD amount = bytesReturned / sizeof(DWORD);
    for (size_t i = 0; i < amount; ++i)
    {
        const DWORD processID = processes[i];
        if (!processID) continue;

        const std::wstring currentProcessName = getProcessName(processID);
        if (currentProcessName == name) return processID;
    }

    return 0;
}

IUIAutomationCondition *createCondition(const DWORD processID)
{

    VARIANT variant;
    VariantInit(&variant);
    variant.vt = VT_I4;
    variant.lVal = processID;

    IUIAutomationCondition *condition = nullptr;
    HRESULT result = g_pAutomation->CreatePropertyCondition(UIA_ProcessIdPropertyId, variant, &condition);
    VariantClear(&variant);

    if (FAILED(result)) return nullptr;
    return condition;
}

// chrome.exe
// firefox.exe
// msedge.exe

int main()
{
    initializeUIAutomation();

    IUIAutomationElement *root = nullptr;
    HRESULT res = g_pAutomation->GetRootElement(&root);
    if (FAILED(res))
    {
        std::wcout << "Failed" << std::endl;
        return -1;
    }
    std::wcout << "Successfully got root element" << std::endl;

    DWORD processId = getProcessIdByName(L"firefox.exe");
    if (processId == 0)
    {
        std::wcout << "Process not found" << std::endl;
        return -1;
    }
    std::wcout << "Process ID: " << processId << std::endl;

    IUIAutomationCondition *condition = createCondition(processId);
    if (!condition)
    {
        std::wcout << "Condition not created" << std::endl;
        return -1;
    }
    std::wcout << "Successfully created condition" << std::endl;

    IUIAutomationElement *foundElement = nullptr;
    res = root->FindFirst(TreeScope_Children, condition, &foundElement);
    if (SUCCEEDED(res) && foundElement) std::wcout << "Successfully found" << std::endl;
    else
    {
        std::wcout << "Not found" << std::endl;
        return -1;
    }

    return 0;
}
