#pragma once

#include "guards.hpp"

#include <string_view>
#include <optional>

#include <Windows.h>
#include <UIAutomation.h>


std::optional<std::wstring> getProcessName(const DWORD processID);
std::optional<DWORD> getProcessID(std::wstring_view name);

UiaGuard<IUIAutomationCondition> createCondition(IUIAutomation *automation, const DWORD processID);
UiaGuard<IUIAutomationCondition> createCondition(const CONTROLTYPEID id, IUIAutomation *automation);
std::optional<BOOL> isPatternReadOnly(IUIAutomationValuePattern *pattern);

std::wstring modifyRequest(std::wstring_view request);
