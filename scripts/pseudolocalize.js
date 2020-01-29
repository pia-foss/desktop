#! /usr/bin/env node

// Copyright (c) 2020 Private Internet Access, Inc.
//
// This file is part of the Private Internet Access Desktop Client.
//
// The Private Internet Access Desktop Client is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// The Private Internet Access Desktop Client is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Private Internet Access Desktop Client.  If not, see
// <https://www.gnu.org/licenses/>.

const fs = require('fs')

function print_usage() {
  var args = process.argv.slice()
  args[0] = process.argv0
  console.log(
`usage: ${args.join(' ')} [../path/to/built/en_US.onesky.ts]
  Argument 1 is the path to the en_US.onesky.ts from the build output, such as
  ../build-pia_desktop-Desktop_Qt_5_11_0_clang_64bit-Debug/qtc_Desktop_Qt_5_11_0_clang_64bit_Debug/translations/en_US.onesky.ts

This script generates ./client/ts/ro.ts and ./client/ts/ps.ts as pseudolocalized resources
`)
}

// This table of character widths is used to expand the strings by 30%.
// To generate this, paste the following code somewhere and call measureAll,
// such as in the LoginPage's onEnter():
/*
  Text {
    id: measureText
  }

  function measureChars(start, end) {
    var startCode = start.charCodeAt(0)
    var endCode = end.charCodeAt(0)
    var measureCount = 10

    var printMetrics = ''

    for(var code=startCode; code<=endCode; ++code) {
      var measureChar = String.fromCharCode(code)
      var measureStr = measureChar
      while(measureStr.length < measureCount)
        measureStr = measureStr + measureChar

      measureText.text = measureStr
      var charWidth = measureText.advance.width / measureCount

      printMetrics += "'" + measureChar + "':" + charWidth.toFixed(1) + ", "
    }
    console.info(printMetrics);
  }

  function measureAll() {
    measureChars(' ', '/')
    measureChars('0', '9')
    measureChars(':', '@')
    measureChars('A', 'M')
    measureChars('N', 'Z')
    measureChars('[', '`')
    measureChars('a', 'm')
    measureChars('n', 'z')
    measureChars('{', '~')
  }
*/
const charWidths = {
  ' ':3.2, '!':3.4, '"':4.0, '#':8.1, '$':7.3, '%':9.5, '&':8.1, '\'':2.1, '(':4.3, ')':4.4, '*':5.6, '+':7.4, ',':2.6, '-':3.5, '.':3.5, '\/':5.4,
  '0':7.3, '1':7.3, '2':7.3, '3':7.3, '4':7.3, '5':7.3, '6':7.3, '7':7.3, '8':7.3, '9':7.3,
  ':':3.3, ';':3.3, '<':6.6, '=':7.3, '>':6.8, '?':6.2, '@':11.6,

  'A':8.4, 'B':8.3, 'C':8.2, 'D':8.6, 'E':7.6, 'F':7.6, 'G':8.9, 'H':9.3, 'I':3.7, 'J':7.2, 'K':8.4, 'L':7.0, 'M':11.4,
  'N':9.3, 'O':8.9, 'P':8.3, 'Q':8.9, 'R':8.6, 'S':8.1, 'T':7.7, 'U':8.8, 'V':8.2, 'W':11.5, 'X':8.2, 'Y':8.0, 'Z':7.8,
  '[':3.5, '\\':5.4, ']':3.5, '^':5.4, '_':5.9, '`':4.1,
  'a':7.1, 'b':7.4, 'c':6.9, 'd':7.4, 'e':6.9, 'f':4.5, 'g':7.4, 'h':7.4, 'i':3.3, 'j':3.4, 'k':6.7, 'l':3.3, 'm':11.4,
  'n':7.4, 'o':7.4, 'p':7.4, 'q':7.4, 'r':4.5, 's':6.8, 't':4.5, 'u':7.4, 'v':6.5, 'w':9.8, 'x':6.5, 'y':6.5, 'z':6.5,
  '{':4.4, '|':3.2, '}':4.4, '~':8.8
}

const substitutions = {
  'A': 'Á',
  'E': 'É',
  'I': 'Í',
  'O': 'Ó',
  'U': 'Ú',
  'a': 'á',
  'e': 'é',
  'i': 'í',
  'o': 'ó',
  'u': 'ú'
}

function pseudolocalize(str, comment) {
  // These time strings used for the connection duration aren't padded.
  // Due to the way these are constructed (localized strings for each time part,
  // then a localized string for the complete duration), they would be padded
  // twice otherwise.
  if(comment === 'short-time-part' || comment === 'long-time-part')
    return str

  // Estimate the width of the string
  let width=0
  for(let c of str) {
    width += charWidths[c] || charWidths['?']
  }

  // Divide by 2 for the number of underscores on each side
  let underscores = Math.round(width * 0.3 / charWidths['_'])

  // Use a few characters to verify rendering / clipping
  // - Dotted capital I as in turkish - verifies top isn't clipped
  // - Cyrillic shcha as in Russian - verifies descender isn't clipped
  // - 'A' kana - script support.
  // (Clipping doesn't look like it will really be an issue, few characters seem
  // to be taller than '[]' in this font anyway)
  //
  // The I is so narrow that we always include it.
  var plStr = '[&#x130;'
  // Don't excessively expand the string, count the shcha and kana as ~2
  // underscores each, they're really wide
  if(underscores >= 2) {
    plStr += '&#x429;'
    underscores -= 2
  }
  if(underscores >= 2) {
    plStr += '&#x30A2;'
    underscores -= 2
  }

  // Split up the underscores on left and right
  let leftUnderscores = Math.ceil(underscores/2)
  let rightUnderscores = Math.floor(underscores/2)

  plStr += '_'.repeat(leftUnderscores)

  // Substitute some chars (accent vowels, etc.) - so it's obvious when
  // text substituted into a localized string is not itself localized
  // (Like Auto (US Chicago), becomes [___Aútó (US Chicago)___], the city name
  // was not localized.)
  // Don't do anything inside an HTML entity though (very crudely parsed)
  let inEntity = false
  for(let c of str) {
    if(c == '&')
      inEntity = true
    else if(c == ';')
      inEntity = false

    plStr += inEntity ? c : (substitutions[c] || c)
  }

  plStr += '_'.repeat(rightUnderscores) + ']'

  return plStr
}

function pseudolocalizeToLang(langId) {
  // The repo dir is the parent of the scripts dir that this script is in
  const repoDir= __dirname + '/..'

  const outTsPath= repoDir + '/client/ts/' + langId + '.ts'

  let tsSrc = fs.readFileSync(process.argv[2], {encoding:'utf8'})

  // Change language
  tsSrc = tsSrc.replace('en_US', langId)
  // Replace strings with pseudolocalized translations.  This operates on the
  // OneSky-exported TS file, so use the translation field (the source has been
  // changed to a string key for OneSky).
  // These tags appear in the order:
  // - comment (disambiguation identifier, optional)
  // - extracomment (translator notes, optional)
  // - translation
  // If the string has a 'comment' tag, detect the comment name, a few strings
  // have to be special-cased
  tsSrc = tsSrc.replace(/( *<comment>([^<]*)<\/comment>\n|)( *<extracomment>[^<]*<\/extracomment>\n|)( *)<translation>([^<]*)<\/translation>/g,
    function(match, commentline, commentval, extracommentline, indent, src) {
      return commentline + extracommentline + indent + '<translation>' + pseudolocalize(src, commentval) + '</translation>'
    })

  fs.writeFileSync(outTsPath, tsSrc)
}

if(process.argv.length < 3) {
  print_usage()
}
else {
  // We have to use an actual language code, not a language with a user-defined
  // country code like en_ZZ, because QLocale ignores user-defined codes.
  // 'ro' (Romanian) is used just because it's a language that Windows supports
  // that we don't normally support ourselves.  (The Windows installer follows
  // the Windows UI language, so we have to use a language supported by
  // Windows.)
  pseudolocalizeToLang('ro')
  // The RTL pseudolocalization uses Pashto, which is an RTL language supported
  // by Windows.  (Also 'ps' ~= "pseudolocalized", sort of.)
  // There's no other difference in the translation; the key is that the client
  // and Windows installer apply RTL mirroring for this language.
  pseudolocalizeToLang('ps')
}
