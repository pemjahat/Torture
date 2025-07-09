#pragma once

#include "PCH.h"

class RenderApplication;

class WindowApplication
{
public:
    static int Run(RenderApplication* pApp);
    static HWND GetHwnd() { return m_hwnd; }
private:
    static HWND m_hwnd;
};