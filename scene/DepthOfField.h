#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "../system/commontypes.h"

//被写界深度（DOF）のポストエフェクト


class DepthOfField
{
public:
	
	bool Init(UINT width, UINT height);

	void BeginScene();	// 描画先をオフスクリーンへ切り替えてクリア
	void EndScene();	// 元の描画先へ戻し、ぼかし→合成して画面へ出す

	void DrawUI();		// 調整UI（Begin/End は呼ばない）

	bool  IsEnabled() const { return m_enable && m_ready; }
	bool  IsAutoFocus() const { return m_autoFocus; }
	void  SetFocusDistance(float d) { m_focusDist = d; }	// オートフォーカス用

private:
	template<class T>using ComPtr = Microsoft::WRL::ComPtr<T>;

	//シェーダーへ送る（b8）16バイト境界

	struct Params
	{
		float texelW, texelH;	// 入力テクスチャ1ピクセルの UV 幅
		float blurScale;		// ぼかしのサンプル間隔（ピクセル）
		float focusDist;		// ピント距離（カメラからの距離）
		float focusRange;		// ピントが合って見える前後の幅
		float blurRange;		// そこから最大ボケになるまでの距離
		float nearZ, farZ;		// 深度を距離に戻すためのクリップ面
		int   debugView;		// 1 で CoC を色で表示
		float maxBlur;			// ボケの最大の強さ（0〜1）
		float pad[2];
	};

	struct Target	// 描画先にも入力にもなるテクスチャ
	{
		ComPtr<ID3D11Texture2D>          tex;
		ComPtr<ID3D11RenderTargetView>   rtv;
		ComPtr<ID3D11ShaderResourceView> srv;
	};


	bool createTarget(Target& t, UINT w, UINT h);
	void drawFullscreen(ID3D11PixelShader* ps, ID3D11RenderTargetView* rtv, UINT w, UINT h,
		ID3D11ShaderResourceView* const* srvs, UINT srvCount);


	//調整値
	bool m_enable	 = true;
	bool m_autoFocus = true;
	float m_focusDist = 500.0f;
	float m_focusRange = 120.0f;
	float m_blurRange = 400.0f;
	float m_blurScale = 2.0f;
	float m_maxBlur = 1.0f;
	bool  m_debugView = false;

	// --- リソース ---
	bool m_ready = false;
	UINT m_w = 0, m_h = 0;			// フル解像度
	UINT m_hw = 0, m_hh = 0;		// 半分の解像度（ぼかし用）


	Target m_scene;					// シーンのカラー（フル）
	Target m_blurA, m_blurB;		// 横ぼかし・縦ぼかしの結果（半分）
	ComPtr<ID3D11Texture2D>          m_depthTex;	// シーンの深度（DSV と SRV の両方で使う）
	ComPtr<ID3D11DepthStencilView>   m_depthDSV;
	ComPtr<ID3D11ShaderResourceView> m_depthSRV;

	ComPtr<ID3D11VertexShader> m_vsFull;
	ComPtr<ID3D11PixelShader>  m_psBlurH, m_psBlurV, m_psComposite;
	ComPtr<ID3D11SamplerState> m_sampler;	// 端で折り返さない線形サンプラー（s1）
	ComPtr<ID3D11Buffer>       m_cb;

	// BeginScene で退避した元の描画先（EndScene で戻す）
	ComPtr<ID3D11RenderTargetView> m_prevRTV;
	ComPtr<ID3D11DepthStencilView> m_prevDSV;
	D3D11_VIEWPORT m_prevVP{};
};

