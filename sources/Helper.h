#pragma once

#include <cassert>
#include <windows.h>
#include <string>

inline void CheckHRESULT(HRESULT hr = S_OK)
{
    if (FAILED(hr))
    {
        std::string fullMessage = "(HRESULT: 0x" + std::to_string(hr) + ")";
        assert(false && fullMessage.c_str());
    }
}

inline std::string WStringToString(const std::wstring& wstr)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size_needed, nullptr, nullptr);
    return str;
}

//void LogError(const char* message, HRESULT hr = S_OK)
//{
//    std::string errorMsg = message;
//    if (hr != S_OK) {
//        char hrMsg[512];
//        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, hrMsg, sizeof(hrMsg), nullptr);
//        errorMsg += " HRESULT: 0x" + std::to_string(hr) + " - " + hrMsg;
//    }
//    errorMsg += "\n";
//    std::cerr << errorMsg;
//}
