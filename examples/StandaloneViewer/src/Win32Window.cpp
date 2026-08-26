#include "Win32Window.h"

#include <stdexcept>

namespace viewer
{
    namespace
    {
        constexpr const wchar_t* kWindowClassName = L"SpatialSDKStandaloneViewerWindowClass";
    }

    Win32Window::Win32Window(const std::wstring& title, int width, int height) : m_width(width), m_height(height)
    {
        const HINSTANCE instance = GetModuleHandleW(nullptr);

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(WNDCLASSEXW);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = &Win32Window::wndProcThunk;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = kWindowClassName;
        RegisterClassExW(&windowClass);

        RECT rect{0, 0, width, height};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        m_hwnd = CreateWindowExW(
            0, kWindowClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
            nullptr, nullptr, instance, this);

        if (m_hwnd == nullptr)
        {
            throw std::runtime_error("Failed to create the viewer window");
        }

        ShowWindow(m_hwnd, SW_SHOW);
    }

    Win32Window::~Win32Window()
    {
        if (m_hwnd != nullptr)
        {
            DestroyWindow(m_hwnd);
        }
        UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
    }

    bool Win32Window::processMessages()
    {
        m_keysPressedThisFrame.fill(false);
        m_mouseDeltaX = 0.0f;
        m_mouseDeltaY = 0.0f;

        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return !m_shouldClose;
    }

    bool Win32Window::consumeResized() noexcept
    {
        const bool resized = m_resized;
        m_resized = false;
        return resized;
    }

    bool Win32Window::isKeyDown(int virtualKey) const noexcept
    {
        return m_keysDown[static_cast<std::size_t>(virtualKey) & 0xFF];
    }

    bool Win32Window::consumeKeyPressed(int virtualKey) noexcept
    {
        return m_keysPressedThisFrame[static_cast<std::size_t>(virtualKey) & 0xFF];
    }

    MouseDelta Win32Window::consumeMouseDelta() noexcept { return MouseDelta{m_mouseDeltaX, m_mouseDeltaY}; }

    void Win32Window::setTitle(const std::wstring& title) { SetWindowTextW(m_hwnd, title.c_str()); }

    void Win32Window::beginMouseCapture()
    {
        if (m_mouseCaptured)
        {
            return;
        }
        m_mouseCaptured = true;
        SetCapture(m_hwnd);
        ShowCursor(FALSE);

        RECT rect{};
        GetWindowRect(m_hwnd, &rect);
        m_captureCenter = POINT{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
        SetCursorPos(m_captureCenter.x, m_captureCenter.y);
    }

    void Win32Window::endMouseCapture()
    {
        if (!m_mouseCaptured)
        {
            return;
        }
        m_mouseCaptured = false;
        ReleaseCapture();
        ShowCursor(TRUE);
    }

    LRESULT CALLBACK Win32Window::wndProcThunk(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        Win32Window* self = nullptr;
        if (msg == WM_NCCREATE)
        {
            auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = static_cast<Win32Window*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        else
        {
            self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (self != nullptr)
        {
            return self->handleMessage(hwnd, msg, wparam, lparam);
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    LRESULT Win32Window::handleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        switch (msg)
        {
            case WM_CLOSE:
            case WM_DESTROY:
                m_shouldClose = true;
                return 0;

            case WM_SIZE:
                m_width = LOWORD(lparam);
                m_height = HIWORD(lparam);
                m_resized = true;
                return 0;

            case WM_KEYDOWN:
            {
                const auto key = static_cast<std::size_t>(wparam) & 0xFF;
                const bool wasDown = (lparam & (1 << 30)) != 0; // auto-repeat flag
                m_keysDown[key] = true;
                if (!wasDown)
                {
                    m_keysPressedThisFrame[key] = true;
                }
                return 0;
            }

            case WM_KEYUP:
                m_keysDown[static_cast<std::size_t>(wparam) & 0xFF] = false;
                return 0;

            case WM_RBUTTONDOWN:
                beginMouseCapture();
                return 0;

            case WM_RBUTTONUP:
                endMouseCapture();
                return 0;

            case WM_MOUSEMOVE:
                if (m_mouseCaptured)
                {
                    POINT cursor{};
                    GetCursorPos(&cursor);
                    m_mouseDeltaX += static_cast<float>(cursor.x - m_captureCenter.x);
                    m_mouseDeltaY += static_cast<float>(cursor.y - m_captureCenter.y);
                    SetCursorPos(m_captureCenter.x, m_captureCenter.y);
                }
                return 0;

            case WM_KILLFOCUS:
                endMouseCapture();
                return 0;

            default:
                return DefWindowProcW(hwnd, msg, wparam, lparam);
        }
    }
}
