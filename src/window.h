#ifndef __CAPGRAPH_WINDOW_H__
#define __CAPGRAPH_WINDOW_H__
#include <windows.h>

class Window {
public:
    HWND GetHandle() const { return hWindow; }
    BOOL Show(int nCmdShow) { return ShowWindow(hWindow, nCmdShow); }
    BOOL Update() { return UpdateWindow(hWindow); }
    BOOL Redraw(bool erase = true) { return RedrawWindow(hWindow, NULL, NULL, RDW_INVALIDATE | (erase ? RDW_ERASE : 0)); }
protected:
    HWND hWindow;

    Window(HWND hWindow = NULL): hWindow(hWindow) {};    
    Window(const Window&) = delete;
    Window& operator= (const Window&) = delete;
};

inline int ScaleToDPI(int dimension, unsigned int dpi)
{
    return dimension * dpi / 96;
}

#endif