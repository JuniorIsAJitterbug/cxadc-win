// SPDX-License-Identifier: GPL-2.0-or-later
using System.Text.Json;
using Windows.ApplicationModel.Resources;
using Windows.Globalization;

namespace Cxadc_ControlPanel.Services;

public static class LanguageSettings
{
    private static readonly string SettingsPath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "CXADC Tools",
        "settings.json");

    public static string CurrentLanguage { get; private set; } = "";

    public static void Initialize()
    {
        var language = ReadSavedLanguage();
        if (language is "pt-BR" or "en-US")
            ApplicationLanguages.PrimaryLanguageOverride = language;
        CurrentLanguage = ResolveLanguage();
    }

    public static void SetLanguage(string language)
    {
        if (language is not ("pt-BR" or "en-US"))
            throw new ArgumentOutOfRangeException(nameof(language));

        ApplicationLanguages.PrimaryLanguageOverride = language;
        CurrentLanguage = language;
        Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath)!);
        File.WriteAllText(SettingsPath, JsonSerializer.Serialize(new UserSettings(language)));
    }

    public static string Get(string key)
    {
        try
        {
            var value = ResourceLoader.GetForViewIndependentUse().GetString(key);
            return string.IsNullOrWhiteSpace(value) ? key : value;
        }
        catch
        {
            return key;
        }
    }

    private static string ResolveLanguage()
    {
        if (ApplicationLanguages.PrimaryLanguageOverride is "pt-BR" or "en-US")
            return ApplicationLanguages.PrimaryLanguageOverride;
        return ApplicationLanguages.Languages.Any(x => x.StartsWith("pt", StringComparison.OrdinalIgnoreCase))
            ? "pt-BR"
            : "en-US";
    }

    private static string ReadSavedLanguage()
    {
        try
        {
            return JsonSerializer.Deserialize<UserSettings>(File.ReadAllText(SettingsPath))?.Language ?? "";
        }
        catch
        {
            return "";
        }
    }

    private sealed record UserSettings(string Language);
}
