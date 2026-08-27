using System.Collections.Specialized;
using System.ComponentModel;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using CameraWorkStudio.Models;

namespace CameraWorkStudio.Controls;

/// <summary>
/// カメラワークのタイムライン。
/// 上段＝時間ルーラ、中段＝キー（◆）、下段＝速度グラフ、全体に再生ヘッド。
/// キーはドラッグで時間移動でき、空きをクリックすると再生ヘッドが動く。
/// </summary>
public sealed class TimelineControl : FrameworkElement
{
    // レイアウト定数
    private const double RulerHeight = 20.0;
    private const double TrackHeight = 34.0;
    private const double PadLeft = 8.0;
    private const double PadRight = 8.0;
    private const double KeyRadius = 6.0;

    // 色
    private static readonly Brush BgBrush = new SolidColorBrush(Color.FromRgb(0x22, 0x27, 0x31));
    private static readonly Brush RulerBrush = new SolidColorBrush(Color.FromRgb(0x2C, 0x32, 0x3E));
    private static readonly Brush GridPen = new SolidColorBrush(Color.FromRgb(0x3A, 0x42, 0x52));
    private static readonly Brush TextBrush = new SolidColorBrush(Color.FromRgb(0x9A, 0xA6, 0xBA));
    private static readonly Brush KeyBrush = new SolidColorBrush(Color.FromRgb(0xE6, 0xB4, 0x28));
    private static readonly Brush KeySelBrush = new SolidColorBrush(Color.FromRgb(0xFF, 0xE7, 0x8A));
    private static readonly Brush SpeedBrush = new SolidColorBrush(Color.FromRgb(0x5A, 0xC8, 0x96));
    private static readonly Brush HeadBrush = new SolidColorBrush(Color.FromRgb(0xFF, 0x54, 0x54));

    private readonly Pen _gridPen = new(GridPen, 1);
    private readonly Pen _keyOutline = new(new SolidColorBrush(Color.FromRgb(0x1A, 0x1E, 0x26)), 1);
    private readonly Pen _speedPen = new(SpeedBrush, 1.6);
    private readonly Pen _headPen = new(HeadBrush, 1.6);
    private readonly Pen _baselinePen = new(new SolidColorBrush(Color.FromRgb(0x46, 0x50, 0x62)), 1);

    private CameraKey? _dragKey;

    static TimelineControl()
    {
        // 静的な Brush/Pen はスレッド間で共有するため凍結しておく
        BgBrush.Freeze(); RulerBrush.Freeze(); GridPen.Freeze(); TextBrush.Freeze();
        KeyBrush.Freeze(); KeySelBrush.Freeze(); SpeedBrush.Freeze(); HeadBrush.Freeze();
    }

    public TimelineControl()
    {
        Focusable = true;
        MinHeight = RulerHeight + TrackHeight + 60;
    }

    // ---------------- 依存プロパティ ----------------

    public static readonly DependencyProperty TakeProperty =
        DependencyProperty.Register(nameof(Take), typeof(CameraTake), typeof(TimelineControl),
            new FrameworkPropertyMetadata(null, FrameworkPropertyMetadataOptions.AffectsRender, OnTakeChanged));

    public CameraTake? Take
    {
        get => (CameraTake?)GetValue(TakeProperty);
        set => SetValue(TakeProperty, value);
    }

    public static readonly DependencyProperty SelectedKeyProperty =
        DependencyProperty.Register(nameof(SelectedKey), typeof(CameraKey), typeof(TimelineControl),
            new FrameworkPropertyMetadata(null,
                FrameworkPropertyMetadataOptions.AffectsRender | FrameworkPropertyMetadataOptions.BindsTwoWayByDefault));

    public CameraKey? SelectedKey
    {
        get => (CameraKey?)GetValue(SelectedKeyProperty);
        set => SetValue(SelectedKeyProperty, value);
    }

    public static readonly DependencyProperty CurrentTimeProperty =
        DependencyProperty.Register(nameof(CurrentTime), typeof(double), typeof(TimelineControl),
            new FrameworkPropertyMetadata(0.0,
                FrameworkPropertyMetadataOptions.AffectsRender | FrameworkPropertyMetadataOptions.BindsTwoWayByDefault));

    public double CurrentTime
    {
        get => (double)GetValue(CurrentTimeProperty);
        set => SetValue(CurrentTimeProperty, value);
    }

    private static void OnTakeChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
    {
        var self = (TimelineControl)d;

        // 前のテイクの購読を解除し、新しいテイクを購読して自動再描画する
        if (e.OldValue is CameraTake oldTake)
        {
            oldTake.Keys.CollectionChanged -= self.OnKeysChanged;
            self.UnsubscribeKeys(oldTake);
        }

        if (e.NewValue is CameraTake newTake)
        {
            newTake.Keys.CollectionChanged += self.OnKeysChanged;
            self.SubscribeKeys(newTake);
        }

        self.InvalidateVisual();
    }

    private void OnKeysChanged(object? sender, NotifyCollectionChangedEventArgs e)
    {
        if (e.OldItems is not null)
            foreach (CameraKey k in e.OldItems) k.PropertyChanged -= OnKeyPropertyChanged;

        if (e.NewItems is not null)
            foreach (CameraKey k in e.NewItems) k.PropertyChanged += OnKeyPropertyChanged;

        InvalidateVisual();
    }

    private void OnKeyPropertyChanged(object? sender, PropertyChangedEventArgs e) => InvalidateVisual();

    private void SubscribeKeys(CameraTake take)
    {
        foreach (var k in take.Keys) k.PropertyChanged += OnKeyPropertyChanged;
    }

    private void UnsubscribeKeys(CameraTake take)
    {
        foreach (var k in take.Keys) k.PropertyChanged -= OnKeyPropertyChanged;
    }

    // ---------------- 座標変換 ----------------

    private double Duration => Math.Max(Take?.Duration ?? 0.0, 0.001);
    private double PlotWidth => Math.Max(ActualWidth - PadLeft - PadRight, 1.0);

    private double TimeToX(double t) => PadLeft + (t / Duration) * PlotWidth;
    private double XToTime(double x) => Math.Clamp((x - PadLeft) / PlotWidth, 0.0, 1.0) * Duration;

    private double TrackCenterY => RulerHeight + TrackHeight * 0.5;
    private double SpeedTop => RulerHeight + TrackHeight;
    private double SpeedHeight => Math.Max(ActualHeight - SpeedTop - 4, 10.0);

    // ---------------- 描画 ----------------

    protected override void OnRender(DrawingContext dc)
    {
        double w = ActualWidth, h = ActualHeight;

        dc.DrawRectangle(BgBrush, null, new Rect(0, 0, w, h));
        dc.DrawRectangle(RulerBrush, null, new Rect(0, 0, w, RulerHeight));

        if (Take is null || Take.Keys.Count == 0)
        {
            DrawText(dc, "キーがありません（「録画から取り込み」または「キー追加」）", PadLeft, RulerHeight + 8);
            return;
        }

        DrawRuler(dc, h);
        DrawSpeedGraph(dc);
        DrawKeys(dc);
        DrawPlayhead(dc, h);
    }

    private void DrawRuler(DrawingContext dc, double h)
    {
        // 目盛り間隔は表示幅に合わせて自動で切り替える
        double[] steps = { 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0 };
        double step = steps[^1];

        foreach (var s in steps)
        {
            if (PlotWidth * (s / Duration) >= 60.0) { step = s; break; }
        }

        for (double t = 0; t <= Duration + 1e-9; t += step)
        {
            double x = TimeToX(t);
            dc.DrawLine(_gridPen, new Point(x, 0), new Point(x, h));
            DrawText(dc, t.ToString("0.##", CultureInfo.InvariantCulture) + "s", x + 3, 3);
        }
    }

    private void DrawSpeedGraph(DrawingContext dc)
    {
        if (Take is null || Take.Keys.Count < 2) return;

        double top = SpeedTop, hh = SpeedHeight;

        // 縦軸は最大速度に合わせる（最低でも 1.0 は入るように）
        double vmax = 1.0;
        foreach (var k in Take.Keys) vmax = Math.Max(vmax, k.Speed);
        vmax *= 1.15;

        double SpeedToY(double s) => top + hh - (s / vmax) * hh;

        // 1.0 の基準線
        double baseY = SpeedToY(1.0);
        dc.DrawLine(_baselinePen, new Point(PadLeft, baseY), new Point(ActualWidth - PadRight, baseY));
        DrawText(dc, "速度 1.0", PadLeft + 2, baseY - 14);

        var geo = new StreamGeometry();
        using (var ctx = geo.Open())
        {
            ctx.BeginFigure(new Point(TimeToX(Take.Keys[0].T), SpeedToY(Take.Keys[0].Speed)), false, false);

            for (int i = 1; i < Take.Keys.Count; i++)
                ctx.LineTo(new Point(TimeToX(Take.Keys[i].T), SpeedToY(Take.Keys[i].Speed)), true, false);
        }
        geo.Freeze();
        dc.DrawGeometry(null, _speedPen, geo);
    }

    private void DrawKeys(DrawingContext dc)
    {
        if (Take is null) return;

        double cy = TrackCenterY;

        foreach (var k in Take.Keys)
        {
            double x = TimeToX(k.T);
            bool sel = ReferenceEquals(k, SelectedKey);
            double r = sel ? KeyRadius + 1.5 : KeyRadius;

            // ◆（ひし形）で描く
            var geo = new StreamGeometry();
            using (var ctx = geo.Open())
            {
                ctx.BeginFigure(new Point(x, cy - r), true, true);
                ctx.LineTo(new Point(x + r, cy), true, false);
                ctx.LineTo(new Point(x, cy + r), true, false);
                ctx.LineTo(new Point(x - r, cy), true, false);
            }
            geo.Freeze();

            dc.DrawGeometry(sel ? KeySelBrush : KeyBrush, _keyOutline, geo);

            // ロールが付いているキーは下に印を出す
            if (Math.Abs(k.Roll) > 0.01)
                DrawText(dc, k.Roll.ToString("0", CultureInfo.InvariantCulture) + "°", x + r + 2, cy - 8);
        }
    }

    private void DrawPlayhead(DrawingContext dc, double h)
    {
        double x = TimeToX(Math.Clamp(CurrentTime, 0, Duration));
        dc.DrawLine(_headPen, new Point(x, 0), new Point(x, h));
    }

    private void DrawText(DrawingContext dc, string text, double x, double y)
    {
        var ft = new FormattedText(text, CultureInfo.CurrentUICulture, FlowDirection.LeftToRight,
            new Typeface("Yu Gothic UI"), 10.5, TextBrush, VisualTreeHelper.GetDpi(this).PixelsPerDip);

        dc.DrawText(ft, new Point(x, y));
    }

    // ---------------- 操作 ----------------

    protected override void OnMouseLeftButtonDown(MouseButtonEventArgs e)
    {
        base.OnMouseLeftButtonDown(e);
        Focus();

        if (Take is null || Take.Keys.Count == 0) return;

        var p = e.GetPosition(this);
        var hit = HitTestKey(p);

        if (hit is not null)
        {
            SelectedKey = hit;
            _dragKey = hit;              // キーを掴んだ：時間移動を始める
            CaptureMouse();
        }
        else
        {
            CurrentTime = XToTime(p.X);  // 空きをクリック：再生ヘッドを移動
        }

        InvalidateVisual();
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);

        if (_dragKey is null || Take is null || e.LeftButton != MouseButtonState.Pressed) return;

        int i = Take.Keys.IndexOf(_dragKey);
        if (i < 0) return;

        // 隣のキーを追い越さないように挟み込む
        double min = i > 0 ? Take.Keys[i - 1].T + 0.001 : 0.0;
        double max = i < Take.Keys.Count - 1 ? Take.Keys[i + 1].T - 0.001 : Duration;

        _dragKey.T = Math.Clamp(XToTime(e.GetPosition(this).X), min, max);

        InvalidateVisual();
    }

    protected override void OnMouseLeftButtonUp(MouseButtonEventArgs e)
    {
        base.OnMouseLeftButtonUp(e);

        if (_dragKey is not null)
        {
            _dragKey = null;
            ReleaseMouseCapture();
            Take?.RecalculateDuration();
        }
    }

    private CameraKey? HitTestKey(Point p)
    {
        if (Take is null) return null;
        if (Math.Abs(p.Y - TrackCenterY) > TrackHeight * 0.5) return null;

        CameraKey? best = null;
        double bestDx = KeyRadius + 4;

        foreach (var k in Take.Keys)
        {
            double dx = Math.Abs(p.X - TimeToX(k.T));
            if (dx < bestDx) { bestDx = dx; best = k; }
        }
        return best;
    }
}
