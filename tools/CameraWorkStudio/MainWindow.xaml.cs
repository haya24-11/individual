using System.Windows;
using CameraWorkStudio.ViewModels;

namespace CameraWorkStudio;

public partial class MainWindow : Window
{
    private MainViewModel? _vm;

    public MainWindow()
    {
        InitializeComponent();

        Loaded += OnLoadedHandler;
        Closed += OnClosedHandler;
    }

    private void OnLoadedHandler(object sender, RoutedEventArgs e)
    {
        _vm = DataContext as MainViewModel;
        if (_vm is null) return;

        Preview.LibraryRoot = _vm.LibraryPath;

        // ゲームから新しいフレームが届くたびに3Dプレビューへ反映する
        _vm.Sync.Updated += OnSyncUpdated;
        _vm.Sync.ConnectionChanged += OnSyncUpdated;

        // 未接続でも「未接続」と出るよう一度描いておく
        OnSyncUpdated(this, EventArgs.Empty);
    }

    private void OnSyncUpdated(object? sender, EventArgs e)
    {
        if (_vm is null) return;

        Preview.FollowGameCamera = _vm.FollowGameCamera;
        Preview.LibraryRoot = _vm.LibraryPath;
        Preview.Apply(_vm.Sync.State, _vm.Sync.IsConnected);
    }

    private void OnClosedHandler(object? sender, EventArgs e)
    {
        if (_vm is null) return;

        _vm.Sync.Updated -= OnSyncUpdated;
        _vm.Sync.ConnectionChanged -= OnSyncUpdated;
        _vm.Sync.Dispose();
    }
}
