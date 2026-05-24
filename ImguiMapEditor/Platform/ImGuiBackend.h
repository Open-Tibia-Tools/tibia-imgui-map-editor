#pragma once
#include "IWindow.h"
namespace MapEditor {
namespace Platform {

/**
 * ImGui backend initialization and shutdown.
 * 
 * Separates ImGui platform/renderer backend setup from Application.
 */
class ImGuiBackend {
public:
    ImGuiBackend() = default;
    ~ImGuiBackend();
    
    // Non-copyable
    ImGuiBackend(const ImGuiBackend&) = delete;
    ImGuiBackend& operator=(const ImGuiBackend&) = delete;
    
    /**
     * Initialize ImGui with GLFW and OpenGL3 backends.
     * @param window The platform window to attach to
     * @param ini_path Optional path to imgui.ini file (must be stable pointer)
     * @return true if successful
     */
    bool initialize(IWindow& window, const char* ini_path = nullptr);
    
    /**
     * Shutdown ImGui backends.
     */
    void shutdown();
    
    /**
     * Begin a new ImGui frame.
     */
    static void newFrame();
    
    /**
     * Render ImGui draw data.
     */
    static void renderDrawData();
    
    bool isInitialized() const { return initialized_; }

private:
    bool initialized_ = false;
};

} // namespace Platform
} // namespace MapEditor
