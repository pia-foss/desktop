#! /usr/bin/env node

// Copyright (c) 2022 Private Internet Access, Inc.
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

/*
This script is used to generate localized STRINGTABLE resource scripts for the
Windows installer from the translated Qt localization files.

Once translations have been included in a localization file under client/ts/,
use this tool to generate a new <lang>.rc under extras/installer/win/strings/.

These localized resource scripts currently have to be generated manually and
checked in.  The alternative would be to require node.js and xml2js (and its
dependencies, and so on) during the build process, which we'd prefer to avoid.
These strings shouldn't change much, so manual generation shouldn't be too
troublesome.
*/

const fs = require('fs')
const xml2js = require('xml2js')
const PiaOneSky = require('../pia_onesky.js')

// xml2js's parseString, but returned as a promise
function parseXml(xml) {
  return new Promise((resolve, reject) => {
    xml2js.parseString(xml, (err, result) => {
      if(err)
        reject(err)
      else
        resolve(result)
    })
  })
}

// Get a context from the parsed TS file
function getTsContext(ts, context) {
  return ts && ts.TS.context.find((ctxObj) => ctxObj.name[0] === context)
}

// Get a message object from the context object
function getCtxMessage(ctxObj, srcText) {
  return ctxObj && ctxObj.message.find((msgObj) => msgObj.source[0] === srcText)
}

// Get a message in a context from the TS file
function getTsMessage(ts, context, srcText) {
  const ctxObj = getTsContext(ts, context)
  return getCtxMessage(ctxObj, srcText)
}

// Test if a line is a character set line, and if so, change the character set.
// Returns the new line, or an empty string if it wasn't a character set line.
function changeCharsetLine(line, charset) {
  const match = line.match(/^( *)ANSI_CHARSET$/)
  if(!match)
    return

  return match[1] + charset
}

// Test if a line is the mirror flag line, and if so, change the value.
// 'mirror' is a boolean value indicating whether the UI is mirrored in this
// language.
function changeUiMirrorLine(line, mirror) {
  if(line.endsWith('ui_mirror_localize'))
    return '    ' + (mirror ? '1' : '0') + ' // ui_mirror_localize'
}

// Test if a line is a LANGUAGE line, and if so, change the language.
// Returns the new line, or an empty string if it wasn't a LANGUAGE line.
function changeLangLine(line, lang, sublang) {
  const match = line.match(/^( *LANGUAGE +)LANG_[A-Z_]+, +SUBLANG_[A-Z_]+$/)
  if(!match)
    return

  return match[1] + lang + ', ' + sublang
}

// Test if a line is a translatable string line, and if so, localize the string.
// Returns the new line, or an empty string if it wasn't a translatable string
// line.
// If it's a translatable line, but the string given hasn't been translated
// (it's not found in 'ts' or it is marked unfinished), leaves the English text
// and prints a diagnostic.
function translateStringLine(line, ts) {
  const match = line.match(/^([A-Z_][A-Z0-9_]* +)QT_TRANSLATE_NOOP\("([^"]*)", *\"([^"]*)\"\)$/)
  if(!match)
    return

  const idPrefix = match[1]
  const context = match[2]
  const sourceText = match[3]

  const msg = getTsMessage(ts, context, sourceText)

  // Finished translations with no attributes become plain strings
  let translated = sourceText
  if(msg && typeof msg.translation[0] === 'string') {
    translated = msg.translation[0]

    // None of the source strings contain quotation marks, but a few of the
    // translations do.  Despite the resemblance to C string literals, rc.exe
    // actually escapes quotation marks as double quotation marks.
    translated = translated.replace(/"/g, "\"\"")
  }
  else {
    // If it's not a plain string, it could be undefined, an object with
    // unfinished=false, etc.  Warn and don't translate in those cases.
    // Look up the string in the ts content.
    console.warn(`No translation for "${sourceText}" in context "${context}"`)
  }

  return `${idPrefix}"${translated}"`
}

// Generate a localized resource script.
// Uses the en_US.rc script content as a template, replaces QT_TRANSLATE_NOOP()
// with localized
//
// Params:
// 'enrc' - Content of the en_US.rc resource script
// 'tspath' - Path to the localized .ts file used to generate the localization
// 'lang' - Windows language ID for the new resource (LANG_ENGLISH, etc.)
//     (see https://docs.microsoft.com/en-us/windows/desktop/menurc/language-statement)
// 'sublang' - Windows sublanguage ID for the new resource
//
// Returns a promise that will resolve with the localized resource script
// content
function localize(enrc, tspath, lang, sublang, charset, mirror) {
  // Load the TS file and parse it.
  const tsContent = fs.readFileSync(tspath, {encoding: 'utf8'})
  const tsContentImported = PiaOneSky.tsImport(tsContent)
  // Import the TS content to undo OneSky transformations
  return parseXml(tsContentImported)
    .then(ts => {
      // Split the template RC into lines, but keep the line breaks so we can
      // preserve LF/CRLF.
      const rcLines = enrc.split(/(\r\n|\n)/);

      var rcResult = []
      for(let i=0; i+1<rcLines.length; i += 2) {
        const rcLine = rcLines[i]
        const linebreak = rcLines[i+1]

        let updatedLine = changeCharsetLine(rcLine, charset) ||
                          changeUiMirrorLine(rcLine, mirror) ||
                          changeLangLine(rcLine, lang, sublang) ||
                          translateStringLine(rcLine, ts) ||
                          rcLine
        updatedLine += linebreak
        rcResult.push(updatedLine)
      }

      return rcResult.join('')
    })
}

if(process.argv.length < 6 || process.argv.length > 8) {
  console.log(`usage: ${process.argv0} ${process.argv[1]} [lang_file_base] [win_lang_id] [win_sublang_id] [win_charset] [--mirror] [--debug]`)
  console.log(` - lang_file_base - name of .ts file in <pia_desktop>/client/ts/`)
  console.log(` - win_lang_id / win_sublang_id - Language code (https://docs.microsoft.com/en-us/windows/desktop/Intl/language-identifier-constants-and-strings)`)
  console.log(` - win_charset - Character set for this language: (https://msdn.microsoft.com/en-us/library/cc194829.aspx)`)
  console.log(` - --mirror - If present, mirrors the UI for this language`)
  console.log(` - --debug - If present, places the generated file in translations/debug (for pseudotranslations)`)
  console.log(``)
  console.log(`Generates <pia_desktop>/extras/installer/win/translations/<lang_file_base>.rc`)
  console.log(``)
  console.log(`lang_file_base does not need to match the language IDs - for`)
  console.log(`example, pseudolocalized resources can be tested with:`)
  console.log(`  ./scripts/pseudolocalize.js <path/to/build/en_US.ts>`)
  console.log(`  ./scripts/winstaller_localize.js ro LANG_ROMANIAN SUBLANG_ROMANIAN_ROMANIA SHIFTJIS_CHARSET --debug`)
  console.log(`Then, rebuild the installer, set your OS locale to ro and test`)
}
else {
  // The repo dir is the parent of the scripts dir that this script is in
  const repoDir = __dirname + '/..'

  const langFileBase = process.argv[2]
  const winLangId = process.argv[3]
  const winSublangId = process.argv[4]
  const winCharsetId = process.argv[5]

  // If argv[6] is present, we already know it's --mirror, checked in validation
  // above
  var winUiMirror = false
  var debug = false
  for(var i=6; i<process.argv.length; ++i) {
    switch(process.argv[i])
    {
      case '--mirror':
        winUiMirror = true
        break
      case '--debug':
        debug = true
        break
      default:
        console.log('Unknown option: ' + process.argv[i])
        process.exit(1)
        break
    }
  }

  // Path to the en_US.rc template
  const enUSpath = repoDir + '/extras/installer/win/strings.rc'
  // Path to the translations directory - create it if needed
  const translationsDir = repoDir + '/extras/installer/win/translations' + (debug ? '/debug' : '')
  // Path to the resource to generate
  const outRcPath = translationsDir + `/${langFileBase}.rc`
  // Path to the localized TS file
  const tsPath = repoDir + `/client/ts/${langFileBase}.ts`

  if(!fs.existsSync(translationsDir)) {
    console.log(`creating ${translationsDir}`)
    fs.mkdirSync(translationsDir)
  }

  console.log(`generating ${outRcPath} from ${tsPath}`)
  console.log(`using language ${winLangId}-${winSublangId} and charset ${winCharsetId}, mirror ${winUiMirror}`)

  const enUSrc = fs.readFileSync(enUSpath, {encoding: 'utf8'})

  localize(enUSrc, tsPath, winLangId, winSublangId, winCharsetId, winUiMirror)
    .then((outRcContent) => {
      // The file has to be written out in UTF-16LE, this is the only Unicode
      // format that seems to be supported by the resource compiler.
      fs.writeFileSync(outRcPath, outRcContent, {encoding: 'utf16le'})
    })
}
