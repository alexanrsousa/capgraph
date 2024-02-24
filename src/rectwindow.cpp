#include "rectwindow.h"
#include <windowsx.h>

HINSTANCE RectWindow::hInstance = NULL;
const WCHAR RectWindow::szClassName[] = L"CapGraphRect";

enum {
    BID_SETAREA = 100,
};

std::shared_ptr<RectWindow> RectWindow::Create() {
    return std::shared_ptr<RectWindow>(new RectWindow());
}

RectWindow::RectWindow()
    : rCaptureRect({0, 0, 0, 0})
    , rDraggingRect({0, 0, 0, 0}) {
    hWindow = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED, RectWindow::szClassName, nullptr, WS_POPUP,
                              CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, RectWindow::hInstance, this);
    SetLayeredWindowAttributes(hWindow, RGB(255, 0, 0), 127, LWA_ALPHA | LWA_COLORKEY);
}

void RectWindow::StartSelecting() {
    auto x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    auto y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    auto cx = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    auto cy = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    rDraggingRect = {0, 0, 0, 0};
    SetWindowPos(hWindow, HWND_TOPMOST, x, y, cx, cy, 0);
    Show(SW_SHOW);
}

void RectWindow::StartDrag(POINT position) {
    SetCapture(hWindow);
    rDraggingRect.top = rDraggingRect.bottom = position.y;
    rDraggingRect.left = rDraggingRect.right = position.x;
}

void RectWindow::DoDrag(POINT position) {
    rDraggingRect.bottom = position.y;
    rDraggingRect.right = position.x;
    Redraw(false);
}

void RectWindow::EndDrag(POINT position) {
    ReleaseCapture();
    rDraggingRect.bottom = position.y;
    rDraggingRect.right = position.x;
    if (rDraggingRect.bottom >= rDraggingRect.top) {
        rCaptureRect.bottom = rDraggingRect.bottom;
        rCaptureRect.top = rDraggingRect.top;
    } else {
        rCaptureRect.bottom = rDraggingRect.top;
        rCaptureRect.top = rDraggingRect.bottom;
    }
    if (rDraggingRect.right >= rDraggingRect.left) {
        rCaptureRect.right = rDraggingRect.right;
        rCaptureRect.left = rDraggingRect.left;
    } else {
        rCaptureRect.right = rDraggingRect.left;
        rCaptureRect.left = rDraggingRect.right;
    }
    Show(SW_HIDE);
}

void RectWindow::Paint(LPPAINTSTRUCT ps) {
    if (rDraggingRect.left != rDraggingRect.right && rDraggingRect.top != rDraggingRect.bottom) {
        auto dpi = GetDpiForWindow(hWindow);
        HBRUSH brush = CreateSolidBrush(RGB(255, 0, 0));
        HPEN pen = CreatePen(PS_SOLID, ScaleToDPI(2, dpi), RGB(255, 255, 255));
        HBRUSH oldBrush = (HBRUSH)SelectObject(ps->hdc, (HGDIOBJ)brush);
        HPEN oldPen = (HPEN)SelectObject(ps->hdc, (HGDIOBJ)pen);
        HBRUSH background = CreateSolidBrush(RGB(0, 0, 0));

        Rectangle(ps->hdc, rDraggingRect.left, rDraggingRect.top, rDraggingRect.right, rDraggingRect.bottom);
        ExcludeClipRect(ps->hdc, rDraggingRect.left, rDraggingRect.top, rDraggingRect.right, rDraggingRect.bottom);
        FillRect(ps->hdc, &ps->rcPaint, background);

        SelectObject(ps->hdc, (HGDIOBJ)oldBrush);
        SelectObject(ps->hdc, (HGDIOBJ)oldPen);
        DeleteObject(pen);
        DeleteObject(brush);
        DeleteObject(background);
    }
}

void RectWindow::Register(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;
    RectWindow::hInstance = hInstance;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = RectWindow::WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = NULL;
    wcex.hCursor = LoadCursor(nullptr, IDC_CROSS);
    wcex.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = RectWindow::szClassName;
    wcex.hIconSm = NULL;

    RegisterClassExW(&wcex);
}

LRESULT CALLBACK RectWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ACTIVATE:
        if (wParam == WA_INACTIVE) {
            ShowWindow(hWnd, SW_HIDE);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        RectWindow* window = (RectWindow*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        if (window) {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            window->StartDrag(pt);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        RectWindow* window = (RectWindow*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        if ((wParam & MK_LBUTTON) && window) {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            window->DoDrag(pt);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        RectWindow* window = (RectWindow*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        if (window) {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            window->EndDrag(pt);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        RectWindow* window = (RectWindow*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        HDC dc = BeginPaint(hWnd, &ps);
        if (dc && window) {
            window->Paint(&ps);
        }
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_CREATE: {
        LPCREATESTRUCTW createStruct = (LPCREATESTRUCTW)lParam;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)createStruct->lpCreateParams);
        break;
    }
    case WM_DESTROY: {
        RectWindow* window = (RectWindow*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        if (window) {
            window->hWindow = 0;
        }
        return 0;
    }
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}