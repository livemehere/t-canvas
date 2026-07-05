#include "ImGuiLayer.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace {
void LoadSystemFont() {
    ImGuiIO &io = ImGui::GetIO();

    constexpr float fontSize = 15.0f;
    const char *fontPaths[] = {
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/AppleSDGothicNeo.ttc"
    };

    for (const char *path: fontPaths) {
        if (io.Fonts->AddFontFromFileTTF(path, fontSize)) {
            return;
        }
    }

    io.Fonts->AddFontDefault();
}

void ApplyDarkEditorStyle() {
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *colors = style.Colors;

    style.WindowPadding = ImVec2(14.0f, 12.0f);
    style.FramePadding = ImVec2(9.0f, 6.0f);
    style.CellPadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);
    style.ItemInnerSpacing = ImVec2(7.0f, 5.0f);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 5.0f;

    colors[ImGuiCol_Text] = ImVec4(0.88f, 0.90f, 0.93f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.48f, 0.52f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.105f, 0.112f, 0.125f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.085f, 0.092f, 0.105f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.105f, 0.112f, 0.125f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.145f, 0.155f, 0.175f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.205f, 0.23f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.105f, 0.112f, 0.125f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.125f, 0.135f, 0.155f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.105f, 0.112f, 0.125f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.09f, 0.095f, 0.105f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.29f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.39f, 0.44f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.44f, 0.48f, 0.54f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.62f, 0.74f, 0.82f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.46f, 0.55f, 0.62f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.62f, 0.74f, 0.82f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.15f, 0.17f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.23f, 0.27f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.29f, 0.34f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.29f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.31f, 0.36f, 0.43f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.44f, 0.52f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.38f, 0.44f, 0.52f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.50f, 0.58f, 0.66f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.46f, 0.55f, 0.62f, 0.15f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.46f, 0.55f, 0.62f, 0.35f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.62f, 0.74f, 0.82f, 0.60f);
    colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.31f, 0.36f, 0.43f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.25f, 0.29f, 0.35f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.22f, 0.29f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.55f, 0.58f, 0.62f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.62f, 0.74f, 0.82f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.46f, 0.55f, 0.62f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.62f, 0.74f, 0.82f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.14f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.24f, 0.26f, 0.30f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.18f, 0.20f, 0.23f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.38f, 0.44f, 0.52f, 0.45f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.62f, 0.74f, 0.82f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.62f, 0.74f, 0.82f, 0.65f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.55f);
}
} // namespace

bool ImGuiLayer::Init(GLFWwindow *window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    LoadSystemFont();
    ApplyDarkEditorStyle();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410 core");
    return true;
}

void ImGuiLayer::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiLayer::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
