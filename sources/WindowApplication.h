#pragma once

#include "RenderApplication.h"

class WindowApplication
{
public:
    static int Run(RenderApplication* pApp);
    static HWND GetHwnd() { return m_hwnd; }
private:
    static HWND m_hwnd;
};