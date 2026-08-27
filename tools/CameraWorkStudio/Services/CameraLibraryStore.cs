using System.IO;
using System.Text.Encodings.Web;
using System.Text.Json;
using CameraWorkStudio.Models;

namespace CameraWorkStudio.Services;

/// <summary>
/// カメラワーク・ライブラリの読み書き。
/// 構成: RootPath / キャラクター名 / テイク名.json
/// キャラクター＝サブフォルダ名なので、フォルダを作るだけでキャラを増やせる。
/// </summary>
public sealed class CameraLibraryStore
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        // 日本語をエスケープさせない（人が読める JSON にする）
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping
    };

    public string RootPath { get; private set; }

    public CameraLibraryStore(string rootPath)
    {
        RootPath = rootPath;
    }

    public void SetRoot(string rootPath) => RootPath = rootPath;

    /// <summary>ライブラリフォルダを作成する（既にあれば何もしない）。</summary>
    public void EnsureCreated()
    {
        Directory.CreateDirectory(RootPath);
    }

    // ---------------- キャラクター ----------------

    /// <summary>キャラクター名（サブフォルダ名）の一覧。</summary>
    public List<string> GetCharacters()
    {
        EnsureCreated();
        return Directory.GetDirectories(RootPath)
                        .Select(Path.GetFileName)
                        .Where(n => !string.IsNullOrEmpty(n))
                        .Select(n => n!)
                        .OrderBy(n => n, StringComparer.OrdinalIgnoreCase)
                        .ToList();
    }

    public string CreateCharacter(string name)
    {
        var safe = Sanitize(name);
        if (string.IsNullOrWhiteSpace(safe))
            throw new ArgumentException("キャラクター名が空です。");

        Directory.CreateDirectory(Path.Combine(RootPath, safe));
        return safe;
    }

    public void DeleteCharacter(string name)
    {
        var dir = Path.Combine(RootPath, name);
        if (Directory.Exists(dir)) Directory.Delete(dir, recursive: true);
    }

    public string GetCharacterDirectory(string character)
        => Path.Combine(RootPath, character);

    // ---------------- テイク ----------------

    /// <summary>指定キャラクターの全テイクを読み込む。壊れたファイルは読み飛ばす。</summary>
    public List<CameraTake> LoadTakes(string character)
    {
        var dir = GetCharacterDirectory(character);
        var list = new List<CameraTake>();
        if (!Directory.Exists(dir)) return list;

        foreach (var file in Directory.GetFiles(dir, "*.json").OrderBy(f => f))
        {
            try
            {
                var json = File.ReadAllText(file);
                var take = JsonSerializer.Deserialize<CameraTake>(json, JsonOptions);
                if (take is null) continue;

                take.FilePath = file;
                take.Character = character;
                if (string.IsNullOrWhiteSpace(take.Name))
                    take.Name = Path.GetFileNameWithoutExtension(file);

                list.Add(take);
            }
            catch (Exception ex)
            {
                // 1件壊れていても他は読めるようにする
                System.Diagnostics.Debug.WriteLine($"読み込み失敗: {file} / {ex.Message}");
            }
        }
        return list;
    }

    /// <summary>テイクを保存する。名前が変わっていれば古いファイルを削除して置き換える。</summary>
    public void Save(CameraTake take)
    {
        if (string.IsNullOrWhiteSpace(take.Character))
            throw new InvalidOperationException("テイクにキャラクターが設定されていません。");

        var dir = GetCharacterDirectory(take.Character);
        Directory.CreateDirectory(dir);

        take.RecalculateDuration();

        var target = MakeUniquePath(dir, take.Name, take.FilePath);

        var json = JsonSerializer.Serialize(take, JsonOptions);
        File.WriteAllText(target, json);

        // リネームで保存先が変わった場合は旧ファイルを削除
        if (take.FilePath is not null &&
            !string.Equals(take.FilePath, target, StringComparison.OrdinalIgnoreCase) &&
            File.Exists(take.FilePath))
        {
            File.Delete(take.FilePath);
        }

        take.FilePath = target;
    }

    public void Delete(CameraTake take)
    {
        if (take.FilePath is not null && File.Exists(take.FilePath))
            File.Delete(take.FilePath);
        take.FilePath = null;
    }

    /// <summary>複製を作って保存する（別ID・別ファイル）。</summary>
    public CameraTake Duplicate(CameraTake take)
    {
        var copy = take.Clone();
        copy.Name = take.Name + " のコピー";
        Save(copy);
        return copy;
    }

    // ---------------- 内部 ----------------

    /// <summary>ファイル名に使えない文字を除去する（日本語はそのまま使える）。</summary>
    public static string Sanitize(string name)
    {
        var invalid = Path.GetInvalidFileNameChars();
        var cleaned = new string(name.Where(c => !invalid.Contains(c)).ToArray());
        return cleaned.Trim();
    }

    /// <summary>
    /// テイク名からファイルパスを決める。同名が既にあれば連番を付ける。
    /// currentPath（自分自身）とは衝突扱いにしない。
    /// </summary>
    private static string MakeUniquePath(string dir, string name, string? currentPath)
    {
        var baseName = Sanitize(name);
        if (string.IsNullOrWhiteSpace(baseName)) baseName = "take";

        var candidate = Path.Combine(dir, baseName + ".json");
        int n = 2;
        while (File.Exists(candidate) &&
               !string.Equals(candidate, currentPath, StringComparison.OrdinalIgnoreCase))
        {
            candidate = Path.Combine(dir, $"{baseName}_{n}.json");
            n++;
        }
        return candidate;
    }
}
