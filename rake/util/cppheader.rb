class LineContent
    def initialize()
        @content = ''
    end

    def line(code)
        @content << code + "\n"
    end

    def content()
        @content
    end
end

# Generate the content for a C++ header.  Used to generate version.h/brand.h,
# primarily useful for defining macros containing build information.
class CppHeader
    # Create CppHeader with the name of the header (excluding ".h"), such as
    # 'version' or 'brand'
    def initialize(name)
        guard = "#{name.upcase}_H"
        @content = LineContent.new
        @content.line '#ifndef ' + guard
        @content.line '#define ' + guard
        @content.line ''
    end

    # Add an arbitrary line of code.
    def line(code)
        @content.line code
    end

    # Define a macro with a string value.  The value given becomes the content
    # of a string literal.
    #
    # This quotes the string with String.undump().  For very long or complex
    # strings that don't need to be used in resource scripts, defineRawString()
    # can write the string as a raw string literal.
    def defineString(macro, value)
        @content.line '#define ' + macro + ' ' + value.dump
    end

    # Define a macro with a string value, using a raw string literal to
    # represent the value.  This avoids any possible corner cases for excaping,
    # so it's best for long or complex strings.  However, rc.exe does not
    # support raw string literals, so this can't be used for values used in
    # Windows resource scripts.
    def defineRawString(macro, value)
        @content.line '#define ' + macro + ' R"(' + value + ')"'
    end

    # Define a macro with a literal value - the value given is written verbatim
    # into the header.
    def defineLiteral(macro, value)
        @content.line '#define ' + macro + ' ' + value
    end

    # Get the final content of the header file
    def content
        # Add the final #endif to close the include guard
        @content.content + '#endif' + "\n"
    end
end
