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

IUIAutomationCondition *createProcessIdCondition(const DWORD processID)
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

IUIAutomationCondition *createAutomationIdCondition(const std::wstring &automationID)
{
    VARIANT variant;
    variant.vt = VT_BSTR;
    variant.bstrVal = SysAllocString(automationID.c_str());

    IUIAutomationCondition *condition = nullptr;
    HRESULT result = g_pAutomation->CreatePropertyCondition(UIA_AutomationIdPropertyId, variant, &condition);
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
        std::wcerr << "Failed" << std::endl;
        return -1;
    }
    std::wcout << "Successfully got root element" << std::endl;

    DWORD processId = getProcessIdByName(L"firefox.exe");
    if (processId == 0)
    {
        std::wcerr << "Process not found" << std::endl;
        return -1;
    }
    std::wcout << "Process ID: " << processId << std::endl;

    IUIAutomationCondition *condition = createProcessIdCondition(processId);
    if (!condition)
    {
        std::wcerr << "Condition not created" << std::endl;
        return -1;
    }
    std::wcout << "Successfully created condition" << std::endl;

    IUIAutomationElement *foundElement = nullptr;
    res = root->FindFirst(TreeScope_Children, condition, &foundElement);
    if (FAILED(res) || !foundElement)
    {
        std::wcerr << "Specified browser not found" << std::endl;
        return -1;
    }
    std::wcout << "Browser found" << std::endl;

    IUIAutomationCondition *searchBoxCondition = createAutomationIdCondition(L"gsr");
    if (!searchBoxCondition)
    {
        std::wcerr << "Cannot create search box condition" << std::endl;
        return -1;
    }
    std::wcout << "Search box condition created" << std::endl;

    IUIAutomationElement *searchBox = nullptr;
    res = foundElement->FindFirst(TreeScope_Subtree, searchBoxCondition, &searchBox);
    if (FAILED(res) || !searchBox)
    {
        std::wcerr << "Search box not found" << std::endl;
        return -1;
    }
    std::wcout << "Search box successfully found" << std::endl;

    return 0;
}
