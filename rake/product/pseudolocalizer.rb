# Pseudolocalization is the process of replacing English characters with more exotic counterparts
# that are still readable to an English speaker.
# This is done to verify there's no glitches when the UI renders these characters - and helps
# to identfy translation-related bugs that could appear with real translations.
class PseudoLocalizer
    # Regex (with capture groups) used to pull out the relevant fields for (pseudo) localization
    TRANSLATION_REGEX = /(<source>([^<]*)<\/source>\n)( *<comment>([^<]*)<\/comment>\n|)( *<extracomment>[^<]*<\/extracomment>\n|)( *)<translation[^>]*>[^<]*<\/translation>/

    CHAR_WIDTHS = {
        ' '=>3.2, '!'=>3.4, '"'=>4.0, '#'=>8.1, '$'=>7.3, '%'=>9.5, '&'=>8.1, '\''=>2.1,
        '('=>4.3, ')'=>4.4, '*'=>5.6, '+'=>7.4, ','=>2.6, '-'=>3.5, '.'=>3.5, '\/'=>5.4,
        '0'=>7.3, '1'=>7.3, '2'=>7.3, '3'=>7.3, '4'=>7.3, '5'=>7.3, '6'=>7.3, '7'=>7.3,
        '8'=>7.3, '9'=>7.3, '=>'=>3.3, ';'=>3.3, '<'=>6.6, '='=>7.3, '>'=>6.8, '?'=>6.2,
        '@'=>11.6, 'A'=>8.4, 'B'=>8.3, 'C'=>8.2, 'D'=>8.6, 'E'=>7.6, 'F'=>7.6, 'G'=>8.9,
        'H'=>9.3, 'I'=>3.7, 'J'=>7.2, 'K'=>8.4, 'L'=>7.0, 'M'=>11.4, 'N'=>9.3, 'O'=>8.9,
        'P'=>8.3, 'Q'=>8.9, 'R'=>8.6, 'S'=>8.1, 'T'=>7.7, 'U'=>8.8, 'V'=>8.2, 'W'=>11.5,
        'X'=>8.2, 'Y'=>8.0, 'Z'=>7.8,'['=>3.5, '\\'=>5.4, ']'=>3.5, '^'=>5.4, '_'=>5.9,
        'a'=>7.1, 'b'=>7.4, 'c'=>6.9, 'd'=>7.4, 'e'=>6.9, 'f'=>4.5, 'g'=>7.4, 'h'=>7.4,
        'i'=>3.3, 'j'=>3.4, 'k'=>6.7, 'l'=>3.3, 'm'=>11.4, 'n'=>7.4, 'o'=>7.4, 'p'=>7.4,
        'q'=>7.4, 'r'=>4.5, 's'=>6.8, 't'=>4.5, 'u'=>7.4, 'v'=>6.5, 'w'=>9.8, 'x'=>6.5,
        'y'=>6.5, 'z'=>6.5, '{'=>4.4, '|'=>3.2, '}'=>4.4, '~'=>8.8, '`'=>4.1,
    }

    SUBSTITIONS = {
        'A'=> 'Á',
        'E'=> 'É',
        'I'=> 'Í',
        'O'=> 'Ó',
        'U'=> 'Ú',
        'a'=> 'á',
        'e'=> 'é',
        'i'=> 'í',
        'o'=> 'ó',
        'u'=> 'ú'
    }

    public

    # Expose class methods for a simplified API
    def self.pseudolocalizeToLang(*args)
        self.new.pseudolocalizeToLang(*args)
    end

    def self.pseudolocalizeString(*args)
        self.new.pseudolocalizeString(*args)
    end

    private

    # Estimate the 'width' of the string
    def stringWidth(str)
        str.each_char.reduce(0) { |a, c| a + (CHAR_WIDTHS[c] || CHAR_WIDTHS['?']) }
    end

    # The number of underscores to use to pad a string.
    # Underscores are added to increase the width for display purposes
    def underscoreCountFor(str)
        # Calculate the number of underscores to use for the string
        # Divide by 2 for the number of underscores on each side
        (stringWidth(str) * 0.3 / CHAR_WIDTHS['_']).round(2)
    end

    # Determine the prefix to use with the string
    # The prefix contains some unusual characters to properly
    # test our UI localization support. Also return the (adjusted) number of underscores
    # to use. These two are connnected so must be calculated together.
    def calculatePrefixAndUnderscoreCount(underscoreCount)
        # Use a few characters to verify rendering / clipping
        # - Dotted capital I as in turkish - verifies top isn't clipped
        # - Cyrillic shcha as in Russian - verifies descender isn't clipped
        # - 'A' kana - script support.
        # (Clipping doesn't look like it will really be an issue, few characters seem
        # to be taller than '[]' in this font anyway)
        #
        # The I is so narrow that we always include it.
        prefix = "\u0130"

        # Don't excessively expand the string, count the shcha and kana as ~2
        # underscores each, they're really wide

        # shcha character
        if underscoreCount >= 2
            prefix += "\u0429"
            underscoreCount -= 2
        end
        # kana character
        if underscoreCount >= 2
            prefix += "\u30A2"
            underscoreCount -= 2
        end

        [prefix, underscoreCount]
    end

    # Given a string, wrap it with underscores and add a prefix.
    # The number of underscores is determined by the string width but can be adjusted
    # depending on prefix..
    # The prefix is comprised of a narrow turkish "I" and shcha and kana characters
    def addUnderscoresAndPrefix(initialUnderscoreCount, &block)
        prefix, adjustedunderscoreCount = calculatePrefixAndUnderscoreCount(initialUnderscoreCount)

        # Determine the underscores for left and right
        leftUnderscores = '_' * (adjustedunderscoreCount / 2).ceil
        rightUnderscores = '_' * (adjustedunderscoreCount / 2).floor + ']'

        # Return the final string adjusted for prefix and underscores
        prefix + leftUnderscores + block.call + rightUnderscores
    end

    # Update flag to indicate we're currently parsing an html entitiy
    # e.g "&quote;" - flag should remain set between the '&' and ';' characters
    # meaning we should not perform any substitutions on characters in "quote"
    def checkForHtmlEntity(char)
        case char
        when '&'
            @inEntity = true
        when ';'
            @inEntity = false
        end
    end

    # Are we currently parsing an html entity?
    # See above.
    def inEntity?
        @inEntity
    end

    def pseudolocalize(str, comment)
        # These time strings used for the connection duration aren't padded.
        # Due to the way these are constructed (localized strings for each time part,
        # then a localized string for the complete duration), they would be padded
        # twice otherwise.
        if comment === 'short-time-part' || comment === 'long-time-part'
            return str
        end

        # The number of underscores for the string
        underscoreCount = underscoreCountFor(str)

        # Wrap the string returned by the block with underscores and a prefix
        # These additional elements are added to test how the UI renders the text.
        addUnderscoresAndPrefix(underscoreCount) do
            @inEntity = false

            # Substitute some chars (accent vowels, etc.) - so it's obvious when
            # text substituted into a localized string is not itself localized
            # (Like Auto (US Chicago), becomes [___Aútó (US Chicago)___], the city name
            # was not localized.)
            # Don't do anything inside an HTML entity though (very crudely parsed)
            #
            # Returns the string built up with .with_object("")
            str.each_char.with_object("") do |c, plStr|
                # update @inEntity state
                checkForHtmlEntity(c)

                # Need to use << here rather than += as << mutates the object
                # whereas += returns a new object
                # Do not perform substitutions if we're parsing an html entity.
                plStr << (inEntity? ? c : (SUBSTITIONS[c] || c))
            end
        end
    end

    def pseudolocalizeMessage(srcline, src, commentline, commentval, extracommentline, indent)
        srcline + commentline + extracommentline + indent + "<translation>" +
             pseudolocalize(src, commentval) + '</translation>'
    end

    public

    def pseudolocalizeString(tsSrc, langId)
        tsSrc.gsub!('en_US', langId)
        # Replace strings with pseudolocalized translations. This operates on the
        # OneSky-exported TS file, so use the translation field (the source has been
        # changed to a string key for OneSky).
        # These tags appear in the order:
        # - comment (disambiguation identifier, optional)
        # - extracomment (translator notes, optional)
        # - translation
        # If the string has a 'comment' tag, detect the comment name, a few strings
        # have to be special-cased
        tsSrc.gsub(TRANSLATION_REGEX) { pseudolocalizeMessage($1, $2, $3, $4, $5, $6) }
    end

    def pseudolocalizeToLang(exportFile, outTsPath, langId)
        tsSrc = File.read(exportFile, :encoding => 'UTF-8')
        tsOutput = pseudolocalizeString(tsSrc, langId)

        File.write(outTsPath, tsOutput)
    end
end

