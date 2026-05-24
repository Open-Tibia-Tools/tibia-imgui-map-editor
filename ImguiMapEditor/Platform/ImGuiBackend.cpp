#include "ImGuiBackend.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Platform {

ImGuiBackend::~ImGuiBackend() {
    shutdown();
}

bool ImGuiBackend::initialize(IWindow& window, const char* ini_path) {
    if (initialized_) {
        return true;
    }
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // Optional
    
    // Set custom ini path if provided (must be stable pointer)
    if (ini_path && ini_path[0] != '\0') {
        io.IniFilename = ini_path;
    }
    
    // Load default font and merge FontAwesome icons
    io.Fonts->AddFontDefault();
    
    // Merge FontAwesome icons into the default font
    ImFontConfig config;
    config.MergeMode = true;
    config.GlyphMinAdvanceX = 13.0f; // Monospace icons
    static const ImWchar icon_ranges[] = { 0xe005, 0xf8ff, 0 }; // FontAwesome 6 range
    io.Fonts->AddFontFromFileTTF("data/fonts/fa-solid-900.ttf", 13.0f, &config, icon_ranges);
    
    // Set dark theme
    ImGui::StyleColorsDark();
    
    // Setup platform/renderer backends
    GLFWwindow* glfw_window = static_cast<GLFWwindow*>(window.getNativeHandle());
    ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
    
    // Select appropriate GLSL version based on GL version
    int const gl_major = window.getGLVersionMajor();
    int const gl_minor = window.getGLVersionMinor();
    
    const char* glsl_version = "#version 330";
    if (gl_major >= 4 && gl_minor >= 6) {
        glsl_version = "#version 460";
    } else if (gl_major >= 4 && gl_minor >= 3) {
        glsl_version = "#version 430";
    }
    
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    spdlog::info("ImGui initialized with {}", glsl_version);
    initialized_ = true;
    return true;
}

void ImGuiBackend::shutdown() {
    if (!initialized_) {
        return;
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    initialized_ = false;
}

void ImGuiBackend::newFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiBackend::renderDrawData() {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace Platform
} // namespace MapEditor
