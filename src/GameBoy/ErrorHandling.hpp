#pragma once

#include <string>
#include <windows.h>

void ShowErrorMessage(const std::wstring& message, const std::wstring& title);
void ShowWarningMessage(const std::wstring& message, const std::wstring& title);
void ShowInfoMessage(const std::wstring& message, const std::wstring& title);
