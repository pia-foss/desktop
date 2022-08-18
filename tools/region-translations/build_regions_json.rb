#! /usr/bin/env ruby

require 'json'

# Load gps and country_groups; these are passed directly through to the output
gps = JSON.parse(File.read('gps.json'))
countryGroups = JSON.parse(File.read('country_groups.json'))

# Load all translation files
langs = {}
Dir['translations/*.json'].each do |t|
    locale = t.match(/translations\/(.*)\.json/)[1]
    langs[locale] = JSON.parse(File.read(t))
end

translations = {}
# For each translation key in en-US, list all translations
langs["en-US"].each do |translationKey, enUsValue|
    translations[translationKey] = {}
    # enUsValue isn't used since we iterate through all languages anyway.
    # We do need to render the translated value for en-US, in a few cases
    # this differs from the string key
    langs.each do |langKey, langData|
        translatedValue = langData[translationKey]
        if(translatedValue.is_a? String)
            # We also need to add a zero-width-space to SE Streaming Optimized in
            # order to prepare for the regions list refactor merge.
            # The new implementation adapts the v2 metadata to the v3 model,
            #which provides separate “country prefix” and “region name” texts
            # (i.e. “SE “ and “Streaming Optimized”; these are combined in v2).
            #
            # The v2 compatibility path generates these by finding the common
            # prefix among regions in a country, which works for all regions/languages
            # except Sweden.  Sweden only has two regions, “Stockholm” and “Streaming Optimized”,
            # which both happen to begin with “St”, which fools the prefix detection.
            #
            # We need to put a ZWS between “SE “ and “Streaming Optimized” to work with this.
            markedName = translatedValue.gsub("SE Str", "SE \u200BStr")

            translations[translationKey][langKey] = markedName
        else
            STDERR.puts("No translation for #{translationKey} in #{langKey} (got #{translatedValue})")
        end
    end
end

Dir.mkdir('out') if not Dir.exist?('out')

regionsData = {
    translations: translations,
    gps: gps,
    country_groups: countryGroups
}

File.open('out/regions.json', 'wb') do |f|
    f.write(JSON.pretty_generate(regionsData))
end

