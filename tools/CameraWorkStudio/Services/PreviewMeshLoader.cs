using System.IO;
using System.Windows.Media;
using System.Windows.Media.Media3D;

namespace CameraWorkStudio.Services;

/// <summary>
/// ゲームが書き出したプレビュー用メッシュ（.pvm）を読み込む。
///
/// 形式:
///   char[4]  "PVM1"
///   uint32   vertexCount
///   uint32   indexCount
///   float[3] * vertexCount   位置
///   float[3] * vertexCount   法線
///   uint32   * indexCount    インデックス
///
/// DirectX（左手系）→ WPF（右手系）へ Z を反転して取り込む。
/// 反転すると三角形の巻き順が逆になるため、インデックスの順序も入れ替える。
/// </summary>
public static class PreviewMeshLoader
{
    private static readonly Dictionary<string, MeshGeometry3D> Cache = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>モデル置き場（CameraLibrary/_models）。</summary>
    public static string ModelDirectory(string libraryRoot) => Path.Combine(libraryRoot, "_models");

    /// <summary>メッシュIDから .pvm を読む。無ければ null。</summary>
    public static MeshGeometry3D? Load(string libraryRoot, string meshId)
    {
        if (string.IsNullOrWhiteSpace(meshId)) return null;

        var path = Path.Combine(ModelDirectory(libraryRoot), meshId + ".pvm");

        if (Cache.TryGetValue(path, out var cached)) return cached;
        if (!File.Exists(path)) return null;

        try
        {
            var mesh = ReadFile(path);
            mesh.Freeze();                 // 描画スレッドで使い回すので凍結
            Cache[path] = mesh;
            return mesh;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"pvm 読み込み失敗: {path} / {ex.Message}");
            return null;
        }
    }

    /// <summary>書き出し直した時などにキャッシュを捨てる。</summary>
    public static void ClearCache() => Cache.Clear();

    private static MeshGeometry3D ReadFile(string path)
    {
        using var fs = File.OpenRead(path);
        using var r = new BinaryReader(fs);

        var magic = new string(r.ReadChars(4));
        if (magic != "PVM1")
            throw new InvalidDataException($"想定外のヘッダです: {magic}");

        int vcount = (int)r.ReadUInt32();
        int icount = (int)r.ReadUInt32();

        if (vcount <= 0 || icount <= 0)
            throw new InvalidDataException("頂点数またはインデックス数が不正です。");

        var positions = new Point3DCollection(vcount);
        var normals = new Vector3DCollection(vcount);
        var indices = new Int32Collection(icount);

        // 位置（Z反転）
        for (int i = 0; i < vcount; i++)
        {
            float x = r.ReadSingle(), y = r.ReadSingle(), z = r.ReadSingle();
            positions.Add(new Point3D(x, y, -z));
        }

        // 法線（Z反転）
        for (int i = 0; i < vcount; i++)
        {
            float x = r.ReadSingle(), y = r.ReadSingle(), z = r.ReadSingle();
            normals.Add(new Vector3D(x, y, -z));
        }

        // インデックス（Z反転で巻き順が裏返るので 2番目と3番目を入れ替える）
        var raw = new int[icount];
        for (int i = 0; i < icount; i++) raw[i] = (int)r.ReadUInt32();

        for (int i = 0; i + 2 < icount; i += 3)
        {
            indices.Add(raw[i]);
            indices.Add(raw[i + 2]);
            indices.Add(raw[i + 1]);
        }

        return new MeshGeometry3D
        {
            Positions = positions,
            Normals = normals,
            TriangleIndices = indices
        };
    }
}
