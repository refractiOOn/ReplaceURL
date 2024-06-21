#include "utils.hpp"
#include "scope_guard.hpp"
#include <iostream>

int main()
{
    IUIAutomation *automation = nullptr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    HRESULT hr = CoCreateInstance(__uuidof(CUIAutomation), NULL, CLSCTX_INPROC_SERVER,
                                  __uuidof(IUIAutomation), (void**)&automation);
    if (FAILED(hr))
    {
        std::wcerr << "Failed to initialize UI Automation" << std::endl;
        return -1;
    }
    auto automationGuard = sg::make_scope_guard([automation]
    {
        automation->Release();
        CoUninitialize();
    });

    UiaGuard<IUIAutomationElement> root(nullptr);
    hr = automation->GetRootElement(root.ptr());
    if (FAILED(hr))
    {
        std::wcerr << "Failed to get root element" << std::endl;
        return -1;
    }

    const std::wstring browserName = L"msedge.exe";
    const std::optional<DWORD> processID = getProcessID(browserName);
    if (!processID.has_value())
    {
        std::wcerr << "Specified process not found" << std::endl;
        return -1;
    }

    UiaGuard<IUIAutomationCondition> browserCond = createCondition(automation, processID.value());
    if (!browserCond.value())
    {
        std::wcerr << "Could not create condition by process ID" << std::endl;
        return -1;
    }

    UiaGuard<IUIAutomationElement> browser = nullptr;
    hr = root.value()->FindFirst(TreeScope_Children, browserCond.value(), browser.ptr());
    if (FAILED(hr) || !browser.value())
    {
        std::wcerr << "Specified browser not found" << std::endl;
        return -1;
    }

    UiaGuard<IUIAutomationCondition> controlCond = createCondition(UIA_EditControlTypeId, automation);
    if (!controlCond.value())
    {
        std::wcerr << "Could not create condition by control type" << std::endl;
        return -1;
    }

    UiaGuard<IUIAutomationElement> editControl = nullptr;
    hr = browser.value()->FindFirst(TreeScope_Subtree, controlCond.value(), editControl.ptr());
    if (FAILED(hr) || !editControl.value())
    {
        std::wcerr << "Edit control not found" << std::endl;
        return -1;
    }

    VARIANT value;
    hr = editControl.value()->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &value);
    if (FAILED(hr))
    {
        std::wcout << L"Failed to get property value." << std::endl;
        return -1;
    }
    VariantClear(&value);

    UiaGuard<IUIAutomationValuePattern> valuePattern = nullptr;
    hr = editControl.value()->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(valuePattern.ptr()));
    if (FAILED(hr))
    {
        std::wcerr << "Could not get value pattern" << std::endl;
        return -1;
    }

    const std::optional<bool> readOnly = isPatternReadOnly(valuePattern.value());
    if (!readOnly.has_value())
    {
        std::wcerr << "Could not get read only property" << std::endl;
        return -1;
    }

    if (readOnly.value())
    {
        std::wcerr << "The value pattern is read only" << std::endl;
        return -1;
    }

    std::wstring newVal = modifyRequest(value.bstrVal);
    hr = valuePattern.value()->SetValue(newVal.data());
    if (FAILED(hr))
    {
        std::wcerr << "Could not set a new value" << std::endl;
        return -1;
    }

    return 0;
}
