using System.Globalization;
using System.IO;
using CameraWorkStudio.Models;

namespace CameraWorkStudio.Services;

/// <summary>
/// C++ 側 CameraRecorder の camera_take.txt との相互変換。
/// 形式: 1行目にキー数、以降 1行ごとに
///       t px py pz lx ly lz ux uy uz
/// </summary>
public static class LegacyTakeIO
{
    /// <summary>camera_take.txt を読み込んで CameraTake にする。</summary>
    public static CameraTake Import(string path, string character, string name)
    {
        var text = File.ReadAllText(path);

        // 空白・改行をまとめて数値トークンとして扱う
        var separators = new[] { ' ', '\t', '\r', '\n' };
        var tokens = text.Split(separators, StringSplitOptions.RemoveEmptyEntries);

        if (tokens.Length == 0)
            throw new InvalidDataException("ファイルが空です。");

        int index = 0;
        int count = int.Parse(tokens[index++], CultureInfo.InvariantCulture);

        var take = new CameraTake
        {
            Character = character,
            Name = name
        };

        double Next() => double.Parse(tokens[index++], CultureInfo.InvariantCulture);

        for (int i = 0; i < count; i++)
        {
            // 1キーあたり 10 個の数値が必要
            if (index + 10 > tokens.Length) break;

            double t = Next();
            var pos = new Vec3(Next(), Next(), Next());
            var look = new Vec3(Next(), Next(), Next());
            var up = new Vec3(Next(), Next(), Next());

            take.Keys.Add(new CameraKey(t, pos, look, up));
        }

        take.RecalculateDuration();
        return take;
    }

    /// <summary>
    /// CameraTake を camera_take.txt 形式で書き出す（C++ 側の Load と互換）。
    /// ロール角は Up ベクトルへ焼き込むので、ゲーム側を変更せずに傾きが再現される。
    /// </summary>
    public static void Export(CameraTake take, string path)
    {
        using var w = new StreamWriter(path, append: false);

        w.WriteLine(take.Keys.Count.ToString(CultureInfo.InvariantCulture));

        foreach (var k in take.Keys)
        {
            var up = TakeEditor.BakeRollIntoUp(k);

            var line = string.Join(' ',
                F(k.T),
                F(k.Pos.X), F(k.Pos.Y), F(k.Pos.Z),
                F(k.Look.X), F(k.Look.Y), F(k.Look.Z),
                F(up.X), F(up.Y), F(up.Z));

            w.WriteLine(line);
        }
    }

    /// <summary>C++ 側は operator&gt;&gt; で読むので、ロケール非依存の書式にする。</summary>
    private static string F(double v) => v.ToString("R", CultureInfo.InvariantCulture);
}
