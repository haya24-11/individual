#include "DepthOfField.h"
#include "../system/renderer.h"
#include "../system/dx11helper.h"
#include "../system/imgui/imgui.h"


namespace
{
	constexpr const char* kShaderFile = "shader/dof.hlsl";

	//Camera::Draw()と同じクリップ面。深度バッファの値を距離に戻すのに使う
	constexpr float kNearZ = 1.0f;
	constexpr float kFarZ = 10000.0f;

	//Renderer::Begin()と同じ背景色
	constexpr float kClearColor[4] = { 0.10f,0.18f,0.55f,1.0f };

}

bool DepthOfField::createTarget(Target& t, UINT w, UINT h)
{
	ID3D11Device* dev = Renderer::GetDevice();

	D3D11_TEXTURE2D_DESC td{};
	td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(dev->CreateTexture2D(&td, nullptr, t.tex.GetAddressOf())))return false;
	if (FAILED(dev->CreateRenderTargetView(t.tex.Get(), nullptr, t.rtv.GetAddressOf()))) return false;
	if (FAILED(dev->CreateShaderResourceView(t.tex.Get(), nullptr, t.srv.GetAddressOf()))) return false;
	return true;

}

bool DepthOfField::Init(UINT width, UINT height)
{
	m_ready = false;
	if (width < 2 || height < 2)return false;
	
	m_w = width;	m_h = height;
	m_hw = width / 2;   m_hh = height / 2;

	ID3D11Device* dev = Renderer::GetDevice();

	//---描画先---
	if (!createTarget(m_scene, m_w, m_h))  return false;
	if (!createTarget(m_blurA, m_hw, m_hh)) return false;
	if (!createTarget(m_blurB, m_hw, m_hh)) return false;

	//深度:書き込みはD32_FLOAT,読み出しはR32_FLOATとして同じテクスチャを使う
	D3D11_TEXTURE2D_DESC dd{};
	dd.Width = m_w; dd.Height = m_h; dd.MipLevels = 1; dd.ArraySize = 1;
	dd.Format = DXGI_FORMAT_R32_TYPELESS; dd.SampleDesc.Count = 1;
	dd.Usage = D3D11_USAGE_DEFAULT;
	dd.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	if (FAILED(dev->CreateTexture2D(&dd, nullptr, m_depthTex.GetAddressOf()))) return false;
	
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
	dsvd.Format = DXGI_FORMAT_D32_FLOAT;
	dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	if (FAILED(dev->CreateDepthStencilView(m_depthTex.Get(), &dsvd, m_depthDSV.GetAddressOf()))) return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
	srvd.Format = DXGI_FORMAT_R32_FLOAT;
	srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvd.Texture2D.MipLevels = 1;
	if (FAILED(dev->CreateShaderResourceView(m_depthTex.Get(), &srvd, m_depthSRV.GetAddressOf()))) return false;


	// --- シェーダー（1つのファイルに4つの入口）---
	ComPtr<ID3DBlob> blob;
	if (FAILED(CompileShaderFromFile(kShaderFile, "VS_Fullscreen", "vs_5_0", blob.GetAddressOf()))) return false;
	if (FAILED(dev->CreateVertexShader
	(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_vsFull.GetAddressOf()))) return false;


	struct { const char* entry; ComPtr<ID3D11PixelShader>* out; } ps[] = {
		{"PS_BlurH",	&m_psBlurH},
		{"PS_BlurV",	&m_psBlurV},
		{"PS_Composite",&m_psComposite,	},

	};
	for (auto& p : ps)
	{
		blob.Reset();
		if (FAILED(CompileShaderFromFile(kShaderFile, p.entry, "ps_5_0", blob.GetAddressOf()))) return false;
		if (FAILED(dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(),
			nullptr, p.out->GetAddressOf()))) return false;
	
	}
	// --- 画面の端で反対側の色を拾わないよう、クランプの線形サンプラー ---
	D3D11_SAMPLER_DESC sd{};
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(dev->CreateSamplerState(&sd, m_sampler.GetAddressOf()))) return false;

	if (!CreateConstantBuffer(dev, sizeof(Params), m_cb.GetAddressOf())) return false;

	m_ready = true;
	return true;

}

void DepthOfField::BeginScene()
{
	if (!IsEnabled())return;
	
	ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();

	//描画先（バックバッファ）を退避
	m_prevRTV.Reset(); m_prevDSV.Reset();
	ctx->OMGetRenderTargets(1, m_prevRTV.GetAddressOf(), m_prevDSV.GetAddressOf());
	UINT n = 1;
	ctx->RSGetViewports(&n, &m_prevVP);

	//オフスクリーンへ切り替え
	ID3D11RenderTargetView* rtv = m_scene.rtv.Get();
	ctx->OMSetRenderTargets(1, &rtv, m_depthDSV.Get());
	D3D11_VIEWPORT vp{};
	vp.Width = (float)m_w; vp.Height = (float)m_h;vp.MaxDepth=1.0f;
	ctx->RSSetViewports(1, &vp);

	ctx->ClearRenderTargetView(rtv, kClearColor);
	ctx->ClearDepthStencilView(m_depthDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);


	
}

//画面全体に三角形を１枚描く
void DepthOfField::drawFullscreen(ID3D11PixelShader* ps, ID3D11RenderTargetView* rtv, UINT w, UINT h,
	ID3D11ShaderResourceView* const* srvs, UINT srvCount)
{
	ID3D11DeviceContext* ctx = Renderer::GetDeviceContext(); 

	ctx->OMSetRenderTargets(1, &rtv, nullptr);
	D3D11_VIEWPORT vp{};
	vp.Width = (float)w; vp.Height = (float)h; vp.MaxDepth = 1.0f;
	ctx->RSSetViewports(1, &vp);

	ctx->PSSetShader(ps, nullptr, 0);
	ctx->PSSetShaderResources(0, srvCount, srvs);
	ctx->Draw(3, 0);

	//次のパスでこのテクスチャを描画先にするので、入力から外しおく
	ID3D11ShaderResourceView* nulls[3] = {};
	ctx->PSSetShaderResources(0, 3, nulls);


}

void DepthOfField::EndScene()
{
	if (!IsEnabled()) return;

	ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();


	//パイプラインの状態を退避
	D3D11_PRIMITIVE_TOPOLOGY prevTopo;
	ctx->IAGetPrimitiveTopology(&prevTopo);
	ComPtr<ID3D11InputLayout>	 prevLayout;
	ComPtr<ID3D11VertexShader>	 prevVS;
	ComPtr<ID3D11PixelShader>    prevPS;
	ComPtr<ID3D11GeometryShader> prevGS;
	ctx->IAGetInputLayout(prevLayout.GetAddressOf());
	ctx->VSGetShader(prevVS.GetAddressOf(), nullptr, nullptr);
	ctx->PSGetShader(prevPS.GetAddressOf(), nullptr, nullptr);
	ctx->GSGetShader(prevGS.GetAddressOf(), nullptr, nullptr);

	//定数バッファ
	Params p{};
	p.texelW = 1.0f / (float)m_w;
	p.texelH = 1.0f / (float)m_h;
	p.blurScale = m_blurScale;
	p.focusDist = m_focusDist;
	p.focusRange = m_focusRange;
	p.blurRange = m_blurRange;
	p.nearZ = kNearZ;
	p.farZ = kFarZ;
	p.debugView = m_debugView ? 1 : 0;
	p.maxBlur = m_maxBlur;
	ctx->UpdateSubresource(m_cb.Get(), 0, nullptr, &p, 0, 0);

	//全画面パス共通の設定
	Renderer::SetDepthEnable(false);
	Renderer::DisableCulling(false);
	ctx->IASetInputLayout(nullptr);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->VSSetShader(m_vsFull.Get(), nullptr, 0);
	ctx->GSSetShader(nullptr, nullptr, 0);
	ctx->PSSetConstantBuffers(8, 1, m_cb.GetAddressOf());
	ctx->PSSetSamplers(1, 1, m_sampler.GetAddressOf());

	// 1.横ぼかし（シーン → 半分の解像度）
	{
		ID3D11ShaderResourceView* srv[3] = { m_scene.srv.Get(), nullptr, m_depthSRV.Get() };
		drawFullscreen(m_psBlurH.Get(), m_blurA.rtv.Get(), m_hw, m_hh, srv, 3);
	}
	// 2. 縦ぼかし
	{
		ID3D11ShaderResourceView* srv[3] = { nullptr, m_blurA.srv.Get(), m_depthSRV.Get() };
		drawFullscreen(m_psBlurV.Get(), m_blurB.rtv.Get(), m_hw, m_hh, srv, 3);
	}
	// 3. 合成して元の描画先（バックバッファ）へ
	{
		ID3D11ShaderResourceView* srv[3] = { m_scene.srv.Get(), m_blurB.srv.Get(), m_depthSRV.Get() };
		drawFullscreen(m_psComposite.Get(), m_prevRTV.Get(), m_w, m_h, srv, 3);
	}

	// --- 元に戻す（後段のデバッグビューと ImGui がそのまま描けるように）---
	ctx->OMSetRenderTargets(1, m_prevRTV.GetAddressOf(), m_prevDSV.Get());
	ctx->RSSetViewports(1, &m_prevVP);
	ctx->IASetPrimitiveTopology(prevTopo);
	ctx->IASetInputLayout(prevLayout.Get());
	ctx->VSSetShader(prevVS.Get(), nullptr, 0);
	ctx->PSSetShader(prevPS.Get(), nullptr, 0);
	ctx->GSSetShader(prevGS.Get(), nullptr, 0);
	Renderer::DisableCulling(true);
	Renderer::SetDepthEnable(true);

	m_prevRTV.Reset(); m_prevDSV.Reset();



}

void DepthOfField::DrawUI()
{
	if (!m_ready)
	{
		ImGui::TextColored(ImVec4(1, 0.4f, 0.3f, 1), "DOF の初期化に失敗しました（%s）", kShaderFile);
		return;
	}

	ImGui::Checkbox("被写界深度（DOF）##DOF enable", &m_enable);
	ImGui::SameLine();
	ImGui::Checkbox("ボケ具合を色で表示##DOF debug", &m_debugView);

	ImGui::Checkbox("キャラに自動でピント##DOF auto focus", &m_autoFocus);
	if (m_autoFocus) ImGui::BeginDisabled();
	ImGui::SliderFloat("ピント距離##DOF focus", &m_focusDist, 10.0f, 3000.0f, "%.0f");
	if (m_autoFocus) ImGui::EndDisabled();

	ImGui::SliderFloat("ピントが合う幅##DOF range", &m_focusRange, 0.0f, 1000.0f, "%.0f");
	ImGui::SliderFloat("ボケきるまでの距離##DOF blur range", &m_blurRange, 10.0f, 2000.0f, "%.0f");
	ImGui::SliderFloat("ボケの大きさ##DOF blur scale", &m_blurScale, 0.5f, 6.0f, "%.1f");
	ImGui::SliderFloat("ボケの強さ##DOF max blur", &m_maxBlur, 0.0f, 1.0f, "%.2f");

	if (m_debugView)
		ImGui::TextDisabled("赤＝手前のボケ　青＝奥のボケ　グレー＝ピントが合っている");
}

