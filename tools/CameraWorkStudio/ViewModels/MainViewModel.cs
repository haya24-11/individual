using System.Collections.ObjectModel;
using System.IO;
using System.Text.Json;
using System.Windows.Media;
using CameraWorkStudio.Models;
using CameraWorkStudio.Services;

namespace CameraWorkStudio.ViewModels;

/// <summary>
/// メイン画面のビューモデル。
/// 左＝キャラクター一覧／中＝テイク一覧／右＝詳細、をまとめて面倒を見る。
/// </summary>
public sealed class MainViewModel : ObservableObject
{
    private readonly CameraLibraryStore _store;

    public MainViewModel()
    {
        LibraryPath = LoadSavedLibraryPath() ?? ResolveDefaultLibraryPath();
        _store = new CameraLibraryStore(LibraryPath);

        RefreshCommand = new RelayCommand(Refresh);
        NewCharacterCommand = new RelayCommand(NewCharacter, () => !string.IsNullOrWhiteSpace(NewCharacterName));
        NewTakeCommand = new RelayCommand(NewTake, () => SelectedCharacter is not null);
        DuplicateTakeCommand = new RelayCommand(DuplicateTake, () => SelectedTake is not null);
        DeleteTakeCommand = new RelayCommand(DeleteTake, () => SelectedTake is not null);
        SaveTakeCommand = new RelayCommand(SaveTake, () => SelectedTake is not null);
        ImportLegacyCommand = new RelayCommand(ImportLegacy, () => SelectedCharacter is not null);
        ExportLegacyCommand = new RelayCommand(ExportLegacy, () => SelectedTake is not null);
        OpenFolderCommand = new RelayCommand(OpenFolder);
        ApplyLibraryPathCommand = new RelayCommand(ApplyLibraryPath);

        // --- タイムライン ---
        AddKeyCommand = new RelayCommand(AddKey, () => SelectedTake is not null);
        DeleteKeyCommand = new RelayCommand(DeleteKey, () => SelectedKey is not null);
        DuplicateKeyCommand = new RelayCommand(DuplicateKey, () => SelectedKey is not null);
        PlayPauseCommand = new RelayCommand(TogglePlay, () => (SelectedTake?.Keys.Count ?? 0) >= 2);
        StopCommand = new RelayCommand(StopPlay);
        DecimateCommand = new RelayCommand(DecimateKeys, () => (SelectedTake?.Keys.Count ?? 0) > DecimateTarget);
        RecalcTimesCommand = new RelayCommand(RecalcTimes, () => (SelectedTake?.Keys.Count ?? 0) >= 2);

        // --- ゲームとのリアルタイム同期 ---
        Sync = new GameSync();
        Sync.ConnectionChanged += (_, _) =>
        {
            OnPropertyChanged(nameof(IsGameConnected));
            OnPropertyChanged(nameof(ConnectionText));
        };
        Sync.Start();

        InitializeLibrary();
        Refresh();
    }

    /// <summary>ゲームから届く状態（3Dプレビューが参照する）。</summary>
    public GameSync Sync { get; }

    public bool IsGameConnected => Sync.IsConnected;

    public string ConnectionText => Sync.IsConnected ? "ゲーム: 接続中" : "ゲーム: 未接続";

    private bool _followGameCamera;
    /// <summary>3Dプレビューをゲーム視点にする（false なら俯瞰）。</summary>
    public bool FollowGameCamera
    {
        get => _followGameCamera;
        set => SetProperty(ref _followGameCamera, value);
    }

    // ---------------- ライブラリの場所 ----------------

    private string _libraryPath = "";
    /// <summary>ライブラリのルートフォルダ。C++ 側と共有する。</summary>
    public string LibraryPath
    {
        get => _libraryPath;
        set => SetProperty(ref _libraryPath, value);
    }

    // ---------------- 一覧・選択 ----------------

    public ObservableCollection<string> Characters { get; } = new();

    private string? _selectedCharacter;
    public string? SelectedCharacter
    {
        get => _selectedCharacter;
        set
        {
            if (SetProperty(ref _selectedCharacter, value))
                ReloadTakes();
        }
    }

    public ObservableCollection<CameraTake> Takes { get; } = new();

    private CameraTake? _selectedTake;
    public CameraTake? SelectedTake
    {
        get => _selectedTake;
        set
        {
            if (SetProperty(ref _selectedTake, value))
            {
                OnPropertyChanged(nameof(HasSelectedTake));
                OnPropertyChanged(nameof(SelectedTakeTagsText));

                // テイクを変えたら再生位置と選択キーを初期化する
                StopPlay();
                CurrentTime = 0.0;
                SelectedKey = null;
            }
        }
    }

    public bool HasSelectedTake => SelectedTake is not null;

    /// <summary>タグをカンマ区切りで編集するための橋渡し。</summary>
    public string SelectedTakeTagsText
    {
        get => SelectedTake is null ? "" : string.Join(", ", SelectedTake.Tags);
        set
        {
            if (SelectedTake is null) return;
            SelectedTake.Tags = value.Split(',',
                    StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
                .ToList();
            OnPropertyChanged();
        }
    }

    private string _newCharacterName = "";
    public string NewCharacterName
    {
        get => _newCharacterName;
        set => SetProperty(ref _newCharacterName, value);
    }

    private string _status = "";
    /// <summary>画面下部に出す状態メッセージ。</summary>
    public string Status
    {
        get => _status;
        set => SetProperty(ref _status, value);
    }

    // ---------------- コマンド ----------------

    public RelayCommand RefreshCommand { get; }
    public RelayCommand NewCharacterCommand { get; }
    public RelayCommand NewTakeCommand { get; }
    public RelayCommand DuplicateTakeCommand { get; }
    public RelayCommand DeleteTakeCommand { get; }
    public RelayCommand SaveTakeCommand { get; }
    public RelayCommand ImportLegacyCommand { get; }
    public RelayCommand ExportLegacyCommand { get; }
    public RelayCommand OpenFolderCommand { get; }
    public RelayCommand ApplyLibraryPathCommand { get; }

    public RelayCommand AddKeyCommand { get; }
    public RelayCommand DeleteKeyCommand { get; }
    public RelayCommand DuplicateKeyCommand { get; }
    public RelayCommand PlayPauseCommand { get; }
    public RelayCommand StopCommand { get; }
    public RelayCommand DecimateCommand { get; }
    public RelayCommand RecalcTimesCommand { get; }

    // ---------------- タイムライン ----------------

    private CameraKey? _selectedKey;
    /// <summary>タイムラインで選択中のキー（インスペクタの編集対象）。</summary>
    public CameraKey? SelectedKey
    {
        get => _selectedKey;
        set
        {
            if (SetProperty(ref _selectedKey, value))
                OnPropertyChanged(nameof(HasSelectedKey));
        }
    }

    public bool HasSelectedKey => SelectedKey is not null;

    private double _currentTime;
    /// <summary>再生ヘッドの時刻（秒）。</summary>
    public double CurrentTime
    {
        get => _currentTime;
        set
        {
            if (SetProperty(ref _currentTime, value))
                OnPropertyChanged(nameof(CurrentPoseText));
        }
    }

    private bool _isPlaying;
    public bool IsPlaying
    {
        get => _isPlaying;
        private set
        {
            if (SetProperty(ref _isPlaying, value))
                OnPropertyChanged(nameof(PlayButtonText));
        }
    }

    public string PlayButtonText => IsPlaying ? "一時停止" : "再生";

    private bool _loopPlayback = true;
    public bool LoopPlayback
    {
        get => _loopPlayback;
        set => SetProperty(ref _loopPlayback, value);
    }

    private int _decimateTarget = 12;
    /// <summary>間引き後に残すキー数。</summary>
    public int DecimateTarget
    {
        get => _decimateTarget;
        set => SetProperty(ref _decimateTarget, Math.Max(2, value));
    }

    /// <summary>再生ヘッド位置の姿勢（数値プレビュー）。</summary>
    public string CurrentPoseText
    {
        get
        {
            if (SelectedTake is null || SelectedTake.Keys.Count == 0) return "-";

            var k = TakeEditor.Evaluate(SelectedTake, CurrentTime);
            return $"t={CurrentTime:F2}s  位置{k.Pos}  注視点{k.Look}  ロール{k.Roll:F1}°  画角{k.Fov:F0}°";
        }
    }

    private DateTime _lastTick;

    private void TogglePlay()
    {
        if (IsPlaying) { StopTicking(); IsPlaying = false; return; }

        if ((SelectedTake?.Keys.Count ?? 0) < 2) return;

        _lastTick = DateTime.UtcNow;
        CompositionTarget.Rendering += OnRenderTick;
        IsPlaying = true;
    }

    private void StopPlay()
    {
        StopTicking();
        IsPlaying = false;
        CurrentTime = 0.0;
    }

    private void StopTicking() => CompositionTarget.Rendering -= OnRenderTick;

    private void OnRenderTick(object? sender, EventArgs e)
    {
        var now = DateTime.UtcNow;
        double dt = (now - _lastTick).TotalSeconds;
        _lastTick = now;

        if (SelectedTake is null || SelectedTake.Duration <= 0) { StopPlay(); return; }

        double t = CurrentTime + dt;

        if (t >= SelectedTake.Duration)
        {
            if (LoopPlayback) t = 0.0;
            else { CurrentTime = SelectedTake.Duration; StopTicking(); IsPlaying = false; return; }
        }

        CurrentTime = t;
    }

    /// <summary>再生ヘッド位置に、その時点の補間姿勢でキーを挿入する。</summary>
    private void AddKey()
    {
        if (SelectedTake is null) return;

        var key = SelectedTake.Keys.Count > 0
            ? TakeEditor.Evaluate(SelectedTake, CurrentTime)
            : new CameraKey(0, new Vec3(0, 20, -400), Vec3.Zero, Vec3.Up);

        key.T = CurrentTime;

        SelectedTake.Keys.Add(key);
        SelectedTake.SortKeys();
        SelectedTake.RecalculateDuration();

        SelectedKey = key;
        Status = $"キーを追加しました（t={key.T:F2}秒）。";
    }

    private void DeleteKey()
    {
        if (SelectedTake is null || SelectedKey is null) return;

        SelectedTake.Keys.Remove(SelectedKey);
        SelectedKey = null;
        SelectedTake.RecalculateDuration();

        Status = "キーを削除しました。";
    }

    private void DuplicateKey()
    {
        if (SelectedTake is null || SelectedKey is null) return;

        var copy = SelectedKey.Clone();
        copy.T = SelectedKey.T + 0.1;      // 少し後ろにずらして重ならないようにする

        SelectedTake.Keys.Add(copy);
        SelectedTake.SortKeys();
        SelectedTake.RecalculateDuration();

        SelectedKey = copy;
        Status = "キーを複製しました。";
    }

    private void DecimateKeys()
    {
        if (SelectedTake is null) return;

        int before = SelectedTake.Keys.Count;
        TakeEditor.Decimate(SelectedTake, DecimateTarget);
        SelectedKey = null;

        Status = $"キーを間引きました（{before} → {SelectedTake.Keys.Count}）。";
    }

    private void RecalcTimes()
    {
        if (SelectedTake is null || SelectedTake.Keys.Count < 2) return;

        double total = SelectedTake.Duration;
        TakeEditor.RecalculateTimesFromSpeed(SelectedTake, total);

        Status = $"速度から時刻を再計算しました（全体 {total:F2} 秒を維持）。";
    }

    // ---------------- 処理 ----------------

    /// <summary>初回起動時にフォルダとサンプルを用意する。</summary>
    private void InitializeLibrary()
    {
        _store.EnsureCreated();

        if (_store.GetCharacters().Count > 0) return;

        // 空だと何も分からないので、サンプルを1件だけ置く
        var character = _store.CreateCharacter("kiyo");
        var sample = CreateSampleTake(character);
        _store.Save(sample);

        Status = "サンプルを作成しました。";
    }

    /// <summary>原点を見ながら円周上を1周する2秒のテイク（動作確認用）。</summary>
    private static CameraTake CreateSampleTake(string character)
    {
        var take = new CameraTake
        {
            Character = character,
            Name = "サンプル 円周",
            Tags = new List<string> { "sample" }
        };

        const int steps = 60;
        const double duration = 2.0;
        const double radius = 400.0;

        for (int i = 0; i <= steps; i++)
        {
            double u = (double)i / steps;
            double angle = u * Math.PI * 2.0;

            var pos = new Vec3(Math.Sin(angle) * radius, 150.0, Math.Cos(angle) * radius);
            take.Keys.Add(new CameraKey(u * duration, pos, Vec3.Zero, Vec3.Up));
        }

        take.RecalculateDuration();
        return take;
    }

    public void Refresh()
    {
        var keepCharacter = SelectedCharacter;

        Characters.Clear();
        foreach (var c in _store.GetCharacters())
            Characters.Add(c);

        // 直前の選択をできるだけ維持する
        SelectedCharacter = keepCharacter is not null && Characters.Contains(keepCharacter)
            ? keepCharacter
            : Characters.FirstOrDefault();

        Status = $"キャラクター {Characters.Count} 件 / {LibraryPath}";
    }

    private void ReloadTakes()
    {
        Takes.Clear();
        SelectedTake = null;

        if (SelectedCharacter is null) return;

        foreach (var t in _store.LoadTakes(SelectedCharacter))
            Takes.Add(t);

        SelectedTake = Takes.FirstOrDefault();
    }

    private void NewCharacter()
    {
        try
        {
            var created = _store.CreateCharacter(NewCharacterName);
            NewCharacterName = "";
            Refresh();
            SelectedCharacter = created;
            Status = $"キャラクター「{created}」を作成しました。";
        }
        catch (Exception ex)
        {
            Status = $"作成に失敗: {ex.Message}";
        }
    }

    private void NewTake()
    {
        if (SelectedCharacter is null) return;

        var take = new CameraTake
        {
            Character = SelectedCharacter,
            Name = "新規テイク"
        };

        _store.Save(take);
        ReloadTakes();
        SelectedTake = Takes.FirstOrDefault(t => t.Id == take.Id);
        Status = "空のテイクを作成しました。";
    }

    private void DuplicateTake()
    {
        if (SelectedTake is null) return;

        var copy = _store.Duplicate(SelectedTake);
        ReloadTakes();
        SelectedTake = Takes.FirstOrDefault(t => t.Id == copy.Id);
        Status = $"「{copy.Name}」を作成しました。";
    }

    private void DeleteTake()
    {
        if (SelectedTake is null) return;

        var name = SelectedTake.Name;
        _store.Delete(SelectedTake);
        ReloadTakes();
        Status = $"「{name}」を削除しました。";
    }

    private void SaveTake()
    {
        if (SelectedTake is null) return;

        try
        {
            _store.Save(SelectedTake);
            var id = SelectedTake.Id;
            ReloadTakes();
            SelectedTake = Takes.FirstOrDefault(t => t.Id == id);
            Status = "保存しました。";
        }
        catch (Exception ex)
        {
            Status = $"保存に失敗: {ex.Message}";
        }
    }

    private void ImportLegacy()
    {
        if (SelectedCharacter is null) return;

        var dlg = new Microsoft.Win32.OpenFileDialog
        {
            Title = "camera_take.txt を取り込む",
            Filter = "カメラテイク (*.txt)|*.txt|すべてのファイル (*.*)|*.*"
        };
        if (dlg.ShowDialog() != true) return;

        try
        {
            var name = Path.GetFileNameWithoutExtension(dlg.FileName);
            var take = LegacyTakeIO.Import(dlg.FileName, SelectedCharacter, name);
            _store.Save(take);
            ReloadTakes();
            SelectedTake = Takes.FirstOrDefault(t => t.Id == take.Id);
            Status = $"取り込みました（{take.KeyCount} キー / {take.Duration:F2} 秒）。";
        }
        catch (Exception ex)
        {
            Status = $"取り込みに失敗: {ex.Message}";
        }
    }

    private void ExportLegacy()
    {
        if (SelectedTake is null) return;

        var dlg = new Microsoft.Win32.SaveFileDialog
        {
            Title = "camera_take.txt として書き出す",
            FileName = "camera_take.txt",
            Filter = "カメラテイク (*.txt)|*.txt"
        };
        if (dlg.ShowDialog() != true) return;

        try
        {
            LegacyTakeIO.Export(SelectedTake, dlg.FileName);
            Status = $"書き出しました: {dlg.FileName}";
        }
        catch (Exception ex)
        {
            Status = $"書き出しに失敗: {ex.Message}";
        }
    }

    private void OpenFolder()
    {
        try
        {
            _store.EnsureCreated();
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
            {
                FileName = LibraryPath,
                UseShellExecute = true
            });
        }
        catch (Exception ex)
        {
            Status = $"フォルダを開けません: {ex.Message}";
        }
    }

    private void ApplyLibraryPath()
    {
        _store.SetRoot(LibraryPath);
        SaveLibraryPath(LibraryPath);
        InitializeLibrary();
        Refresh();
    }

    // ---------------- 設定の保存 ----------------

    private sealed class Settings
    {
        public string? LibraryPath { get; set; }
    }

    private static string SettingsFile
        => Path.Combine(AppContext.BaseDirectory, "settings.json");

    private static string? LoadSavedLibraryPath()
    {
        try
        {
            if (!File.Exists(SettingsFile)) return null;
            var s = JsonSerializer.Deserialize<Settings>(File.ReadAllText(SettingsFile));
            return string.IsNullOrWhiteSpace(s?.LibraryPath) ? null : s!.LibraryPath;
        }
        catch
        {
            return null;
        }
    }

    private static void SaveLibraryPath(string path)
    {
        try
        {
            var json = JsonSerializer.Serialize(new Settings { LibraryPath = path },
                new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(SettingsFile, json);
        }
        catch
        {
            // 設定が保存できなくても動作は続ける
        }
    }

    /// <summary>
    /// 既定のライブラリ場所を推定する。
    /// exe の位置から親をたどり、C++ プロジェクトの目印（assets フォルダ）を探す。
    /// </summary>
    private static string ResolveDefaultLibraryPath()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);

        for (int i = 0; i < 8 && dir is not null; i++)
        {
            var assets = Path.Combine(dir.FullName, "assets");
            if (Directory.Exists(assets))
                return Path.Combine(dir.FullName, "CameraLibrary");

            dir = dir.Parent;
        }

        // 見つからなければ exe の隣に作る
        return Path.Combine(AppContext.BaseDirectory, "CameraLibrary");
    }
}
