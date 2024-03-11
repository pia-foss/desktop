The UI has to be translatable into our supported languages.   
We currently support 18 languages.

This document covers dealing with translation in PIA desktop client.   
How to order new translation from Crowdin (or any other service) is out of the scope of this document.

# Test with pseudolocalization

The most important thing is to test with pseudolocalization, this will make any other problems pretty apparent.

The `Pseudo-translated` language is included with debug builds and can be selected normally.  This simulates translation by:
 - surrounding translatable strings with `[]` (to detect concatenation used to build strings)
 - expands strings by 30% (mainly with underscores, to allow expansion for longer languages)
 - inserts international characters to test rendering support and clipping (only when it doesn't excessively expand the string - this isn't a likely problem to have with Qt Quick)

# Import new translations

On a Linux system:   
 1. Install `libthai-dev`
 2. Run `rake tools`
 3. Run `import_translations.sh translations.zip`
 4. Check that .ts files have been modified

# Things to remember when writing translatable code

## 1. Use `uiTr()` for all user-visible text

Wrap UI text strings in `uiTr()`, for example:

```
Text {
  text: uiTr("Allow LAN access")
  ...
}
```

This is like `qsTr()`, read Qt docs for more info on what that does, like http://doc.qt.io/qt-5/qtquick-internationalization.html.

The only difference is that `uiTr()` introduces a property binding that allows us to re-translate when the selected language is changed.  You should test this, but it usually works fine as long as you use `uiTr()` and don't have any binding loops.

You can't use `qsTr()` itself - lupdate will not detect these strings, this is intentional.

There are only a few things that we do not localize:
 - Technical acronyms (ciphers, transports, etc., mainly on the Connection page.  `Auto` in these lists _is_ localized though.)
 - The product/company name - `Private Internet Access`; although this appears in some strings anyway and is expected to be preserved when the string is translated.
 - Language names in the Language setting itself - these are presented in their own language.

## 2. Re-use existing translations

Just moving a string from one file to another is easy.  If `uiTr("Option one")` used to be in `OptionsPage.qml` and you're moving it to `OptionsSubPage1.qml`, you can keep the existing context with `uiTranslate("OptionsPage", "Option one")`.  Existing translations will still work.

Any UI element that needs to be translated needs to be explicitly marked in the code with uiTr or uiTranslate. Use them as follows:

`uiTr()`:         when you need to translate new text.

`uiTranslate()`:  when you want to re-use the same translation of a uiTr translated text.

Use `//: comment goes here` to add comments for the translators and for screen reader annotations.

## 3. Combine strings with positional arguments

When necessary, combine strings using positional arguments, like `uiTr("Connected (%1)").arg(/*...*/)`.

Don't just concatenate strings.  Also keep in mind that the parentheses in this example might be localized too when the string is translated.

## 4. Translate location names

Location names need to be translated.  These are a bit different since they come from the servers list though.

`Daemon` provides methods to get translated location names (which also provide a retranslate binding):
- If you have a location ID (`us3`, `de_frankfurt`, etc.) - `Daemon.getLocationIdName(locId)`
- If you have a location object (from `Daemon.data.locations`, `Daemon.state.groupedLocations`, `Daemon.state.connectedLocation`, etc.) - `Daemon.getLocationName(loc)`.
  - (Prefer this over `Daemon.getLocationIdName(loc.id)`, because it works even if you are connected to a location that's just been removed from the server list, etc.)
- For country names (such as in the regions list) - `Daemon.getCountryName(countryCode)`

## 5. Use locale-aware dates, times, sorts, and casing

When rendering dates and times, sorting UI strings, or changing UI string casing, do so in the current locale, which is `Client.state.activeLanguage.locale`.

For example:
- A date: `text: new Date(Daemon.account.expirationTime).toLocaleDateString(Qt.locale(Client.state.activeLanguage.locale), Locale.ShortFormat)` (see `AccountModule`)
- Sorting regions: `first.localeCompare(second, Client.state.activeLanguage.locale)` (see `RegionListView`)
- Upper casing: `hoveredSetting.toLocaleUpperCase(Client.state.activeLanguage.locale)` (see `SettingsModule`)

## 6. Avoid `ListModel`/`ListElement`

Declaratively-created `ListElement`s can't use `uiTr()` in their property values.  (`ListElement` values can't be closures.  It has a special exception to allow `qsTr()`, but that wouldn't retranslate property since it can't be re-evaluated.)

Frequently, a plain array is much simpler to use than the `ListModel`.  QML accepts arrays as a data model in place of `ListModel`s (if the elements are objects, use `modelData.<prop>` to access properties instead of `<prop>` or `model.<prop>`).

This is often simpler if the list elements could change too, because the array can be generated by a closure that will be reevaluated when its property dependencies change in usual QML style.

In cases where you really need the dynamic insertion/removal/moving signals from `ListModel`, like the regions list, you will need to separate the translatable strings from the `ListModel`.  `ListElement` does not have a special case for `QT_TR_NOOP()` like `qsTr()`, so you can't use that either.

Instead, you might put an ID in the `ListElement`s, then provide a map from IDs to `usTr()`-ed strings to the code using the model.  The map can be generated by a closure, so it'll retranslate correctly.  Frequently the elements already have some sort of ID, so this is straightforward.

# Editing existing translations

## Manually editing translated strings

Never edit the .ts files in source control.  Always edit the translations in Crowdin, then re-download the translation files.

Editing the .ts files in source control directly would cause those changes to be lost the next time we download the full translation from Crowdin.

## Editing the English source strings

Sometimes the English-language text changes.  Usually, we just let this become a "new" string and retranslate it (it doesn't cost much).  Keep in mind:
* Changes that are trivial in English might not be trivial in translations
* Any detail might have been picked up by translators (if you forgot a period, some of the translations might be missing it too)
* None of us are familiar enough with _all_ of the languages we support to fix them all
* Even when we can potentially figure it out (like adding that period, we could figure out what periods the other strings are using); it's probably cheaper just to let the translators do it

If you really want to edit the English text but keep the translations the same, you will most likely need to copy them over manually.

# Issues with Thai language

Thai word breaking is necessary because the Thai language does not use spaces between words. Without a Thai dictionary, text rendering does not know where to break lines in mid-sentence - it’ll move a whole sentence to a new line, or break arbitrarily in mid-word if the sentence doesn’t fit on a line.

Most programs ship a Thai dictionary with libicu (Unicode utilities library) to do this. Most Linux desktops include this. PIA doesn’t ship the dictionary due to file size.

The thaibreak program uses libthai to find word breaks and inserts “zero width spaces” automatically between words. This allows word breaking to be done correctly without affecting display.

This process is performed by the `import_translation.sh` script.

# Dev tools

PIA DevTools has a hidden checkbox where pressing “F6” cycles through languages, so you can change languages at the touch of a key.   
Access DevTools by pressing ctrl+shift+i (cmd+shift+i on MacOS).
