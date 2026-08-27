using System.Text.Json;
using System.Text.Json.Serialization;

namespace CameraWorkStudio.Models;

/// <summary>
/// 3次元ベクトル。C++ 側（DirectX・左手座標系）の生値をそのまま保持する。
/// JSON では [x, y, z] の配列として読み書きする（将来 C++ 側で手書きパースしやすいため）。
/// </summary>
[JsonConverter(typeof(Vec3JsonConverter))]
public struct Vec3
{
    public double X { get; set; }
    public double Y { get; set; }
    public double Z { get; set; }

    public Vec3(double x, double y, double z)
    {
        X = x; Y = y; Z = z;
    }

    public static Vec3 Zero => new(0, 0, 0);
    public static Vec3 Up => new(0, 1, 0);

    public static Vec3 Lerp(Vec3 a, Vec3 b, double t)
        => new(a.X + (b.X - a.X) * t,
               a.Y + (b.Y - a.Y) * t,
               a.Z + (b.Z - a.Z) * t);

    public double Length => Math.Sqrt(X * X + Y * Y + Z * Z);

    public override string ToString() => $"({X:F1}, {Y:F1}, {Z:F1})";
}

/// <summary>Vec3 を JSON 配列 [x, y, z] として入出力するコンバータ。</summary>
public sealed class Vec3JsonConverter : JsonConverter<Vec3>
{
    public override Vec3 Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
    {
        if (reader.TokenType != JsonTokenType.StartArray)
            throw new JsonException("Vec3 は [x, y, z] の配列である必要があります。");

        Span<double> v = stackalloc double[3];
        for (int i = 0; i < 3; i++)
        {
            if (!reader.Read() || reader.TokenType != JsonTokenType.Number)
                throw new JsonException("Vec3 の要素が数値ではありません。");
            v[i] = reader.GetDouble();
        }

        if (!reader.Read() || reader.TokenType != JsonTokenType.EndArray)
            throw new JsonException("Vec3 の配列は要素3つである必要があります。");

        return new Vec3(v[0], v[1], v[2]);
    }

    public override void Write(Utf8JsonWriter writer, Vec3 value, JsonSerializerOptions options)
    {
        // WriteStartArray を使うとインデント指定時に1要素ずつ改行されてしまうため、
        // 生の JSON として [x, y, z] を1行で書き出す（読みやすさとファイルサイズのため）。
        var raw = string.Create(System.Globalization.CultureInfo.InvariantCulture,
            $"[{value.X:R}, {value.Y:R}, {value.Z:R}]");

        writer.WriteRawValue(raw, skipInputValidation: true);
    }
}
