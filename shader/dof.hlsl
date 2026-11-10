//被写界深度（dof)

//VS_Fullscreenで画面を覆い、横ぼかし→縦ぼかし→合成　の順に使う

cbuffer DofParams : register(b8)
{
    float2 TexelSize; // フル解像度の1ピクセルの UV 幅
    float BlurScale; // ぼかしのサンプル間隔
    float FocusDist; // ピント距離（カメラからの距離）
    float FocusRange; // ピントが合って見える前後の幅
    float BlurRange; // そこから最大ボケになるまでの距離
    float NearZ;
    float FarZ;
    int DebugView; // 1 で CoC を色で表示
    float MaxBlur; // ボケの最大の強さ
    float2 Pad;
};

Texture2D g_Scene : register(t0);
Texture2D g_Blur : register(t1); // ぼかした画像
Texture2D<float> g_Depth : register(t2); // シーンの深度
SamplerState g_Clamp : register(s1);

struct VS_OUT
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
    
};

//頂点バッファを使わず、頂点番号かｒ画面を覆う大きな三角形
//id=→0；UV(0,0)id=1 → UV(2,0)  id=2 → UV(0,2)
VS_OUT VS_Fullscreen(uint id:SV_VertexID)
{
    VS_OUT o;
    o.UV = float2((id << 1) & 2, id & 2);
    o.Pos = float4(o.UV * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
    
}

float LinearDepth(float d)
{
    return NearZ * FarZ / (FarZ - d * (FarZ - NearZ));
}


float DepthAt(float2 uv)
{
    int2 size = int2(round(1.0 / TexelSize));
    int2 p = clamp(int2(uv * size), int2(0, 0), size - 1);
    return LinearDepth(g_Depth.Load(int3(p, 0)));
}

float CocFromDepth(float z)
{
    return saturate((abs(z - FocusDist) - FocusRange) / max(BlurRange, 1e-3)) * MaxBlur;
}

static const float W[5] = { 0.2270270270, 0.1945945946,
    0.1216216216, 0.0540540541, 0.0162162162 };


float4 Blur(Texture2D tex,float2 uv,float2 dir)
{
    float3 sum = tex.Sample(g_Clamp, uv).rgb * W[0];
    float wsum = W[0];
    
    [unroll]
    for (int i = 1; i < 5;++i)
    {
        float2 o = dir * (i * BlurScale);
        float2 uvA = uv + o;
        float2 uvB = uv - o;
        
        //ピントが合っている画素はほとんど混ぜない
        //そのまま混ぜると、ぼかした背景にキャラの色がにじみ出て輪郭に光の和ができる
        
        float wa = W[i] * CocFromDepth(DepthAt(uvA));
        float wb = W[i] * CocFromDepth(DepthAt(uvB));
        
        sum += tex.Sample(g_Clamp, uvA).rgb * wa + tex.Sample(g_Clamp, uvB).rgb * wb;
        wsum += wa + wb;
    }
    return float4(sum / wsum, 1.0);
        
}
    
float4 PS_BlurH(VS_OUT i):SV_Target
{
    return Blur(g_Scene, i.UV, float2(TexelSize.x * 2.0, 0.0));
}

float4 PS_BlurV(VS_OUT i):SV_Target
{
    return Blur(g_Blur, i.UV, float2(0.0,TexelSize.y * 2.0));
    
}

// 3. 合成：CoC に応じて、くっきりした画像とぼかした画像を混ぜる
float4 PS_Composite(VS_OUT i) : SV_Target
{
    float4 sharp = g_Scene.Sample(g_Clamp, i.UV);
    float4 blur = g_Blur.Sample(g_Clamp, i.UV);
    float z = DepthAt(i.UV);
    float coc = CocFromDepth(z);

    if (DebugView)
    {
        // 手前のボケ＝赤、奥のボケ＝青、ピントが合っている所＝グレー
        float gray = dot(sharp.rgb, float3(0.299, 0.587, 0.114)) * 0.6;
        float3 tint = (z < FocusDist) ? float3(1.0, 0.25, 0.1) : float3(0.2, 0.45, 1.0);
        return float4(lerp(gray.xxx, tint, saturate(coc / max(MaxBlur, 1e-3))), 1.0);
    }

    return float4(lerp(sharp.rgb, blur.rgb, coc), 1.0);
}