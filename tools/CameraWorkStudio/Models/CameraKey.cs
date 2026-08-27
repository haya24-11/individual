using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Text.Json.Serialization;

namespace CameraWorkStudio.Models;

/// <summary>
/// タイムライン上の1キー。位置・注視点に加えて、演出用のロール／画角／速度を持つ。
/// C++ 側の CameraRecorder::CamKey（t/pos/look/up）を包含する形。
/// 編集UIから直接バインドするため変更通知を実装している。
/// </summary>
public sealed class CameraKey : INotifyPropertyChanged
{
    private double _t;
    private Vec3 _pos;
    private Vec3 _look;
    private Vec3 _up = Vec3.Up;
    private double _roll;
    private double _fov = 45.0;
    private double _speed = 1.0;

    /// <summary>テイク先頭からの経過秒。</summary>
    [JsonPropertyName("t")]
    public double T
    {
        get => _t;
        set => Set(ref _t, value);
    }

    /// <summary>カメラ位置。</summary>
    [JsonPropertyName("pos")]
    public Vec3 Pos
    {
        get => _pos;
        set { if (Set(ref _pos, value)) NotifyVectorParts(nameof(PosX), nameof(PosY), nameof(PosZ)); }
    }

    /// <summary>注視点。</summary>
    [JsonPropertyName("look")]
    public Vec3 Look
    {
        get => _look;
        set { if (Set(ref _look, value)) NotifyVectorParts(nameof(LookX), nameof(LookY), nameof(LookZ)); }
    }

    /// <summary>アップベクトル（録画由来の値。ロールは Roll 側で別に持つ）。</summary>
    [JsonPropertyName("up")]
    public Vec3 Up
    {
        get => _up;
        set => Set(ref _up, value);
    }

    /// <summary>ロール角（度）。ダッチアングル。txt 書き出し時に Up へ焼き込む。</summary>
    [JsonPropertyName("roll")]
    public double Roll
    {
        get => _roll;
        set => Set(ref _roll, value);
    }

    /// <summary>画角（度）。※ゲーム側は現在45度固定のため未対応。</summary>
    [JsonPropertyName("fov")]
    public double Fov
    {
        get => _fov;
        set => Set(ref _fov, value);
    }

    /// <summary>速度倍率。時刻の再計算に使う（大きいほどその区間を速く通過）。</summary>
    [JsonPropertyName("speed")]
    public double Speed
    {
        get => _speed;
        set => Set(ref _speed, value);
    }

    // --- 編集UI用（Vec3 は構造体なので成分ごとにバインドできるようにする）---

    [JsonIgnore] public double PosX { get => _pos.X; set { var v = _pos; v.X = value; Pos = v; } }
    [JsonIgnore] public double PosY { get => _pos.Y; set { var v = _pos; v.Y = value; Pos = v; } }
    [JsonIgnore] public double PosZ { get => _pos.Z; set { var v = _pos; v.Z = value; Pos = v; } }

    [JsonIgnore] public double LookX { get => _look.X; set { var v = _look; v.X = value; Look = v; } }
    [JsonIgnore] public double LookY { get => _look.Y; set { var v = _look; v.Y = value; Look = v; } }
    [JsonIgnore] public double LookZ { get => _look.Z; set { var v = _look; v.Z = value; Look = v; } }

    public CameraKey() { }

    public CameraKey(double t, Vec3 pos, Vec3 look, Vec3 up)
    {
        _t = t; _pos = pos; _look = look; _up = up;
    }

    public CameraKey Clone() => new(T, Pos, Look, Up)
    {
        Roll = Roll,
        Fov = Fov,
        Speed = Speed
    };

    // --- 変更通知 ---

    public event PropertyChangedEventHandler? PropertyChanged;

    private bool Set<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        return true;
    }

    private void NotifyVectorParts(params string[] names)
    {
        foreach (var n in names)
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(n));
    }
}
