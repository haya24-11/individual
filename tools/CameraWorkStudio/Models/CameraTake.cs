using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Text.Json.Serialization;

namespace CameraWorkStudio.Models;

/// <summary>
/// カメラワーク1本（テイク）。キャラクターごとのフォルダに1ファイルとして保存する。
/// v1 = 位置/注視点/up のみ、v2 = ロール/画角/速度を追加。
/// </summary>
public sealed class CameraTake : INotifyPropertyChanged
{
    public const int CurrentVersion = 2;

    private string _name = "新規テイク";
    private double _duration;

    [JsonPropertyName("version")]
    public int Version { get; set; } = CurrentVersion;

    [JsonPropertyName("id")]
    public string Id { get; set; } = Guid.NewGuid().ToString("N");

    /// <summary>所属キャラクター（＝保存先フォルダ名）。</summary>
    [JsonPropertyName("character")]
    public string Character { get; set; } = "";

    /// <summary>表示名（日本語可）。</summary>
    [JsonPropertyName("name")]
    public string Name
    {
        get => _name;
        set => Set(ref _name, value);
    }

    [JsonPropertyName("tags")]
    public List<string> Tags { get; set; } = new();

    [JsonPropertyName("createdUtc")]
    public DateTime CreatedUtc { get; set; } = DateTime.UtcNow;

    /// <summary>長さ（秒）。保存時に最終キーの t から更新される。</summary>
    [JsonPropertyName("duration")]
    public double Duration
    {
        get => _duration;
        set { if (Set(ref _duration, value)) OnPropertyChanged(nameof(Summary)); }
    }

    /// <summary>キー列（時刻の昇順で保つ）。</summary>
    [JsonPropertyName("keys")]
    public ObservableCollection<CameraKey> Keys { get; set; } = new();

    /// <summary>読み込み元のファイルパス（JSON には保存しない）。</summary>
    [JsonIgnore]
    public string? FilePath { get; set; }

    [JsonIgnore]
    public int KeyCount => Keys.Count;

    /// <summary>一覧表示用のサマリ。</summary>
    [JsonIgnore]
    public string Summary => $"{Keys.Count} キー / {Duration:F2} 秒";

    /// <summary>最終キーの時刻から長さを再計算する。</summary>
    public void RecalculateDuration()
    {
        Duration = Keys.Count > 0 ? Keys[^1].T : 0.0;
        OnPropertyChanged(nameof(KeyCount));
        OnPropertyChanged(nameof(Summary));
    }

    /// <summary>キーを時刻の昇順に並べ直す。</summary>
    public void SortKeys()
    {
        var sorted = Keys.OrderBy(k => k.T).ToList();

        Keys.Clear();
        foreach (var k in sorted) Keys.Add(k);
    }

    public CameraTake Clone()
    {
        var copy = new CameraTake
        {
            Version = CurrentVersion,
            Id = Guid.NewGuid().ToString("N"),   // 複製は別IDにする
            Character = Character,
            Name = Name,
            Tags = new List<string>(Tags),
            CreatedUtc = DateTime.UtcNow,
            Duration = Duration,
            FilePath = null
        };

        foreach (var k in Keys) copy.Keys.Add(k.Clone());

        return copy;
    }

    // --- 変更通知 ---

    public event PropertyChangedEventHandler? PropertyChanged;

    private void OnPropertyChanged([CallerMemberName] string? name = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    private bool Set<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        OnPropertyChanged(name);
        return true;
    }
}
