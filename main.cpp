#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>
#include <comutil.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <iostream>

#include "scope_guard.hpp"

#pragma comment(lib, "comsuppw.lib")

const std::wstring CHROME(L"chrome.exe");
const std::wstring MSEDGE(L"msedge.exe");
const std::wstring FIREFOX(L"firefox.exe");

IUIAutomation *g_pAutomation;

BOOL initializeUIAutomation()
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    HRESULT hr = CoCreateInstance(__uuidof(CUIAutomation), NULL, CLSCTX_INPROC_SERVER,
                                  __uuidof(IUIAutomation), (void**)&g_pAutomation);
    return (SUCCEEDED(hr));
}

std::wstring getProcessName(const DWORD processID)
{
    const HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
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

    const DWORD amount = bytesReturned / sizeof(DWORD);
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

IUIAutomationCondition *createControlTypeCondition(const CONTROLTYPEID id)
{
    VARIANT variant;
    variant.vt = VT_I4;
    variant.lVal = id;

    IUIAutomationCondition *condition = nullptr;
    HRESULT result = g_pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, variant, &condition);
    VariantClear(&variant);

    if (FAILED(result)) return nullptr;
    return condition;
}

bool isReadOnly(IUIAutomationValuePattern *pattern)
{
    BOOL result;
    HRESULT hr = pattern->get_CurrentIsReadOnly(&result);
    if (FAILED(hr))
        throw std::runtime_error("Could not get IsReadOnly property");

    return result;
}

void modifySearchRequest(std::wstring &request)
{
    const std::wstring toFind = L"search?q=", toInsert = L"test:";
    request.insert(request.find(toFind) + toFind.size(), toInsert);
}

int main()
{
    if (!initializeUIAutomation())
    {
        std::wcerr << "Failed to initialize UI Automation" << std::endl;
        return -1;
    }
    auto uiaGuard = sg::make_scope_guard([]
    {
        g_pAutomation->Release();
        CoUninitialize();
    });

    IUIAutomationElement *rootElement = nullptr;
    HRESULT res = g_pAutomation->GetRootElement(&rootElement);
    if (FAILED(res))
    {
        std::wcerr << "Failed to get root element" << std::endl;
        return -1;
    }
    auto rootGuard = sg::make_scope_guard([rootElement] { rootElement->Release(); });

    DWORD processId = getProcessIdByName(MSEDGE);
    if (processId == 0)
    {
        std::wcerr << "Process not found" << std::endl;
        return -1;
    }

    IUIAutomationCondition *browserCondition = createProcessIdCondition(processId);
    if (!browserCondition)
    {
        std::wcerr << "Condition not created" << std::endl;
        return -1;
    }
    auto browserConditionGuard = sg::make_scope_guard([browserCondition] { browserCondition->Release(); });

    IUIAutomationElement *browserElement = nullptr;
    res = rootElement->FindFirst(TreeScope_Children, browserCondition, &browserElement);
    if (FAILED(res) || !browserElement)
    {
        std::wcerr << "Specified browser not found" << std::endl;
        return -1;
    }
    auto browserElementGuard = sg::make_scope_guard([browserElement] { browserElement->Release(); });

    IUIAutomationCondition *controlCondition = createControlTypeCondition(UIA_EditControlTypeId);
    if (!controlCondition)
    {
        std::wcerr << "Cannot create edit control condition" << std::endl;
        return -1;
    }
    auto controlConditionGuard = sg::make_scope_guard([controlCondition] { controlCondition->Release(); });

    IUIAutomationElement *editControlElement = nullptr;
    res = browserElement->FindFirst(TreeScope_Subtree, controlCondition, &editControlElement);
    if (FAILED(res) || !editControlElement)
    {
        std::wcerr << "Edit control not found" << std::endl;
        return -1;
    }
    auto editControlElementGuard = sg::make_scope_guard([editControlElement] { editControlElement->Release(); });

    VARIANT value;
    res = editControlElement->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &value);
    if (FAILED(res))
    {
        std::wcout << L"Failed to get property value." << std::endl;
        return -1;
    }
    if (value.vt == VT_BSTR) std::wcout << L"Found: " << value.bstrVal << std::endl;
    else std::wcout << L"Property is not a string." << std::endl;
    VariantClear(&value);

    IUIAutomationValuePattern* valuePattern = nullptr;
    res = editControlElement->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&valuePattern));
    if (FAILED(res))
    {
        std::wcerr << "Failed to get value pattern" << std::endl;
        return -1;
    }
    auto vpGuard = sg::make_scope_guard([valuePattern] { valuePattern->Release(); });

    bool readOnly;
    try
    {
        readOnly = isReadOnly(valuePattern);
    }
    catch (const std::runtime_error &ex)
    {
        std::wcerr << ex.what() << std::endl;
        return -1;
    }

    if (readOnly)
    {
        std::wcerr << "The value is read only" << std::endl;
        return -1;
    }

    std::wstring val(value.bstrVal);
    modifySearchRequest(val);

    res = valuePattern->SetValue(val.data());
    if (FAILED(res))
    {
        std::wcerr << "Could not set new value" << std::endl;
        return -1;
    }

    return 0;
}
