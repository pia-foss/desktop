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
            translations[translationKey][langKey] = langData[translationKey]
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

