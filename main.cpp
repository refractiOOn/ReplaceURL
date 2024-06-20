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
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
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

class EventHandler: public IUIAutomationPropertyChangedEventHandler
{
private:
    LONG _refCount;

public:
    int _eventCount;

    //Constructor.
    EventHandler(): _refCount(1), _eventCount(0)
    {
    }

    //IUnknown methods.
    ULONG STDMETHODCALLTYPE AddRef()
    {
        ULONG ret = InterlockedIncrement(&_refCount);
        return ret;
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ret = InterlockedDecrement(&_refCount);
        if (ret == 0)
        {
            delete this;
            return 0;
        }
        return ret;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppInterface)
    {
        if (riid == __uuidof(IUnknown))
            *ppInterface=static_cast<IUIAutomationPropertyChangedEventHandler*>(this);
        else if (riid == __uuidof(IUIAutomationPropertyChangedEventHandler))
            *ppInterface=static_cast<IUIAutomationPropertyChangedEventHandler*>(this);
        else
        {
            *ppInterface = NULL;
            return E_NOINTERFACE;
        }
        this->AddRef();
        return S_OK;
    }

    // IUIAutomationPropertyChangedEventHandler methods.
    HRESULT STDMETHODCALLTYPE HandlePropertyChangedEvent(IUIAutomationElement* pSender, PROPERTYID propertyID, VARIANT newValue)
    {
        _eventCount++;
        if (propertyID == UIA_ValueValuePropertyId)
            wprintf(L">> Property Value changed! ");
        else
            wprintf(L">> Property (%d) changed! ", propertyID);

        if (newValue.vt == VT_I4)
            wprintf(L"(0x%0.8x) ", newValue.lVal);

        wprintf(L"(count: %d)\n", _eventCount);
        return S_OK;
    }
};

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
        std::wcerr << "Failed to get root element" << std::endl;
        return -1;
    }

    DWORD processId = getProcessIdByName(L"msedge.exe");
    if (processId == 0)
    {
        std::wcerr << "Process not found" << std::endl;
        return -1;
    }

    IUIAutomationCondition *condition = createProcessIdCondition(processId);
    if (!condition)
    {
        std::wcerr << "Condition not created" << std::endl;
        return -1;
    }

    IUIAutomationElement *foundElement = nullptr;
    res = root->FindFirst(TreeScope_Children, condition, &foundElement);
    if (FAILED(res) || !foundElement)
    {
        std::wcerr << "Specified browser not found" << std::endl;
        return -1;
    }

    // IUIAutomationCondition *cond = createControlTypeCondition(UIA_DocumentControlTypeId);
    IUIAutomationCondition *cond = createControlTypeCondition(UIA_EditControlTypeId);
    if (!cond)
    {
        std::wcerr << "Cannot create document control condition" << std::endl;
        return -1;
    }

    IUIAutomationElement *editControl = nullptr;
    res = foundElement->FindFirst(TreeScope_Subtree, cond, &editControl);
    if (FAILED(res) || !editControl)
    {
        std::wcerr << "Edit control not found" << std::endl;
        return -1;
    }

    VARIANT value;
    res = editControl->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &value);
    if (FAILED(res))
    {
        std::wcout << L"Failed to get property value." << std::endl;
        return -1;
    }
    if (value.vt == VT_BSTR) std::wcout << L"Found: " << value.bstrVal << std::endl;
    else std::wcout << L"Property is not a string." << std::endl;
    VariantClear(&value);

    std::wstring val(value.bstrVal);

    const std::wstring toFind = L"search?q=", toInsert = L"test:";
    val.insert(val.find(toFind) + toFind.size(), toInsert);

    IUIAutomationValuePattern* valuePattern = nullptr;
    res = editControl->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&valuePattern));
    if (FAILED(res))
    {
        std::wcout << "Failed to get value pattern" << std::endl;
        return -1;
    }
    valuePattern->SetValue(val.data());

    BOOL isReadOnly;
    res = valuePattern->get_CurrentIsReadOnly(&isReadOnly);
    if (isReadOnly)
    {
        std::wcerr << "Selected value is read only" << std::endl;
        return -1;
    }

    return 0;
}
