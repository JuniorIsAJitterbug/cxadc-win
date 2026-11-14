// SPDX-License-Identifier: GPL-2.0-or-later
using System.Collections.ObjectModel;
using System.Globalization;
using Cxadc_ControlPanel.Models;
using Cxadc_ControlPanel.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Cxadc_ControlPanel;

public sealed partial class MainPage : Page
{
    private readonly CxadcWmiService _cxadc = new();
    private readonly CxadcClockGenService _clockgen = new();
    private readonly DispatcherTimer _statusTimer = new() { Interval = TimeSpan.FromSeconds(3) };
    private bool _ready;
    private bool _refreshing;

    public MainPage()
    {
        InitializeComponent();
        Loaded += MainPage_Loaded;
        _statusTimer.Tick += StatusTimer_Tick;
    }

    private async void MainPage_Loaded(object sender, RoutedEventArgs e)
    {
        _ready = true;
        UpdateLocalizedText();
        _statusTimer.Start();
        await RefreshAllAsync();
    }

    private void UpdateLocalizedText()
    {
        PageTitle.Text = LanguageSettings.Get("AppTitle");
        if (App.Window is MainWindow mainWindow)
            mainWindow.SetLocalizedTitle(LanguageSettings.Get("AppTitle"));
        PageSubtitle.Text = LanguageSettings.Get("Subtitle");
        LanguageLabel.Text = LanguageSettings.Get("Language");
        LanguageComboBox.SelectedIndex = LanguageSettings.CurrentLanguage == "pt-BR" ? 0 : 1;
        CardTab.Header = LanguageSettings.Get("CardTab");
        ClockTab.Header = LanguageSettings.Get("ClockTab");
        AdvancedTab.Header = LanguageSettings.Get("AdvancedTab");
        CardComboBox.Header = LanguageSettings.Get("CaptureCard");
        CardComboBox.PlaceholderText = LanguageSettings.Get("NoCardFound");
        RefreshCardsButton.Content = LanguageSettings.Get("Refresh");
        CardStatusText.Text = LanguageSettings.Get("NotChecked");
        SettingsHeading.Text = LanguageSettings.Get("CaptureSettings");
        VideoMuxComboBox.Header = LanguageSettings.Get("VideoMux");
        LevelNumberBox.Header = LanguageSettings.Get("Level");
        CenterOffsetNumberBox.Header = LanguageSettings.Get("CenterOffset");
        TenBitCheckBox.Content = LanguageSettings.Get("TenBit");
        SixDbCheckBox.Content = LanguageSettings.Get("SixDb");
        ApplyCardButton.Content = LanguageSettings.Get("Apply");
        ReloadCardButton.Content = LanguageSettings.Get("Reload");
        CardHelpText.Text = LanguageSettings.Get("RawValueHelp");
        StatusHeading.Text = LanguageSettings.Get("DriverStatus");
        DriverActiveLabel.Text = LanguageSettings.Get("DriverActive");
        CapturingLabel.Text = LanguageSettings.Get("Capturing");
        ReadersLabel.Text = LanguageSettings.Get("Readers");
        OverflowsLabel.Text = LanguageSettings.Get("Overflows");
        LastGpLabel.Text = LanguageSettings.Get("LastGp");
        InitialPageLabel.Text = LanguageSettings.Get("InitialPage");
        ResetOverflowButton.Content = LanguageSettings.Get("ResetOverflows");
        StatusHelpText.Text = LanguageSettings.Get("AutoRefreshHelp");
        ClockGenComboBox.Header = LanguageSettings.Get("ClockDevice");
        ClockGenComboBox.PlaceholderText = LanguageSettings.Get("NoClockgen");
        RefreshClockButton.Content = LanguageSettings.Get("Refresh");
        ClockSettingsHeading.Text = LanguageSettings.Get("ClockOutputs");
        ClockDeviceInfo.Text = LanguageSettings.Get("ClockConnectHelp");
        Clock0ComboBox.Header = LanguageSettings.Get("Clock0");
        Clock1ComboBox.Header = LanguageSettings.Get("Clock1");
        AdcRateComboBox.Header = LanguageSettings.Get("AdcRate");
        ApplyClockButton.Content = LanguageSettings.Get("ApplyClock");
        ClockHelpText.Text = LanguageSettings.Get("ClockDynamicHelp");
        if (ClockGenComboBox.SelectedItem is ClockGenInfo currentClock)
            ShowClockgen(currentClock);
        RegisterHeading.Text = LanguageSettings.Get("RegisterHeading");
        RegisterWarning.Text = LanguageSettings.Get("RegisterWarning");
        RegisterAddressBox.Header = LanguageSettings.Get("AddressHex");
        RegisterValueBox.Header = LanguageSettings.Get("ValueHex");
        ReadRegisterButton.Content = LanguageSettings.Get("Read");
        WriteRegisterButton.Content = LanguageSettings.Get("Write");
        FunctionsHeading.Text = LanguageSettings.Get("FunctionsHeading");
        FunctionsWarning.Text = LanguageSettings.Get("FunctionsWarning");
        VideoFunctionCheckBox.Content = LanguageSettings.Get("VideoRequired");
        AudioFunctionCheckBox.Content = LanguageSettings.Get("Audio");
        MpegFunctionCheckBox.Content = LanguageSettings.Get("Mpeg");
        IrFunctionCheckBox.Content = LanguageSettings.Get("Infrared");
        LoadFunctionsButton.Content = LanguageSettings.Get("ReadEeprom");
        ApplyFunctionsButton.Content = LanguageSettings.Get("WriteEeprom");
    }

    private async Task RefreshAllAsync()
    {
        if (_refreshing)
            return;
        _refreshing = true;
        try
        {
            var devicesTask = Task.Run(_cxadc.GetDevices);
            var clocksTask = Task.Run(_clockgen.GetDevices);
            var devices = await devicesTask;
            IReadOnlyList<ClockGenInfo> clocks;
            try
            {
                clocks = await clocksTask;
            }
            catch (Exception ex)
            {
                clocks = Array.Empty<ClockGenInfo>();
                ClockStatusText.Text = Format("OperationFailed", ex.Message);
            }

            var previousCardPath = (CardComboBox.SelectedItem as CxadcDevice)?.Path;
            var previousClockPath = (ClockGenComboBox.SelectedItem as ClockGenInfo)?.DevicePath;
            CardComboBox.ItemsSource = new ObservableCollection<CxadcDevice>(devices);
            ClockGenComboBox.ItemsSource = new ObservableCollection<ClockGenInfo>(clocks);
            CardComboBox.SelectedItem = devices.FirstOrDefault(x => x.Path == previousCardPath) ?? devices.FirstOrDefault();
            ClockGenComboBox.SelectedItem = clocks.FirstOrDefault(x => x.DevicePath == previousClockPath) ?? clocks.FirstOrDefault();
            if (devices.Count == 0)
            {
                CardStatusText.Text = LanguageSettings.Get("NoCxadc");
                ClearCardState();
            }
            else
            {
                var card = CardComboBox.SelectedItem as CxadcDevice;
                CardStatusText.Text = card is null
                    ? LanguageSettings.Get("Connected")
                    : Format("ConnectedCard", card.Path, string.IsNullOrWhiteSpace(card.PciLocation) ? "—" : card.PciLocation);
                await LoadCardConfigurationAsync();
            }
            if (clocks.Count == 0)
            {
                ClockDeviceInfo.Text = LanguageSettings.Get("ClockConnectHelp");
                ClockStatusText.Text = "";
                ClearClockRates();
            }
            else if (ClockGenComboBox.SelectedItem is ClockGenInfo selectedClock)
            {
                ShowClockgen(selectedClock);
            }
        }
        catch (Exception ex)
        {
            CardStatusText.Text = Format("OperationFailed", ex.Message);
            ClockStatusText.Text = Format("OperationFailed", ex.Message);
        }
        finally
        {
            _refreshing = false;
        }
    }

    private async void RefreshCardsButton_Click(object sender, RoutedEventArgs e) => await RefreshAllAsync();

    private async void RefreshClockButton_Click(object sender, RoutedEventArgs e) => await RefreshAllAsync();

    private async void CardComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_ready && !_refreshing)
            await LoadCardConfigurationAsync();
    }

    private void ClockGenComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (!_ready || _refreshing)
            return;
        if (ClockGenComboBox.SelectedItem is ClockGenInfo info)
            ShowClockgen(info);
        else
            ClearClockRates();
    }

    private async void ApplyCardButton_Click(object sender, RoutedEventArgs e)
    {
        if (CardComboBox.SelectedItem is not CxadcDevice device)
        {
            CardStatusText.Text = LanguageSettings.Get("ChooseCard");
            return;
        }

        if (!double.IsFinite(LevelNumberBox.Value) || !double.IsFinite(CenterOffsetNumberBox.Value)
            || VideoMuxComboBox.SelectedItem is not ComboBoxItem selectedMux)
        {
            CardStatusText.Text = LanguageSettings.Get("OperationFailed").Replace("{0}", "Invalid setting value");
            return;
        }

        try
        {
            var config = new CxadcConfiguration(
                true,
                int.Parse((string)selectedMux.Tag, CultureInfo.InvariantCulture),
                (int)Math.Round(LevelNumberBox.Value),
                (int)Math.Round(CenterOffsetNumberBox.Value),
                TenBitCheckBox.IsChecked == true,
                SixDbCheckBox.IsChecked == true);
            await Task.Run(() => _cxadc.SetConfiguration(device, config));
            await LoadCardConfigurationAsync();
            CardStatusText.Text = LanguageSettings.Get("SavedCard");
        }
        catch (Exception ex)
        {
            CardStatusText.Text = Format("OperationFailed", ex.Message);
        }
    }

    private async void ReloadCardButton_Click(object sender, RoutedEventArgs e) => await LoadCardConfigurationAsync();

    private async void ResetOverflowButton_Click(object sender, RoutedEventArgs e)
    {
        if (CardComboBox.SelectedItem is not CxadcDevice device)
        {
            CardStatusText.Text = LanguageSettings.Get("ChooseCard");
            return;
        }
        try
        {
            var previous = await Task.Run(() => _cxadc.ResetOverflowCount(device));
            CardStatusText.Text = Format("CounterReset", previous.ToString(CultureInfo.CurrentCulture));
            await RefreshStatusAsync();
        }
        catch (Exception ex)
        {
            CardStatusText.Text = Format("OperationFailed", ex.Message);
        }
    }

    private async void ApplyClockButton_Click(object sender, RoutedEventArgs e)
    {
        if (ClockGenComboBox.SelectedItem is not ClockGenInfo info)
        {
            ClockStatusText.Text = LanguageSettings.Get("ChooseClockgen");
            return;
        }
        if (Clock0ComboBox.SelectedItem is not string clock0 || Clock1ComboBox.SelectedItem is not string clock1
            || AdcRateComboBox.SelectedItem is not string adcRate)
        {
            ClockStatusText.Text = LanguageSettings.Get("SelectAllRates");
            return;
        }

        try
        {
            var current = info;
            if (clock0 != current.ClockRate0)
                current = await Task.Run(() => _clockgen.SetRate(current, ClockOutput.Clock0, clock0));
            if (clock1 != current.ClockRate1)
                current = await Task.Run(() => _clockgen.SetRate(current, ClockOutput.Clock1, clock1));
            if (!uint.TryParse(adcRate, NumberStyles.None, CultureInfo.InvariantCulture, out var requestedAdc))
                throw new InvalidOperationException("Invalid ADC rate value.");
            if (requestedAdc != current.AdcRate)
                current = await Task.Run(() => _clockgen.SetRate(current, ClockOutput.Adc, adcRate));
            ShowClockgen(current);
            ClockGenComboBox.SelectedItem = current;
            ClockStatusText.Text = LanguageSettings.Get("ClockApplied");
        }
        catch (Exception ex)
        {
            ClockStatusText.Text = Format("OperationFailed", ex.Message);
        }
    }

    private async void ReadRegisterButton_Click(object sender, RoutedEventArgs e)
    {
        if (!TryGetSelectedCard(out var device) || !TryParseHex(RegisterAddressBox.Text, out var address))
            return;
        try
        {
            var value = await Task.Run(() => _cxadc.ReadRegister(device, address));
            RegisterValueBox.Text = value.ToString("X8", CultureInfo.InvariantCulture);
            RegisterStatusText.Text = Format("RegisterReadResult", address, value);
        }
        catch (Exception ex)
        {
            RegisterStatusText.Text = Format("OperationFailed", ex.Message);
        }
    }

    private async void WriteRegisterButton_Click(object sender, RoutedEventArgs e)
    {
        if (!TryGetSelectedCard(out var device)
            || !TryParseHex(RegisterAddressBox.Text, out var address)
            || !TryParseHex(RegisterValueBox.Text, out var value))
            return;
        if (!await ConfirmAsync("ConfirmRegisterTitle", "ConfirmRegisterBody"))
            return;
        try
        {
            await Task.Run(() => _cxadc.WriteRegister(device, address, value));
            RegisterStatusText.Text = Format("RegisterWritten", value, address);
        }
        catch (Exception ex)
        {
            RegisterStatusText.Text = Format("OperationFailed", ex.Message);
        }
    }

    private async void LoadFunctionsButton_Click(object sender, RoutedEventArgs e)
    {
        if (!TryGetSelectedCard(out var device))
            return;
        try
        {
            var functions = await Task.Run(() => _cxadc.GetFunctions(device));
            VideoFunctionCheckBox.IsChecked = functions.Video;
            AudioFunctionCheckBox.IsChecked = functions.Audio;
            MpegFunctionCheckBox.IsChecked = functions.Mpeg;
            IrFunctionCheckBox.IsChecked = functions.Infrared;
        }
        catch (Exception ex)
        {
            RegisterStatusText.Text = Format("OperationFailed", ex.Message);
        }
    }

    private async void ApplyFunctionsButton_Click(object sender, RoutedEventArgs e)
    {
        if (!TryGetSelectedCard(out var device))
            return;
        if (!await ConfirmAsync("ConfirmEepromTitle", "ConfirmEepromBody"))
            return;
        try
        {
            var functions = new CxadcFunctions(
                VideoFunctionCheckBox.IsChecked == true,
                AudioFunctionCheckBox.IsChecked == true,
                MpegFunctionCheckBox.IsChecked == true,
                IrFunctionCheckBox.IsChecked == true);
            await Task.Run(() => _cxadc.SetFunctions(device, functions));
            RegisterStatusText.Text = LanguageSettings.Get("FunctionUpdateSuccess");
        }
        catch (Exception ex)
        {
            RegisterStatusText.Text = Format("OperationFailed", ex.Message);
        }
    }

    private void LanguageComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (!_ready || LanguageComboBox.SelectedItem is not ComboBoxItem item || item.Tag is not string language)
            return;
        try
        {
            LanguageSettings.SetLanguage(language);
            UpdateLocalizedText();
        }
        catch (Exception ex)
        {
            CardStatusText.Text = Format("OperationFailed", ex.Message);
        }
    }

    private async void StatusTimer_Tick(object? sender, object e)
    {
        if (_ready && MainTabs.SelectedIndex == 0)
            await RefreshStatusAsync();
    }

    private async Task LoadCardConfigurationAsync()
    {
        if (CardComboBox.SelectedItem is not CxadcDevice device)
            return;
        try
        {
            var configTask = Task.Run(() => _cxadc.GetConfiguration(device));
            var stateTask = Task.Run(() => _cxadc.GetState(device));
            var config = await configTask;
            ApplyConfiguration(config);
            UpdateState(await stateTask);
            CardStatusText.Text = LanguageSettings.Get("LoadedCard");
        }
        catch (Exception ex)
        {
            CardStatusText.Text = Format("OperationFailed", ex.Message);
        }
    }

    private async Task RefreshStatusAsync()
    {
        if (CardComboBox.SelectedItem is not CxadcDevice device)
            return;
        try
        {
            UpdateState(await Task.Run(() => _cxadc.GetState(device)));
        }
        catch
        {
            // A capture application may close or restart the device between status reads.
        }
    }

    private void ApplyConfiguration(CxadcConfiguration config)
    {
        VideoMuxComboBox.SelectedIndex = config.VideoMux is >= 0 and <= 3 ? config.VideoMux : -1;
        LevelNumberBox.Value = config.Level;
        CenterOffsetNumberBox.Value = config.CenterOffset;
        TenBitCheckBox.IsChecked = config.EnableTenbit;
        SixDbCheckBox.IsChecked = config.EnableSixDB;
    }

    private void UpdateState(CxadcState state)
    {
        DriverActiveValue.Text = LanguageSettings.Get(state.Active ? "True" : "False");
        CapturingValue.Text = LanguageSettings.Get(state.IsCapturing ? "True" : "False");
        ReadersValue.Text = state.ReaderCount.ToString(CultureInfo.CurrentCulture);
        OverflowsValue.Text = state.OverUnderflowCount.ToString(CultureInfo.CurrentCulture);
        LastGpValue.Text = state.LastGpCount.ToString(CultureInfo.CurrentCulture);
        InitialPageValue.Text = state.InitialPage.ToString(CultureInfo.CurrentCulture);
    }

    private void ShowClockgen(ClockGenInfo info)
    {
        Clock0ComboBox.ItemsSource = info.ClockRates;
        Clock1ComboBox.ItemsSource = info.ClockRates;
        AdcRateComboBox.ItemsSource = info.AdcRates.Select(x => x.ToString(CultureInfo.InvariantCulture)).ToArray();
        Clock0ComboBox.SelectedItem = info.ClockRate0;
        Clock1ComboBox.SelectedItem = info.ClockRate1;
        AdcRateComboBox.SelectedItem = info.AdcRate.ToString(CultureInfo.InvariantCulture);
        ClockDeviceInfo.Text = Format("ClockInfo", string.IsNullOrWhiteSpace(info.Serial) ? "—" : info.Serial, $"0x{info.FirmwareVersion:X4}");
    }

    private void ClearCardState()
    {
        VideoMuxComboBox.SelectedIndex = -1;
        DriverActiveValue.Text = CapturingValue.Text = ReadersValue.Text = OverflowsValue.Text = LastGpValue.Text = InitialPageValue.Text = "—";
    }

    private void ClearClockRates()
    {
        Clock0ComboBox.ItemsSource = null;
        Clock1ComboBox.ItemsSource = null;
        AdcRateComboBox.ItemsSource = null;
    }

    private bool TryGetSelectedCard(out CxadcDevice device)
    {
        if (CardComboBox.SelectedItem is CxadcDevice selected)
        {
            device = selected;
            return true;
        }
        device = null!;
        RegisterStatusText.Text = LanguageSettings.Get("ChooseCard");
        return false;
    }

    private bool TryParseHex(string? text, out uint value)
    {
        var normalized = (text ?? "").Trim();
        if (normalized.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            normalized = normalized[2..];
        if (TryGetSelectedCard(out _) && uint.TryParse(normalized, NumberStyles.AllowHexSpecifier, CultureInfo.InvariantCulture, out value))
            return true;
        value = 0;
        RegisterStatusText.Text = Format("OperationFailed", "Enter a valid hexadecimal number and select a card.");
        return false;
    }

    private async Task<bool> ConfirmAsync(string titleKey, string bodyKey)
    {
        var dialog = new ContentDialog
        {
            Title = LanguageSettings.Get(titleKey),
            Content = LanguageSettings.Get(bodyKey),
            PrimaryButtonText = LanguageSettings.Get("Yes"),
            CloseButtonText = LanguageSettings.Get("No"),
            DefaultButton = ContentDialogButton.Close,
            XamlRoot = XamlRoot
        };
        return await dialog.ShowAsync() == ContentDialogResult.Primary;
    }

    private static string Format(string key, params object[] args) => string.Format(
        CultureInfo.CurrentCulture,
        LanguageSettings.Get(key),
        args);
}
