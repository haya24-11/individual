using CameraWorkStudio.Models;

namespace CameraWorkStudio.Services;

/// <summary>
/// テイクに対する編集ロジック（UIに依存しない純粋な計算）。
/// </summary>
public static class TakeEditor
{
    /// <summary>時刻 time における姿勢を線形補間で求める。</summary>
    public static CameraKey Evaluate(CameraTake take, double time)
    {
        if (take.Keys.Count == 0) return new CameraKey();
        if (take.Keys.Count == 1) return take.Keys[0].Clone();

        // 範囲外は端でクランプ
        if (time <= take.Keys[0].T) return take.Keys[0].Clone();
        if (time >= take.Keys[^1].T) return take.Keys[^1].Clone();

        // time を挟む区間を探す
        int j = 0;
        for (int i = 0; i < take.Keys.Count - 1; i++)
        {
            if (time >= take.Keys[i].T && time <= take.Keys[i + 1].T) { j = i; break; }
        }

        var a = take.Keys[j];
        var b = take.Keys[j + 1];

        double span = b.T - a.T;
        double f = span > 1e-9 ? (time - a.T) / span : 0.0;

        var up = Vec3.Lerp(a.Up, b.Up, f);
        if (up.Length > 1e-9) up = Normalize(up);

        return new CameraKey(time, Vec3.Lerp(a.Pos, b.Pos, f), Vec3.Lerp(a.Look, b.Look, f), up)
        {
            Roll = a.Roll + (b.Roll - a.Roll) * f,
            Fov = a.Fov + (b.Fov - a.Fov) * f,
            Speed = a.Speed + (b.Speed - a.Speed) * f
        };
    }

    /// <summary>
    /// 密なキー列を指定数まで間引く。始点・終点は必ず残し、間は等間隔で抜き出す。
    /// </summary>
    public static void Decimate(CameraTake take, int maxKeys)
    {
        if (maxKeys < 2 || take.Keys.Count <= maxKeys) return;

        var src = take.Keys.ToList();
        var picked = new List<CameraKey>(maxKeys);

        for (int i = 0; i < maxKeys; i++)
        {
            // 0 〜 (件数-1) を等間隔にサンプリング（始点と終点を含む）
            double u = (double)i / (maxKeys - 1);
            int index = (int)Math.Round(u * (src.Count - 1));
            index = Math.Clamp(index, 0, src.Count - 1);

            picked.Add(src[index].Clone());
        }

        take.Keys.Clear();
        foreach (var k in picked) take.Keys.Add(k);

        take.RecalculateDuration();
    }

    /// <summary>
    /// 各キーの Speed から時刻を再計算する。
    /// 速度が大きい区間ほど短い時間で通過し、テイク全体の長さは totalDuration に保つ。
    /// </summary>
    public static void RecalculateTimesFromSpeed(CameraTake take, double totalDuration)
    {
        int n = take.Keys.Count;
        if (n < 2 || totalDuration <= 0) return;

        // 区間ごとの「重み」= 1 / 平均速度（速いほど時間が短くなる）
        var weights = new double[n - 1];
        double sum = 0.0;

        for (int i = 0; i < n - 1; i++)
        {
            double avgSpeed = (take.Keys[i].Speed + take.Keys[i + 1].Speed) * 0.5;
            if (avgSpeed < 0.01) avgSpeed = 0.01;          // 0除算とフリーズ防止

            weights[i] = 1.0 / avgSpeed;
            sum += weights[i];
        }

        if (sum < 1e-9) return;

        // 重みの比率で totalDuration を配分する
        double t = 0.0;
        take.Keys[0].T = 0.0;

        for (int i = 0; i < n - 1; i++)
        {
            t += totalDuration * (weights[i] / sum);
            take.Keys[i + 1].T = t;
        }

        take.RecalculateDuration();
    }

    /// <summary>
    /// ロール角を Up ベクトルに焼き込む（前方軸まわりに回す）。
    /// C++ 側 SplineCamera::Update と同じ計算。ゲームは Up しか見ないのでこれで傾きが再現される。
    /// </summary>
    public static Vec3 BakeRollIntoUp(CameraKey key)
    {
        var fwd = new Vec3(key.Look.X - key.Pos.X, key.Look.Y - key.Pos.Y, key.Look.Z - key.Pos.Z);

        if (fwd.Length < 1e-9 || Math.Abs(key.Roll) < 1e-9) return key.Up;

        fwd = Normalize(fwd);
        double rad = key.Roll * Math.PI / 180.0;

        return RotateAroundAxis(key.Up, fwd, rad);
    }

    // ---------------- ベクトル補助 ----------------

    public static Vec3 Normalize(Vec3 v)
    {
        double len = v.Length;
        return len < 1e-9 ? v : new Vec3(v.X / len, v.Y / len, v.Z / len);
    }

    /// <summary>ロドリゲスの回転公式で、軸まわりに角度 rad だけ回す。</summary>
    public static Vec3 RotateAroundAxis(Vec3 v, Vec3 axis, double rad)
    {
        double c = Math.Cos(rad);
        double s = Math.Sin(rad);

        // v*cos + (axis × v)*sin + axis*(axis・v)*(1-cos)
        var cross = new Vec3(
            axis.Y * v.Z - axis.Z * v.Y,
            axis.Z * v.X - axis.X * v.Z,
            axis.X * v.Y - axis.Y * v.X);

        double dot = axis.X * v.X + axis.Y * v.Y + axis.Z * v.Z;

        return new Vec3(
            v.X * c + cross.X * s + axis.X * dot * (1 - c),
            v.Y * c + cross.Y * s + axis.Y * dot * (1 - c),
            v.Z * c + cross.Z * s + axis.Z * dot * (1 - c));
    }
}
