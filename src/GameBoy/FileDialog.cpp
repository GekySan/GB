#include "FileDialog.hpp"
#include <windows.h>
#include <commdlg.h>
#include <string>

std::string OpenFileDialog() {
    OPENFILENAMEW ofn;
    wchar_t szFileName[MAX_PATH] = L"";

    wchar_t szFilter[] = L"Rom GameBoy (*.gb)\0*.gb\0Tout les fichiers (*.*)\0*.*\0\0";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = szFilter;
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Sélectionnez un fichier ROM";
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, szFileName, -1, NULL, 0, NULL, NULL);
        std::string filePath(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, szFileName, -1, &filePath[0], size_needed, NULL, NULL);
        if (!filePath.empty() && filePath.back() == '\0') {
            filePath.pop_back();
        }
        return filePath;
    }
    else {
        return "";
    }
}
