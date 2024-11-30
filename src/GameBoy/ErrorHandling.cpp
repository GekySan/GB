#include "ErrorHandling.hpp"

void ShowErrorMessage(const std::wstring& message, const std::wstring& title) {
    MessageBoxW(NULL, message.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
}

void ShowWarningMessage(const std::wstring& message, const std::wstring& title) {
    MessageBoxW(NULL, message.c_str(), title.c_str(), MB_ICONWARNING | MB_OK);
}

void ShowInfoMessage(const std::wstring& message, const std::wstring& title) {
    MessageBoxW(NULL, message.c_str(), title.c_str(), MB_ICONINFORMATION | MB_OK);
}
