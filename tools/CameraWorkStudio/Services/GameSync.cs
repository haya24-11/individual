using System.IO;
using System.IO.MemoryMappedFiles;
using System.Text;
using System.Windows.Threading;
using CameraWorkStudio.Models;

namespace CameraWorkStudio.Services;

/// <summary>ゲームから送られてくる1フレーム分の状態。</summary>
public sealed class GameSyncState
{
    public uint Frame;
    public uint Flags;

    public Vec3 CamPos, CamLook, CamUp;
    public double Fov = 45.0;

    public Vec3 P1Pos; public double P1RotY; public double P1Scale = 1.0; public string P1Mesh = "";
    public Vec3 P2Pos; public double P2RotY; public double P2Scale = 1.0; public string P2Mesh = "";

    public bool IsRecording => (Flags & 1u) != 0;
    public bool IsPlaying => (Flags & 2u) != 0;
}

/// <summary>
/// ゲームが書き込む共有メモリ（メモリマップドファイル）を読み取り、状態を配信する。
/// ゲーム側は毎フレーム書くだけなので、こちらの接続状況に関係なく動く。
/// frame カウンタが一定時間変化しなければ「未接続」と判断する。
/// </summary>
public sealed class GameSync : IDisposable
{
    public const string MapName = @"Local\GM31_CameraSync";
    private const uint ExpectedMagic = 0x314D4347;   // "GCM1"
    private const int MapSize = 512;

    // 構造体のオフセット（C++ 側と一致させること）
    private const int OffMagic = 0;
    private const int OffVersion = 4;
    private const int OffFrame = 8;
    private const int OffFlags = 12;
    private const int OffCamPos = 16;
    private const int OffCamLook = 28;
    private const int OffCamUp = 40;
    private const int OffFov = 52;
    private const int OffP1Pos = 56;
    private const int OffP1RotY = 68;
    private const int OffP1Scale = 72;
    private const int OffP2Pos = 76;
    private const int OffP2RotY = 88;
    private const int OffP2Scale = 92;
    private const int OffP1Mesh = 96;
    private const int OffP2Mesh = 160;
    private const int MeshIdBytes = 64;

    /// <summary>この時間だけ frame が変化しなければ切断とみなす。</summary>
    private static readonly TimeSpan Timeout = TimeSpan.FromSeconds(1);

    private MemoryMappedFile? _mmf;
    private MemoryMappedViewAccessor? _view;
    private readonly DispatcherTimer _timer;

    private uint _lastFrame;
    private DateTime _lastChange = DateTime.MinValue;
    private DateTime _lastOpenAttempt = DateTime.MinValue;

    public GameSyncState State { get; } = new();

    private bool _isConnected;
    public bool IsConnected
    {
        get => _isConnected;
        private set
        {
            if (_isConnected == value) return;
            _isConnected = value;
            ConnectionChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    /// <summary>新しいフレームを受け取ったとき。</summary>
    public event EventHandler? Updated;

    /// <summary>接続状態が変わったとき。</summary>
    public event EventHandler? ConnectionChanged;

    public GameSync()
    {
        _timer = new DispatcherTimer(DispatcherPriority.Render)
        {
            Interval = TimeSpan.FromMilliseconds(16)   // 約60fpsで見に行く
        };
        _timer.Tick += (_, _) => Poll();
    }

    public void Start() => _timer.Start();
    public void Stop() => _timer.Stop();

    private void Poll()
    {
        if (_view is null)
        {
            // ゲームが起動していない間は、負荷を避けるため1秒おきにだけ試す
            if (DateTime.UtcNow - _lastOpenAttempt < TimeSpan.FromSeconds(1)) return;
            _lastOpenAttempt = DateTime.UtcNow;

            if (!TryOpen()) { IsConnected = false; return; }
        }

        try
        {
            ReadState();
        }
        catch (Exception)
        {
            // ゲームが終了して共有メモリが消えた場合など。次の周期で開き直す。
            CloseMap();
            IsConnected = false;
            return;
        }

        IsConnected = DateTime.UtcNow - _lastChange < Timeout;
    }

    private bool TryOpen()
    {
        try
        {
            _mmf = MemoryMappedFile.OpenExisting(MapName, MemoryMappedFileRights.Read);
            _view = _mmf.CreateViewAccessor(0, MapSize, MemoryMappedFileAccess.Read);

            // マジックが違うなら別物なので使わない
            if (_view.ReadUInt32(OffMagic) != ExpectedMagic)
            {
                CloseMap();
                return false;
            }

            _lastChange = DateTime.UtcNow;
            return true;
        }
        catch (FileNotFoundException)
        {
            return false;   // ゲーム未起動：正常な状態なので何も言わない
        }
        catch (Exception)
        {
            CloseMap();
            return false;
        }
    }

    private void ReadState()
    {
        var v = _view!;

        uint frame = v.ReadUInt32(OffFrame);
        if (frame == _lastFrame) return;    // 進んでいなければ読み直さない

        _lastFrame = frame;
        _lastChange = DateTime.UtcNow;

        State.Frame = frame;
        State.Flags = v.ReadUInt32(OffFlags);

        State.CamPos = ReadVec(v, OffCamPos);
        State.CamLook = ReadVec(v, OffCamLook);
        State.CamUp = ReadVec(v, OffCamUp);
        State.Fov = v.ReadSingle(OffFov);

        State.P1Pos = ReadVec(v, OffP1Pos);
        State.P1RotY = v.ReadSingle(OffP1RotY);
        State.P1Scale = v.ReadSingle(OffP1Scale);

        State.P2Pos = ReadVec(v, OffP2Pos);
        State.P2RotY = v.ReadSingle(OffP2RotY);
        State.P2Scale = v.ReadSingle(OffP2Scale);

        State.P1Mesh = ReadUtf8(v, OffP1Mesh, MeshIdBytes);
        State.P2Mesh = ReadUtf8(v, OffP2Mesh, MeshIdBytes);

        Updated?.Invoke(this, EventArgs.Empty);
    }

    private static Vec3 ReadVec(MemoryMappedViewAccessor v, int offset)
        => new(v.ReadSingle(offset), v.ReadSingle(offset + 4), v.ReadSingle(offset + 8));

    /// <summary>NUL 終端の UTF-8 文字列を読む（メッシュIDに日本語が入り得るため）。</summary>
    private static string ReadUtf8(MemoryMappedViewAccessor v, int offset, int maxBytes)
    {
        var buf = new byte[maxBytes];
        v.ReadArray(offset, buf, 0, maxBytes);

        int len = Array.IndexOf(buf, (byte)0);
        if (len < 0) len = maxBytes;

        return len == 0 ? "" : Encoding.UTF8.GetString(buf, 0, len);
    }

    private void CloseMap()
    {
        _view?.Dispose();
        _view = null;
        _mmf?.Dispose();
        _mmf = null;
        _lastFrame = 0;
    }

    public void Dispose()
    {
        _timer.Stop();
        CloseMap();
    }
}
