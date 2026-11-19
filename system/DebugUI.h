#pragma once

#include <d3d11.h>
#include <vector>
#include <functional>
#include "../Application.h"

#include "imGui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

class DebugUI {
    // 登録された表示関数。owner はそれを登録したシーン（nullptr なら常に描く）
    struct Entry
    {
        const void*               owner = nullptr;
        std::function<void(void)> func;
    };
    static std::vector<Entry> m_debugfunction;
    static const void*        m_activeOwner;	// 今のシーン。これと違う owner の関数は描かない

public:

    static void Init(ID3D11Device* device, ID3D11DeviceContext* context);

    // デバッグ表示関数の登録。owner にシーンの this を渡すと、そのシーンが今のシーンのときだけ描く
    static void RedistDebugFunction(std::function<void(void)> f, const void* owner = nullptr);

    // 今のシーンを知らせる（SceneManager がシーンを切り替えたときに呼ぶ）
    static void SetActiveOwner(const void* owner) { m_activeOwner = owner; }

    static void Render();

    static void DisposeUI();
};

