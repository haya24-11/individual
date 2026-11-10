#include "common.hlsl"

// リムライト（輪郭光）。視線に対して横を向いている面ほど明るく縁取る
cbuffer RimLightBuffer : register(b7)
{
    float4 RimColor;        // 縁の色
    float  RimPower;        // 大きいほど縁が細くなる
    float  RimIntensity;    // 明るさ
    bool   RimEnable;       // 使うかどうか
    float  RimDummy;        // PADDING
};

PS_IN main(in VSONESKIN_IN In)
{
    PS_IN Out;

	// ワンスキン頂点ブレンドの処理
    float4x4 comb = (float4x4) 0;
    for (int i = 0; i < 4; i++)
    {
		// 重みを計算しながら行列生成
        comb += BoneMatrix[In.BoneIndex[i]] * In.BoneWeight[i];
    }

    float4 Pos;

    Pos = mul(In.Position,comb);
    In.Position = Pos;

	//
    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
	
    // 法線も頂点と同じボーン行列で曲げる（曲げないと陰影が T ポーズのままになる）
    float4 worldNormal, normal;
    normal = mul(float4(In.Normal.xyz, 0.0), comb);
    worldNormal = mul(normal, World);
    worldNormal = normalize(worldNormal);

    float light = -(dot(Light.Direction.xyz, worldNormal.xyz))* 0.5 + 0.5;
    light = saturate(light);  // ランバート反射の計算

    Out.Diffuse = In.Diffuse * Material.Diffuse * light * Light.Diffuse;
    Out.Diffuse += In.Diffuse * Material.Ambient * Light.Ambient;
    Out.Diffuse += Material.Emission;

    // リムライト：ビュー空間で「法線」と「頂点からカメラへの向き」を比べる。
    // 正面を向く面は内積が 1（縁取らない）、輪郭に近い面ほど 0 に近づく（明るく縁取る）
    if (RimEnable)
    {
        float3 viewPos    = mul(mul(In.Position, World), View).xyz;
        float3 viewNormal = normalize(mul(float4(worldNormal.xyz, 0.0), View).xyz);
        float3 toEye      = normalize(-viewPos);    // ビュー空間ではカメラが原点

        float rim = 1.0 - saturate(dot(viewNormal, toEye));
        rim = pow(rim, RimPower) * RimIntensity;

        Out.Diffuse.rgb += RimColor.rgb * rim;
    }

    Out.Diffuse.a = In.Diffuse.a * Material.Diffuse.a;

    Out.Position = mul(In.Position, wvp);
    Out.TexCoord = In.TexCoord;

    return Out;    
}
