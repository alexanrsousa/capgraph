#ifndef __CAPGRAPH_MAINWINDOW_H__
#define __CAPGRAPH_MAINWINDOW_H__
#include "rectwindow.h"
#include "window.h"
#include <memory>
#include <vector>
#include <windows.h>

enum class CaptureStatus {
    NotStarted,
    WaitingStillImage,
    StillImage,
};

struct CaptureItem {
    SYSTEMTIME stTimestamp;
    COLORREF cAvgColor;
};

class MainWindow : public Window {
public:
    static std::shared_ptr<MainWindow> Create(LPCWSTR szTitle);

    static void Register(HINSTANCE hInstance);

private:
    std::vector<CaptureItem> vColorItems;
    std::shared_ptr<RectWindow> pAreaSelector;
    std::vector<uint32_t> vCaptureBuffer;
    FILETIME ftLastChangedImage;
    HWND hbtnSetArea;
    HWND hbtnStartCapture;
    HWND hStatusBar;
    HWND hlvDataList;
    HFONT hCurrentFont;
    CaptureStatus csCapStatus;

    void SelectAreaClick();
    void ToggleCaptureClick();
    void DoCapture();

    void SetupColumns();
    void InsertCaptureItem(const CaptureItem& item);

    void GetMinMaxInfo(LPMINMAXINFO minMaxInfo);
    void UpdateChildrenPos(LPRECT clientArea);
    void UpdateFont();
    void DestroyCleanup();

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    MainWindow(LPCWSTR szTitle);
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    static const WCHAR szClassName[];
    static HINSTANCE hInstance;
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

#endif