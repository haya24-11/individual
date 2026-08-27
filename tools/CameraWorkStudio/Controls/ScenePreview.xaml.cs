using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using CameraWorkStudio.Models;
using CameraWorkStudio.Services;

namespace CameraWorkStudio.Controls;

/// <summary>
/// シーンの3Dプレビュー。ゲームから届く姿勢に合わせて、キャラと現在のカメラを表示する。
/// DirectX（左手系）→ WPF（右手系）の変換として、取り込み時に Z を反転する。
/// </summary>
public partial class ScenePreview : UserControl
{
    // 見た目の定数
    private const double GroundHalf = 1200.0;   // 地面グリッドの広さ（±）
    private const double GridStep = 100.0;      // グリッド間隔
    private const double LineHalfWidth = 1.5;   // グリッド線の太さ
    private const double GroundY = -100.0;      // 地面の高さ（ゲームの草の板ポリに合わせる）

    private readonly ModelVisual3D _staticRoot = new();   // 地面・軸（作り直さない）
    private readonly ModelVisual3D _dynamicRoot = new();  // キャラ・カメラ（毎フレーム更新）

    private GeometryModel3D? _p1Model;
    private GeometryModel3D? _p2Model;
    private GeometryModel3D? _camGizmo;

    private MeshGeometry3D? _proxyMesh;     // モデル未書き出し時の仮の箱
    private MeshGeometry3D? _frustumMesh;   // カメラを表す四角錐

    // 俯瞰カメラの状態
    private double _orbitYaw = 0.7;
    private double _orbitPitch = 0.55;
    private double _orbitDistance = 1400.0;
    private Point3D _orbitTarget = new(0, 40, 0);
    private Point _lastMouse;
    private bool _dragging;

    public ScenePreview()
    {
        InitializeComponent();

        SceneRoot.Children.Add(_staticRoot);
        SceneRoot.Children.Add(_dynamicRoot);

        BuildLights();
        BuildGroundAndAxes();

        _proxyMesh = BuildBox(60, 180, 40);      // 人の当たり判定くらいの箱
        _frustumMesh = BuildFrustum();

        MouseDown += OnMouseDownHandler;
        MouseMove += OnMouseMoveHandler;
        MouseUp += OnMouseUpHandler;
        MouseWheel += OnMouseWheelHandler;

        UpdateOrbitCamera();
    }

    /// <summary>ゲーム視点で見るか（false なら俯瞰）。</summary>
    public bool FollowGameCamera { get; set; }

    /// <summary>モデル探索用のライブラリルート。</summary>
    public string LibraryRoot { get; set; } = "";

    // ---------------- 外部から呼ぶ更新 ----------------

    /// <summary>ゲームの状態を反映する。</summary>
    public void Apply(GameSyncState s, bool connected)
    {
        StatusText.Text = connected
            ? $"接続中  frame {s.Frame}" + (s.IsRecording ? "  ●録画中" : "")
            : "未接続（ゲームを起動してください）";
        StatusText.Foreground = new SolidColorBrush(connected
            ? Color.FromRgb(0x7F, 0xD4, 0xA8)
            : Color.FromRgb(0x8F, 0xA0, 0xB8));

        ModeText.Text = FollowGameCamera ? "ゲーム視点" : "俯瞰";

        if (!connected) return;

        UpdateCharacter(ref _p1Model, s.P1Mesh, s.P1Pos, s.P1RotY, s.P1Scale,
            Color.FromRgb(0x6E, 0xA8, 0xFF));
        UpdateCharacter(ref _p2Model, s.P2Mesh, s.P2Pos, s.P2RotY, s.P2Scale,
            Color.FromRgb(0xFF, 0x9E, 0x6E));

        UpdateCameraGizmo(s);

        if (FollowGameCamera) ApplyGameCamera(s);
        else
        {
            // 俯瞰時は2キャラの中間を見る
            _orbitTarget = new Point3D(
                (s.P1Pos.X + s.P2Pos.X) * 0.5,
                (s.P1Pos.Y + s.P2Pos.Y) * 0.5 + 60,
                (-s.P1Pos.Z + -s.P2Pos.Z) * 0.5);

            UpdateOrbitCamera();
        }
    }

    /// <summary>モデルを書き出し直したときに呼ぶ（次の更新で読み直す）。</summary>
    public void ReloadModels()
    {
        PreviewMeshLoader.ClearCache();

        _dynamicRoot.Children.Clear();
        _p1Model = null;
        _p2Model = null;
        _camGizmo = null;
    }

    // ---------------- キャラ・カメラ ----------------

    private void UpdateCharacter(ref GeometryModel3D? model, string meshId,
                                 Vec3 pos, double rotY, double scale, Color color)
    {
        // 書き出し済みモデルがあれば使い、無ければ箱で代用する
        var real = PreviewMeshLoader.Load(LibraryRoot, meshId);
        var geo = real ?? _proxyMesh;

        // 仮の箱は最初から人サイズなので、モデル用のスケールは掛けない
        if (real is null) scale = 1.0;

        if (model is null || !ReferenceEquals(model.Geometry, geo))
        {
            if (model is not null) RemoveModel(model);

            model = new GeometryModel3D
            {
                Geometry = geo,
                Material = new DiffuseMaterial(new SolidColorBrush(color)),
                BackMaterial = new DiffuseMaterial(new SolidColorBrush(color))  // 巻き順対策
            };

            _dynamicRoot.Children.Add(new ModelVisual3D { Content = model });
        }

        // Z反転しているので、Y軸まわりの回転も符号が反転する
        var t = new Transform3DGroup();
        t.Children.Add(new ScaleTransform3D(scale, scale, scale));
        t.Children.Add(new RotateTransform3D(new AxisAngleRotation3D(
            new Vector3D(0, 1, 0), -rotY * 180.0 / Math.PI)));
        t.Children.Add(new TranslateTransform3D(pos.X, pos.Y, -pos.Z));

        model.Transform = t;
    }

    private void UpdateCameraGizmo(GameSyncState s)
    {
        if (_camGizmo is null)
        {
            _camGizmo = new GeometryModel3D
            {
                Geometry = _frustumMesh,
                Material = new DiffuseMaterial(new SolidColorBrush(Color.FromArgb(0xB0, 0xFF, 0x60, 0x60))),
                BackMaterial = new DiffuseMaterial(new SolidColorBrush(Color.FromArgb(0x60, 0xFF, 0x60, 0x60)))
            };
            _dynamicRoot.Children.Add(new ModelVisual3D { Content = _camGizmo });
        }

        // ゲーム視点のときは自分の姿なので消す
        _camGizmo.Transform = FollowGameCamera
            ? new ScaleTransform3D(0, 0, 0)
            : BuildLookAtTransform(ToWpf(s.CamPos), ToWpf(s.CamLook), ToWpfVec(s.CamUp));
    }

    private void ApplyGameCamera(GameSyncState s)
    {
        var pos = ToWpf(s.CamPos);
        var look = ToWpf(s.CamLook);
        var up = ToWpfVec(s.CamUp);

        var dir = look - pos;
        if (dir.LengthSquared < 1e-9) return;

        ViewCamera.Position = pos;
        ViewCamera.LookDirection = dir;
        ViewCamera.UpDirection = up;

        // DirectX は垂直FOV、WPF は水平FOV。アスペクト比で換算する。
        double aspect = ActualWidth > 0 && ActualHeight > 0 ? ActualWidth / ActualHeight : 16.0 / 9.0;
        double vRad = s.Fov * Math.PI / 180.0;
        double hRad = 2.0 * Math.Atan(Math.Tan(vRad * 0.5) * aspect);

        ViewCamera.FieldOfView = Math.Clamp(hRad * 180.0 / Math.PI, 1.0, 175.0);
    }

    /// <summary>位置と注視点から、模型を向ける変換を作る（右手系）。</summary>
    private static Transform3D BuildLookAtTransform(Point3D pos, Point3D look, Vector3D up)
    {
        var fwd = look - pos;
        if (fwd.LengthSquared < 1e-9) return new TranslateTransform3D(pos.X, pos.Y, pos.Z);
        fwd.Normalize();

        if (up.LengthSquared < 1e-9) up = new Vector3D(0, 1, 0);

        // 右手系の直交基底（X = Y×Z, Y = Z×X）
        var right = Vector3D.CrossProduct(up, fwd);
        if (right.LengthSquared < 1e-9) right = Vector3D.CrossProduct(new Vector3D(0, 0, 1), fwd);
        right.Normalize();

        var trueUp = Vector3D.CrossProduct(fwd, right);

        var m = new Matrix3D(
            right.X, right.Y, right.Z, 0,
            trueUp.X, trueUp.Y, trueUp.Z, 0,
            fwd.X, fwd.Y, fwd.Z, 0,
            pos.X, pos.Y, pos.Z, 1);

        return new MatrixTransform3D(m);
    }

    /// <summary>DirectX（左手系）の位置を WPF（右手系）へ。</summary>
    private static Point3D ToWpf(Vec3 v) => new(v.X, v.Y, -v.Z);

    /// <summary>DirectX（左手系）の方向ベクトルを WPF（右手系）へ。</summary>
    private static Vector3D ToWpfVec(Vec3 v) => new(v.X, v.Y, -v.Z);

    // ---------------- 俯瞰カメラ操作 ----------------

    private void UpdateOrbitCamera()
    {
        if (FollowGameCamera) return;

        double cp = Math.Cos(_orbitPitch), sp = Math.Sin(_orbitPitch);
        var offset = new Vector3D(
            Math.Sin(_orbitYaw) * cp,
            sp,
            Math.Cos(_orbitYaw) * cp) * _orbitDistance;

        ViewCamera.Position = _orbitTarget + offset;
        ViewCamera.LookDirection = -offset;
        ViewCamera.UpDirection = new Vector3D(0, 1, 0);
        ViewCamera.FieldOfView = 55;
    }

    private void OnMouseDownHandler(object sender, MouseButtonEventArgs e)
    {
        if (FollowGameCamera) return;
        _dragging = true;
        _lastMouse = e.GetPosition(this);
        CaptureMouse();
    }

    private void OnMouseMoveHandler(object sender, MouseEventArgs e)
    {
        if (!_dragging || FollowGameCamera) return;

        var p = e.GetPosition(this);
        _orbitYaw -= (p.X - _lastMouse.X) * 0.01;
        _orbitPitch = Math.Clamp(_orbitPitch + (p.Y - _lastMouse.Y) * 0.01, -1.4, 1.4);
        _lastMouse = p;

        UpdateOrbitCamera();
    }

    private void OnMouseUpHandler(object sender, MouseButtonEventArgs e)
    {
        _dragging = false;
        ReleaseMouseCapture();
    }

    private void OnMouseWheelHandler(object sender, MouseWheelEventArgs e)
    {
        if (FollowGameCamera) return;

        _orbitDistance = Math.Clamp(_orbitDistance * (e.Delta > 0 ? 0.9 : 1.1), 100.0, 6000.0);
        UpdateOrbitCamera();
    }

    // ---------------- 静的なシーン要素 ----------------

    private void BuildLights()
    {
        var lights = new Model3DGroup();
        lights.Children.Add(new AmbientLight(Color.FromRgb(0x60, 0x66, 0x72)));
        lights.Children.Add(new DirectionalLight(Color.FromRgb(0xC8, 0xCE, 0xD8), new Vector3D(-0.4, -1, -0.6)));
        lights.Children.Add(new DirectionalLight(Color.FromRgb(0x50, 0x58, 0x68), new Vector3D(0.6, -0.3, 0.7)));

        _staticRoot.Children.Add(new ModelVisual3D { Content = lights });
    }

    private void BuildGroundAndAxes()
    {
        var group = new Model3DGroup();

        // 地面グリッド
        var grid = new MeshGeometry3D();
        for (double v = -GroundHalf; v <= GroundHalf + 0.1; v += GridStep)
        {
            AddQuad(grid, new Point3D(-GroundHalf, GroundY, v - LineHalfWidth),
                          new Point3D(GroundHalf, GroundY, v - LineHalfWidth),
                          new Point3D(GroundHalf, GroundY, v + LineHalfWidth),
                          new Point3D(-GroundHalf, GroundY, v + LineHalfWidth));

            AddQuad(grid, new Point3D(v - LineHalfWidth, GroundY, -GroundHalf),
                          new Point3D(v - LineHalfWidth, GroundY, GroundHalf),
                          new Point3D(v + LineHalfWidth, GroundY, GroundHalf),
                          new Point3D(v + LineHalfWidth, GroundY, -GroundHalf));
        }

        group.Children.Add(MakeModel(grid, Color.FromRgb(0x33, 0x3B, 0x4A)));

        // ワールド軸（X=赤 / Y=緑 / Z=青）。Z は反転して表示する。
        group.Children.Add(MakeModel(BuildAxis(new Vector3D(1, 0, 0), 300), Color.FromRgb(0xD0, 0x50, 0x50)));
        group.Children.Add(MakeModel(BuildAxis(new Vector3D(0, 1, 0), 200), Color.FromRgb(0x50, 0xC0, 0x60)));
        group.Children.Add(MakeModel(BuildAxis(new Vector3D(0, 0, -1), 300), Color.FromRgb(0x50, 0x80, 0xD8)));

        _staticRoot.Children.Add(new ModelVisual3D { Content = group });
    }

    private static GeometryModel3D MakeModel(MeshGeometry3D mesh, Color color)
    {
        mesh.Freeze();
        var mat = new DiffuseMaterial(new SolidColorBrush(color));

        return new GeometryModel3D { Geometry = mesh, Material = mat, BackMaterial = mat };
    }

    /// <summary>原点から方向 dir へ伸びる細い棒。</summary>
    private static MeshGeometry3D BuildAxis(Vector3D dir, double length)
    {
        const double w = 3.0;
        var mesh = new MeshGeometry3D();

        var end = new Point3D(dir.X * length, dir.Y * length, dir.Z * length);

        // 方向に対して垂直な2ベクトルを作る
        var any = Math.Abs(dir.Y) > 0.9 ? new Vector3D(1, 0, 0) : new Vector3D(0, 1, 0);
        var a = Vector3D.CrossProduct(dir, any); a.Normalize(); a *= w;
        var b = Vector3D.CrossProduct(dir, a); b.Normalize(); b *= w;

        AddQuad(mesh, new Point3D(-a.X, -a.Y, -a.Z), new Point3D(a.X, a.Y, a.Z),
                      new Point3D(end.X + a.X, end.Y + a.Y, end.Z + a.Z),
                      new Point3D(end.X - a.X, end.Y - a.Y, end.Z - a.Z));

        AddQuad(mesh, new Point3D(-b.X, -b.Y, -b.Z), new Point3D(b.X, b.Y, b.Z),
                      new Point3D(end.X + b.X, end.Y + b.Y, end.Z + b.Z),
                      new Point3D(end.X - b.X, end.Y - b.Y, end.Z - b.Z));

        return mesh;
    }

    /// <summary>モデル未書き出し時に使う仮の箱（足元が原点）。</summary>
    private static MeshGeometry3D BuildBox(double w, double h, double d)
    {
        double x = w * 0.5, z = d * 0.5;
        var mesh = new MeshGeometry3D();

        var p = new[]
        {
            new Point3D(-x, 0, -z), new Point3D( x, 0, -z), new Point3D( x, 0,  z), new Point3D(-x, 0,  z),
            new Point3D(-x, h, -z), new Point3D( x, h, -z), new Point3D( x, h,  z), new Point3D(-x, h,  z),
        };

        AddQuad(mesh, p[0], p[1], p[2], p[3]);   // 底
        AddQuad(mesh, p[7], p[6], p[5], p[4]);   // 天
        AddQuad(mesh, p[0], p[4], p[5], p[1]);   // 手前
        AddQuad(mesh, p[2], p[6], p[7], p[3]);   // 奥
        AddQuad(mesh, p[3], p[7], p[4], p[0]);   // 左
        AddQuad(mesh, p[1], p[5], p[6], p[2]);   // 右

        return mesh;
    }

    /// <summary>カメラを表す四角錐。原点が視点で、ローカル +Z が視線方向。</summary>
    private static MeshGeometry3D BuildFrustum()
    {
        const double len = 90.0, w = 42.0, h = 26.0;
        var mesh = new MeshGeometry3D();

        var apex = new Point3D(0, 0, 0);
        var a = new Point3D(-w, -h, len);
        var b = new Point3D(w, -h, len);
        var c = new Point3D(w, h, len);
        var d = new Point3D(-w, h, len);

        AddTriangle(mesh, apex, a, b);
        AddTriangle(mesh, apex, b, c);
        AddTriangle(mesh, apex, c, d);
        AddTriangle(mesh, apex, d, a);
        AddQuad(mesh, a, b, c, d);   // 前面（フィルム面）

        return mesh;
    }

    private static void AddTriangle(MeshGeometry3D mesh, Point3D a, Point3D b, Point3D c)
    {
        int i = mesh.Positions.Count;
        mesh.Positions.Add(a); mesh.Positions.Add(b); mesh.Positions.Add(c);
        mesh.TriangleIndices.Add(i); mesh.TriangleIndices.Add(i + 1); mesh.TriangleIndices.Add(i + 2);
    }

    private static void AddQuad(MeshGeometry3D mesh, Point3D a, Point3D b, Point3D c, Point3D d)
    {
        AddTriangle(mesh, a, b, c);
        AddTriangle(mesh, a, c, d);
    }

    /// <summary>差し替え時に、その GeometryModel3D を包んでいる ModelVisual3D を外す。</summary>
    private void RemoveModel(GeometryModel3D model)
    {
        var host = _dynamicRoot.Children.OfType<ModelVisual3D>()
                                        .FirstOrDefault(v => ReferenceEquals(v.Content, model));

        if (host is not null) _dynamicRoot.Children.Remove(host);
    }
}
