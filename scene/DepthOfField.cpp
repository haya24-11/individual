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