#include "DebugUI.h"
#include "renderer.h"	// サブウィンドウ描画時の RTV 退避/復帰用

std::vector<std::function<void(void)>> DebugUI::m_debugfunction;

void DebugUI::Init(ID3D11Device* device, ID3D11DeviceContext* context) 
{

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // ドッキングを有効化
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // ★ウィンドウを実行画面外へ出せる

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // マルチビューポート時：分離ウィンドウを不透明・角丸なしにして見栄えを整える
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    io.Fonts->Clear();

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.MergeMode = false;

    // ���C���I or Yu Gothic �𐄏��iWindows 11 �W�����ځj
    io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\meiryo.ttc",
        18.0f,
        &cfg,
        io.Fonts->GetGlyphRangesJapanese()   // �� �������d�v
    );
    // ImGui 1.92 以降は新しいテクスチャ方式のため Build() を手動で呼ばない（自動構築）

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(Application::GetWindow());
    ImGui_ImplDX11_Init(device, context);
}

void DebugUI::DisposeUI() {
    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

// �f�o�b�O�\���֐��̓o�^
void DebugUI::RedistDebugFunction(std::function<void(void)> f) {
    m_debugfunction.push_back(std::move(f));
}

void DebugUI::Render() {
    // ImGui�̐V�����t���[�����J�n
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // �E�B���h�E�ƃf�o�b�O���̕`��
    ImGui::Begin("Debug Information");
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    ImGui::End();

    // �f�o�b�O�֐��̎��s
    for (auto& f : m_debugfunction)
    {
        f();
    }

    // �t���[���̃����_�����O������
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());	// メインビューポート

    // マルチビューポート：実行画面外へ出たサブウィンドウを描画する
    ImGuiIO& renderIO = ImGui::GetIO();
    if (renderIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        // RenderPlatformWindowsDefault がメインの描画対象を付け替えるため退避→復帰
        ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
        ID3D11RenderTargetView* rtv = nullptr;
        ID3D11DepthStencilView* dsv = nullptr;
        ctx->OMGetRenderTargets(1, &rtv, &dsv);

        // 実行画面外へ出た全サブウィンドウを常に最前面(TopMost)にする
        // （ImGui::Render後・UpdatePlatformWindows前に立てると win32 backend が HWND_TOPMOST を適用）
        ImGuiViewport* mainVp = ImGui::GetMainViewport();
        for (ImGuiViewport* vp : ImGui::GetPlatformIO().Viewports)
        {
            if (vp != mainVp) vp->Flags |= ImGuiViewportFlags_TopMost;
        }

        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();

        ctx->OMSetRenderTargets(1, &rtv, dsv);	// 第3引数は単一ポインタ
        if (rtv) rtv->Release();
        if (dsv) dsv->Release();
    }
}
