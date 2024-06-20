#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>
#include <comutil.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <iostream>

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

void modifySearchRequest(std::wstring &request)
{
    const std::wstring toFind = L"search?q=", toInsert = L"test:";
    request.insert(request.find(toFind) + toFind.size(), toInsert);
}

int main()
{
    initializeUIAutomation();

    IUIAutomationElement *rootElement = nullptr;
    HRESULT res = g_pAutomation->GetRootElement(&rootElement);
    if (FAILED(res))
    {
        std::wcerr << "Failed to get root element" << std::endl;
        return -1;
    }

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

    IUIAutomationElement *browserElement = nullptr;
    res = rootElement->FindFirst(TreeScope_Children, browserCondition, &browserElement);
    if (FAILED(res) || !browserElement)
    {
        std::wcerr << "Specified browser not found" << std::endl;
        return -1;
    }

    IUIAutomationCondition *controlCondition = createControlTypeCondition(UIA_EditControlTypeId);
    if (!controlCondition)
    {
        std::wcerr << "Cannot create edit control condition" << std::endl;
        return -1;
    }

    IUIAutomationElement *editControlElement = nullptr;
    res = browserElement->FindFirst(TreeScope_Subtree, controlCondition, &editControlElement);
    if (FAILED(res) || !editControlElement)
    {
        std::wcerr << "Edit control not found" << std::endl;
        return -1;
    }

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

    std::wstring val(value.bstrVal);
    modifySearchRequest(val);

    IUIAutomationValuePattern* valuePattern = nullptr;
    res = editControlElement->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&valuePattern));
    if (FAILED(res))
    {
        std::wcout << "Failed to get value pattern" << std::endl;
        return -1;
    }
    valuePattern->SetValue(val.data());

    BOOL isReadOnly;
    res = valuePattern->get_CurrentIsReadOnly(&isReadOnly);
    if (FAILED(res))
    {
        std::wcout << "Failed to get isReadOnly property" << std::endl;
        return -1;
    }

    if (isReadOnly)
    {
        std::wcerr << "Selected value is read only" << std::endl;
        return -1;
    }

    valuePattern->Release();
    editControlElement->Release();
    controlCondition->Release();
    browserElement->Release();
    rootElement->Release();
    browserCondition->Release();
    g_pAutomation->Release();
    CoUninitialize();

    return 0;
}
