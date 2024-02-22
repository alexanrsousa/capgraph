#ifndef __CAPGRAPH_RECTWINDOW_H__
#define __CAPGRAPH_RECTWINDOW_H__
#include <windows.h>
#include <memory>
#include <functional>
#include "window.h"

class RectWindow: public Window {
public:
    std::function<void(const RECT&)> OnSetCaptureRect;
    static std::shared_ptr<RectWindow> Create();

    void StartSelecting();
    void StartDrag(POINT position);
    void DoDrag(POINT position);
    void EndDrag(POINT position);
    void Paint(LPPAINTSTRUCT ps);

    RECT GetCaptureRect() const { return rCaptureRect; };
    bool HasSelectedArea() const { return rCaptureRect.bottom != rCaptureRect.top && rCaptureRect.left != rCaptureRect.top; }

    static void Register(HINSTANCE hInstance);
private:
    RectWindow();    
    RectWindow (const RectWindow&) = delete;
    RectWindow& operator= (const RectWindow&) = delete;

    RECT rCaptureRect;
    RECT rDraggingRect;

    static const WCHAR szClassName[];
    static HINSTANCE hInstance;
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

#endif