#ifndef __CAPGRAPH_MAINWINDOW_H__
#define __CAPGRAPH_MAINWINDOW_H__
#include <windows.h>
#include <memory>
#include "window.h"
#include "rectwindow.h"

class MainWindow: public Window {
public:
    static std::shared_ptr<MainWindow> Create(LPCWSTR szTitle);

    static void Register(HINSTANCE hInstance);
private:
    std::shared_ptr<RectWindow> pAreaSelector;
    HWND hbtnSetArea;
    HWND hbtnStartCapture;
    HWND hStatusBar;
    HFONT hCurrentFont;
    bool bStarted;

    void SelectAreaClick();
    void ToggleCaptureClick();
    void DoCapture();

    void GetMinMaxInfo(LPMINMAXINFO minMaxInfo);
    void UpdateChildrenPos(LPRECT clientArea);
    void UpdateFont();
    void DestroyCleanup();

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    MainWindow(LPCWSTR szTitle);    
    MainWindow (const MainWindow&) = delete;
    MainWindow& operator= (const MainWindow&) = delete;

    static const WCHAR szClassName[];
    static HINSTANCE hInstance;
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

#endif