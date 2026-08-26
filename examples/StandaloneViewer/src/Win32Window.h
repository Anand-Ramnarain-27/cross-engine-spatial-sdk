#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <string>

namespace viewer
{
    struct MouseDelta
    {
        float dx = 0.0f;
        float dy = 0.0f;
    };

    // Thin Win32 window + input wrapper: message pump, key state array, and
    // Quake-style mouselook (hold right mouse button, cursor hidden and
    // re-centered every move so it never hits a screen edge). All
    // platform-specific window/input code for the viewer lives here, kept
    // isolated from the renderer and the SDK-facing main loop.
    class Win32Window
    {
    public:
        Win32Window(const std::wstring& title, int width, int height);
        ~Win32Window();

        Win32Window(const Win32Window&) = delete;
        Win32Window& operator=(const Win32Window&) = delete;

        // Pumps pending Win32 messages. Returns false once the window has been closed.
        bool processMessages();

        [[nodiscard]] HWND handle() const noexcept { return m_hwnd; }
        [[nodiscard]] int width() const noexcept { return m_width; }
        [[nodiscard]] int height() const noexcept { return m_height; }

        // True once after a resize, until the caller reads it.
        [[nodiscard]] bool consumeResized() noexcept;

        [[nodiscard]] bool isKeyDown(int virtualKey) const noexcept;
        // True only on the frame the key transitioned from up to down.
        [[nodiscard]] bool consumeKeyPressed(int virtualKey) noexcept;

        [[nodiscard]] bool isMouseCaptured() const noexcept { return m_mouseCaptured; }
        [[nodiscard]] MouseDelta consumeMouseDelta() noexcept;

        void setTitle(const std::wstring& title);

    private:
        static LRESULT CALLBACK wndProcThunk(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
        LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

        void beginMouseCapture();
        void endMouseCapture();

        HWND m_hwnd = nullptr;
        int m_width = 0;
        int m_height = 0;
        bool m_shouldClose = false;
        bool m_resized = false;

        std::array<bool, 256> m_keysDown{};
        std::array<bool, 256> m_keysPressedThisFrame{};

        bool m_mouseCaptured = false;
        POINT m_captureCenter{};
        float m_mouseDeltaX = 0.0f;
        float m_mouseDeltaY = 0.0f;
    };
}
