#include "WindowDragHandler.h"

#include "ui/DesktopIndicator.h"
#include "util/Log.h"

namespace {

const HCURSOR s_hArrow    = LoadCursor(nullptr, IDC_ARROW);
const HCURSOR s_hSizeNWSE = LoadCursor(nullptr, IDC_SIZENWSE);
const HCURSOR s_hSizeNESW = LoadCursor(nullptr, IDC_SIZENESW);
const HCURSOR s_hSizeNS   = LoadCursor(nullptr, IDC_SIZENS);
const HCURSOR s_hSizeWE   = LoadCursor(nullptr, IDC_SIZEWE);

bool IsResizeCursor(HCURSOR hCursor) {
    return hCursor == s_hSizeNWSE || hCursor == s_hSizeNESW ||
           hCursor == s_hSizeNS || hCursor == s_hSizeWE;
}

HCURSOR GetCurrentCursor() {
    CURSORINFO ci = {.cbSize = sizeof(ci)};
    return (GetCursorInfo(&ci) != 0) ? ci.hCursor : nullptr;
}

bool SameRectSize(const RECT &a, const RECT &b) {
    return (a.right - a.left) == (b.right - b.left) &&
           (a.bottom - a.top) == (b.bottom - b.top);
}

} // namespace

WindowDragHandler *WindowDragHandler::s_instance = nullptr;

WindowDragHandler::WindowDragHandler(DesktopIndicator *indicator)
    : m_indicator(indicator) {
    s_instance = this;
}

WindowDragHandler::~WindowDragHandler() {
    UninstallHook();
    if (s_instance == this) { s_instance = nullptr; }
}

bool WindowDragHandler::InstallHook(HWND hwndTarget) {
    m_hwndTarget = hwndTarget;
    if (m_hHook == nullptr) {
        m_hHook = SetWindowsHookExW(WH_MOUSE_LL, DragHookProc,
                                    GetModuleHandleW(nullptr), 0);
        if (m_hHook != nullptr) {
            Log(L"[INFO] WindowDragHandler hook installed");
        } else {
            Log(L"[ERROR] WindowDragHandler hook install failed");
        }
    }
    if (m_hEventHook == nullptr) {
        m_hEventHook = SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND,
                                       nullptr, MoveSizeEventProc, 0, 0,
                                       WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        if (m_hEventHook != nullptr) {
            Log(L"[INFO] WindowDragHandler WinEvent hook installed");
        } else {
            Log(L"[ERROR] WindowDragHandler WinEvent hook install failed");
        }
    }
    return m_hHook != nullptr;
}

void WindowDragHandler::UninstallHook() {
    if (m_hEventHook != nullptr) {
        UnhookWinEvent(m_hEventHook);
        m_hEventHook = nullptr;
        Log(L"[INFO] WindowDragHandler WinEvent hook uninstalled");
    }
    if (m_hHook != nullptr) {
        UnhookWindowsHookEx(m_hHook);
        m_hHook = nullptr;
        Log(L"[INFO] WindowDragHandler hook uninstalled");
    }
}

void WindowDragHandler::BeginDrag(HWND hwnd, bool isNative) {
    m_clickPending     = false;
    m_dragHwnd         = hwnd;
    m_isDraggingWindow = true;
    m_isNativeDrag     = isNative;
    m_enableDragSwitch = IsDragSwitch();
    if (m_enableDragSwitch) {
        m_indicator->ShowDragOverlay();
        m_indicator->SetDraggingWindow(true);
    }
}

void WindowDragHandler::UpdateDragHover(POINT pt) {
    int symIdx = -1;
    if (!m_indicator->GetSymbolIndexAt(pt, symIdx)) {
        m_lastHoverSymbol = -1;
        return;
    }
    if (symIdx != m_lastHoverSymbol) {
        m_lastHoverSymbol = symIdx;
        if (m_enableDragSwitch) {
            Log(L"[INFO] MoveWindow: desktop " + std::to_wstring(symIdx));
            PostMessageW(m_hwndTarget, WM_WINDOW_DRAG_DROP,
                         static_cast<WPARAM>(symIdx),
                         reinterpret_cast<LPARAM>(m_dragHwnd));
        }
    }
}

void WindowDragHandler::OnMoveSizeStart(HWND hwnd) {
    if (m_indicator->IsEditMode() || IsWindow(hwnd) == 0) {
        return;
    }
    if (GetAncestor(hwnd, GA_ROOT) != hwnd) {
        return;
    }
    if (m_isDraggingWindow && hwnd != m_dragHwnd) {
        ResetDragState();
    }

    m_eventMoveSize    = true;
    m_eventHwnd        = hwnd;
    m_hasEventLastRect = (GetWindowRect(hwnd, &m_eventLastRect) != 0);
}

void WindowDragHandler::OnMoveSizeEnd() {
    m_eventMoveSize    = false;
    m_eventHwnd        = nullptr;
    m_hasEventLastRect = false;
    ResetDragState();
}

void WindowDragHandler::ResetDragState() {
    if (m_isDraggingWindow) {
        m_indicator->HideDragOverlay();
        m_indicator->SetDraggingWindow(false);
    }
    m_clickPending     = false;
    m_isDraggingWindow = false;
    m_isNativeDrag     = false;
    m_dragHwnd         = nullptr;
    m_lastHoverSymbol  = -1;
    m_enableDragSwitch = false;
}

bool WindowDragHandler::IsDragSwitch() const {
    if (m_dragMode == DragSwitchMode::Always) { return true; }
    if (m_dragMode == DragSwitchMode::Never) { return false; }
    int vk = (m_dragMode == DragSwitchMode::Alt) ? VK_MENU : VK_CONTROL;
    return (static_cast<UINT>(GetAsyncKeyState(vk)) & 0x8000u) != 0;
}

void CALLBACK
WindowDragHandler::MoveSizeEventProc(HWINEVENTHOOK /*hWinEventHook*/,
                                     DWORD event,
                                     HWND  hwnd,
                                     LONG  idObject,
                                     LONG  idChild,
                                     DWORD /*dwEventThread*/,
                                     DWORD /*dwmsEventTime*/) {
    if (s_instance == nullptr || idObject != OBJID_WINDOW ||
        idChild != CHILDID_SELF || hwnd == nullptr) {
        return;
    }

    if (event == EVENT_SYSTEM_MOVESIZESTART) {
        s_instance->OnMoveSizeStart(hwnd);
    } else if (event == EVENT_SYSTEM_MOVESIZEEND &&
               (hwnd == s_instance->m_eventHwnd ||
                hwnd == s_instance->m_dragHwnd)) {
        s_instance->OnMoveSizeEnd();
    }
}

bool WindowDragHandler::IsDragStillActive(bool inNativeMoveSize) const {
    if (m_isNativeDrag) {
        return inNativeMoveSize;
    }
    return m_eventMoveSize && m_dragHwnd != nullptr &&
           IsWindow(m_dragHwnd) != 0 && IsIconic(m_dragHwnd) == 0;
}

void WindowDragHandler::TryBeginEventDrag() {
    if (!m_eventMoveSize) {
        return;
    }
    if (m_eventHwnd == nullptr || IsWindow(m_eventHwnd) == 0) {
        OnMoveSizeEnd();
        return;
    }

    RECT rc{};
    if (GetWindowRect(m_eventHwnd, &rc) == 0) {
        return;
    }

    if (!m_hasEventLastRect) {
        m_eventLastRect    = rc;
        m_hasEventLastRect = true;
        return;
    }

    const bool moved =
        (rc.left != m_eventLastRect.left || rc.top != m_eventLastRect.top);
    const bool sameSize = SameRectSize(rc, m_eventLastRect);
    m_eventLastRect     = rc;

    if (moved && sameSize && !IsResizeCursor(GetCurrentCursor())) {
        BeginDrag(m_eventHwnd, false);
    }
}

void WindowDragHandler::OnMouseMove(POINT pt) {
    if (!m_clickPending && !m_isDraggingWindow && !m_eventMoveSize) {
        return;
    }

    GUITHREADINFO info = {.cbSize = sizeof(info)};
    const bool    inMoveSize =
        GetGUIThreadInfo(0, &info) != 0 && (info.flags & GUI_INMOVESIZE) != 0;

    if (m_isDraggingWindow) {
        if (!IsDragStillActive(inMoveSize)) {
            OnMoveSizeEnd();
        } else {
            UpdateDragHover(pt);
        }
        return;
    }

    if (inMoveSize) {
        if (GetCurrentCursor() == s_hArrow) {
            BeginDrag(info.hwndMoveSize, true);
        }
        return;
    }

    TryBeginEventDrag();
}

LRESULT CALLBACK WindowDragHandler::DragHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || s_instance == nullptr) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    auto *hs = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)

    if (wParam == WM_LBUTTONDOWN) {
        if (s_instance->m_isDraggingWindow && !s_instance->m_isNativeDrag) {
            s_instance->OnMoveSizeEnd();
        }
        if (!s_instance->m_indicator->IsPtOnOverlay(hs->pt)) {
            s_instance->m_clickPending = true;
        }
    } else if (wParam == WM_MOUSEMOVE) {
        s_instance->OnMouseMove(hs->pt);
    } else if (wParam == WM_LBUTTONUP) {
        s_instance->OnMoveSizeEnd();
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
