#pragma once

#include "PCH.h"

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(uint32_t) + 1)

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

inline uint64_t AlignTo(uint64_t num, uint64_t alignment)
{
    return ((num + alignment - 1) / alignment) * alignment;
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
