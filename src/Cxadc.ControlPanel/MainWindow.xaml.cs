// SPDX-License-Identifier: GPL-2.0-or-later
using Microsoft.UI.Xaml;
using Cxadc_ControlPanel.Services;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Cxadc_ControlPanel;

/// <summary>
/// The application window. This hosts a Frame that displays pages. Add your
/// UI and logic to MainPage.xaml / MainPage.xaml.cs instead of here so you
/// can use Page features such as navigation events and the Loaded lifecycle.
/// </summary>
public sealed partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();

        SetLocalizedTitle(LanguageSettings.Get("AppTitle"));

        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);

        AppWindow.SetIcon("Assets/CXADC-Control-Panel.ico");

        // Navigate the root frame to the main page on startup.
        RootFrame.Navigate(typeof(MainPage));
    }

    public void SetLocalizedTitle(string title)
    {
        Title = title;
        AppTitleBar.Title = title;
    }
}
