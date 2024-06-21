#include "utils.hpp"
#include <Psapi.h>

std::optional<std::wstring> getProcessName(const DWORD processID)
{
    const HandleGuard hg(OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID));
    if (!hg.get()) return std::nullopt;

    std::wstring processName(MAX_PATH, L'\0');
    const DWORD nameSize = GetModuleBaseNameW(hg.get(), 0, &processName[0], processName.length());
    if (!nameSize) return std::nullopt;

    processName.resize(nameSize);
    return processName;
}

std::optional<DWORD> getProcessID(std::wstring_view name)
{
    DWORD processes[1024], bytesReturned;
    if (!EnumProcesses(processes, sizeof(processes), &bytesReturned))
        return std::nullopt;

    const DWORD amount = bytesReturned / sizeof(DWORD);
    for (size_t i = 0; i < amount; ++i)
    {
        const DWORD processID = processes[i];
        if (!processID) continue;

        const std::optional<std::wstring> currentProcessName = getProcessName(processID);
        if (currentProcessName.has_value() && currentProcessName.value() == name)
            return processID;
    }

    return std::nullopt;
}

std::optional<BOOL> isPatternReadOnly(IUIAutomationValuePattern *pattern)
{
    BOOL result;
    HRESULT hr = pattern->get_CurrentIsReadOnly(&result);
    if (FAILED(hr))
        return std::nullopt;

    return result;
}

std::wstring modifyRequest(std::wstring_view request)
{
    const std::wstring toFind = L"q=", toInsert = L"test:";
    const std::wstring result = std::wstring(request).insert(request.find(toFind) + toFind.size(), toInsert);
    return result;
}

UiaGuard<IUIAutomationCondition> createCondition(IUIAutomation *automation, const DWORD processID)
{
    VARIANT variant;
    VariantInit(&variant);
    variant.vt = VT_I4;
    variant.lVal = processID;

    UiaGuard<IUIAutomationCondition> condition(nullptr);
    HRESULT result = automation->CreatePropertyCondition(UIA_ProcessIdPropertyId, variant, condition.ptr());
    VariantClear(&variant);

    return condition;
}

UiaGuard<IUIAutomationCondition> createCondition(const CONTROLTYPEID id, IUIAutomation *automation)
{
    VARIANT variant;
    VariantInit(&variant);
    variant.vt = VT_I4;
    variant.lVal = id;

    UiaGuard<IUIAutomationCondition> condition(nullptr);
    HRESULT result = automation->CreatePropertyCondition(UIA_ControlTypePropertyId, variant, condition.ptr());
    VariantClear(&variant);

    return condition;
}
