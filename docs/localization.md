# Localization

`OBS_MODULE_USE_DEFAULT_LOCALE` loads `data/locale/en-US.ini`, then the OBS
language override. `ObsTranslator` adapts the plugin's Qt contexts to
`obs_module_text()` without exposing OBS APIs to the application or UI layers.
Its runtime lifetime encloses the services and views. English source text is
the lookup key, quoted in the INI so spaces and punctuation are supported.
English remains the fallback for absent entries.

Add UI text through `tr("English source")`. Application notifications use
the `TimestampMemo` Qt context; format dynamic values through a translated
template before interpolation. Add the same quoted key to both locale files.
Literal newlines, tabs and quotes are escaped in lookup keys because OBS
decodes escapes in values, but preserves them in keys. Preserve all `%1`, `%2`
placeholders and file-dialog wildcard syntax in translated values.

Only plugin contexts are translated. Qt's own dialogs retain OBS/Qt's native
translations. Saved marker labels, comments and JSON field names are data, not
UI text, and are never rewritten when the display language changes. Restart
OBS after changing its language to recreate translated plugin windows.

The regression test loads the actual Japanese locale file and checks dock,
table, settings, hotkey descriptions, diagnostics, notifications, English
fallback and translator removal on unload.
