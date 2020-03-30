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

// pia_onesky.js - Utilities to import/export TS translation files to/from
// OneSky
// See tsExport() / tsImport() at the bottom of this file.

// Get a line break string from file content (to avoid mixing line endings on
// Windows)
function grabLineBreak(str) {
  var endlMatch = str.match(/(\r?\n)/)
  return endlMatch ? endlMatch[1] : '\n'
}

// Given a series of XML tags (with no attributes, like the <context> and
// <message> tags), split them up into an array of strings containing just the
// contents of each tag.
function splitTagSeries(xmlTags, tagName) {
  var delim = new RegExp('[\r\n]* *<\/?' + tagName + '> *[\r\n]*')
  // Remove empties from the result, these occur between close/open tags
  return xmlTags.split(delim).filter(function(content){return content})
}

// Break the build if some expected conditions do not hold.
function assert(condition, message) {
  if(!condition) {
    console.error('Assertion failed: ' + message)
    throw new Error('Assertion failed: ' + message)
  }
}

// Get the source string from a message
function getMessageSource(message) {
  var sourceMatch = message.match(/<source>([^]*)<\/source>/)
  assert(sourceMatch, 'message missing source: ' + message)
  return sourceMatch[1]
}

/*** Context transformations ***/

// Identity context transformation; we don't actually need to transform context
// any more since disambiguation is now in the string key.
function contextIdentity(ctxName, message) {
  return ctxName
}

/*** Source transformations ***/

// These delimiters are assumed to never occur in a context or disambiguation,
// but they can occur in the message itself.
// The delimiters are different because we might not have the disambiguation
// delimiter at all if there is no disambiguation comment.
var sourceMessageDelim = ' --- '
var sourceDisambiguationDelim = ' -- '

// Build a unique string key out of a context and message
function buildStringKey(contextName, message) {
  var stringKey = contextName

  var commentMatch = message.match(/<comment>([^]*)<\/comment>/)
  // If there's a comment, add it to the context name
  if(commentMatch) {
    stringKey += sourceDisambiguationDelim
    stringKey += commentMatch[1]
  }

  stringKey += sourceMessageDelim
  stringKey += getMessageSource(message)

  return stringKey
}

// Extract the original source from a string key
function getStringKeySource(contextName, message) {
  var source = getMessageSource(message)

  var delimPos = source.indexOf(sourceMessageDelim)
  if(delimPos < 0)
    return source

  return source.substring(delimPos + sourceMessageDelim.length)
}

/*** Comment transformations ***/

// Keep the original comment (for export)
function commentIdentity(contextName, message) {
  var commentMatch = message.match(/<comment>([^]*)<\/comment>/)
  if(commentMatch)
    return commentMatch[1]
  return  // The message doesn't have a comment
}

// Extract the comment from the exported string key
function commentFromStringKey(contextName, message) {
  var source = getMessageSource(message)
  var startIdx = source.indexOf(sourceDisambiguationDelim)
  var endIdx = source.indexOf(sourceMessageDelim)
  if(startIdx < 0 || endIdx < 0)
    return  // Doesn't have a comment
  startIdx += sourceDisambiguationDelim.length
  return source.substring(startIdx, endIdx)
}

/*** Translation transformations ***/

// Set the translation to the source (for export)
function translationFromSource(contextName, message) {
  return '<translation>' + getMessageSource(message) + '</translation>'
}

// Return the translation as-is (for import)
function translationIdentity(contextName, message) {
  var translationMatch = message.match(/<translation[^]*<\/translation>/)
  assert(translationMatch, 'message missing translation: ' + message)
  return translationMatch[0]
}

// Transform the content of a message with source/translation transformations.
function transformMsg(contextName, message, msgSourceFunc, msgCommentFunc,
                      msgTranslationFunc, endl) {
  var newSource = msgSourceFunc(contextName, message)
  var newComment = msgCommentFunc(contextName, message)
  var newTranslationTag = msgTranslationFunc(contextName, message)

  // Replace the source value
  message = message.replace(/<source.*<\/source>/, '<source>' + newSource + '</source>')

  // Update the comment (replace an existing tag, or add <comment> if it's not
  // there)
  // Remove the existing comment tag, if there is one
  message = message.replace(/<comment.*<\/comment>/, '')
  if(newComment) {
    // Append the new comment tag
    message += endl
    message += '        <comment>'
    message += newComment
    message += '</comment>'
  }

  // Replace the translation tag
  message = message.replace(/<translation.*<\/translation>/, newTranslationTag)

  return message
}

// Given the content of a context (everything between <context>...</context>,
// the <name> and all <message> tags), regroup its messages into new contexts
// determined by msgContextFunc.
//
// (Used to disambiguate messages with disambiguateMsgContext or to strip
// context disambiguations with stripCtxDisambiguation.)
//
// Parameters:
// - contextContent - XML content of a context tag (not including the <context>
//   and </context> itself)
// - msgContextFunc - Function to determine the new context name for a message;
//   called with the original context name and message XML; returns the new
//   context name.
// - msgSourceFunc - Function to determine the new "source" value for a message;
//   called with the original context name and message XML; returns the new
//   "source" string (just the value).
// - msgCommentFunc - Function to determine the new "comment" value for a
//   message; called with the original context name and message XML; returns the
//   new "comment" string (just the value).
// - msgTranslationFunc - Function to determien the new <transation> tag for a
//   message; called with the original context name and message XML; returns the
//   new <translation> tag (the entire tag from <translation> to </translation>)
// - newContextMap - map of new context names to arrays of messages (keys are
//   strings, values are arrays of strings containing the <message>..</message>
//   XML)
// - endl - line break ('\r\n' or '\n')
//
// Adds the messages from contextContent to newContextMap under their new
// contexts.  (Creates contexts that haven't been observed before.)
//
// Returns XML containing one or more <context> tags.
function recontextMessages(contextContent, msgContextFunc, msgSourceFunc,
                           msgCommentFunc, msgTranslationFunc, newContextMap,
                           endl) {
  // Extract the name
  var nameMatch = contextContent.match(/<name>(.*)<\/name>/)
  if(!nameMatch) {
    console.error('Could not extract name of context: ' + contextContent)
    return contextContent
  }

  var name = nameMatch[1]

  // Look for messages that have disambiguation comments, and split them out to
  // new contexts.
  var allMessageXml = contextContent.match(/(<message>[^]*<\/message>)/)[1]
  var allMessages = splitTagSeries(allMessageXml, 'message')

  var i, commentMatch, comment, newCtxName
  // Keep track of how many contexts we contribute to from each original context
  // just for diagnostics
  var contributedContexts = {}
  for(i=0; i<allMessages.length; ++i) {
    newCtxName = msgContextFunc(name, allMessages[i])

    // Add to that context's messages
    if(!newContextMap[newCtxName])
      newContextMap[newCtxName] = []

    var transformedMsg = transformMsg(name, allMessages[i], msgSourceFunc,
                                      msgCommentFunc, msgTranslationFunc, endl)
    newContextMap[newCtxName].push(transformedMsg)
    contributedContexts[newCtxName] = true
  }

  var contributedCtxCount = Object.keys(contributedContexts).length
  if(contributedCtxCount > 1)
    console.info('context ' + name + ' exported to ' + contributedCtxCount + ' disambiguated contexts')
}

// Rebuild the context XML from the new context map.
// Returns the new XML content.
function rebuildContextXml(newContextMap, endl) {
  var rebuiltXml = ''
  for(var newCtxName in newContextMap) {
    // Write out a context
    rebuiltXml += '<context>' + endl
    rebuiltXml += '    <name>' + newCtxName + '</name>' + endl
    for(i=0; i<newContextMap[newCtxName].length; ++i) {
      rebuiltXml += '    <message>' + endl
      rebuiltXml += newContextMap[newCtxName][i] + endl
      rebuiltXml += '    </message>' + endl
    }
    rebuiltXml += '</context>' + endl
  }

  return rebuiltXml
}

// Regroup message contexts in a TS file according to msgContextFunc.
//
// Params:
// - tsContent - content of the .ts file
// - msgContextFunc, msgSourceFunc, msgTranslationFunc - Functions to transform
//   the context/source/translation for a message, see recontextMessages().
//
// Returns the new content to be written to the exported file.
//
// Note that this is implemented with regexes since there's not a full XML
// parser readily available in QBS.  In particular, this assumes that the
// <context> and <message> tags never have any attributes.  (This appears to be
// correct but it'd cause breakage if the .ts format changes in a future version
// of Qt.)
function recontextTsContent(tsContent, msgContextFunc, msgSourceFunc,
                            msgCommentFunc, msgTranslationFunc) {
  // Capture a line break so we don't create mixed line endings
  var endl = grabLineBreak(tsContent)

  // Split up the entire document into a "header", "footer", and all the
  // "context" content.
  var match = tsContent.match(/^([^]*<TS[^>]*>)\r?\n(<context>[^]*<\/context>)\r?\n([^]*)$/)
  if(!match) {
    console.error('Could not export translations for OneSky')
    return tsContent
  }

  var header = match[1]
  var allContextXml = match[2]
  var footer = match[3]

  // Separate all contexts
  var allContexts = splitTagSeries(allContextXml, 'context')

  // Regroup all the messages with new contexts
  var newContextMap = {}
  for(var i=0; i<allContexts.length; ++i) {
    recontextMessages(allContexts[i], msgContextFunc, msgSourceFunc,
                      msgCommentFunc, msgTranslationFunc, newContextMap, endl)
  }

  // Rebuild the XML with the new contexts.
  // This takes advantage of the fact that JS objects iterate properties in the
  // order they were added, so the rebuilt XML is similar to the original
  // (contexts appear in the order first observed, and messages within those
  // appear in their original order).
  var rebuiltTsXml = header + endl
  rebuiltTsXml += rebuildContextXml(newContextMap, endl)
  rebuiltTsXml += footer
  return rebuiltTsXml
}

// Export TS content for OneSky.  Does the following:
// - Embeds context and disambiguation strings into the message "source" so the
//   String Key on OneSky will include them.  (OneSky does not support
//   disambiguation, and its screenshot-tag view only displays String Keys, no
//   context.  It used to at least round-trip the disambiguation 'comment', but
//   it no longer seems to do so for new phrases.)
//   The sources are written as `<context> -- <disambiguation> --- <en_US msg>`,
//   which is relatively human-readable while also allowing us to split up the
//   string again for import.
// - Sets "unfinished" <translation> tags to the original message source, so the
//   en_US message is still the original text on OneSKy.
function tsExport(tsContent) {
  return recontextTsContent(tsContent, contextIdentity, buildStringKey,
                            commentIdentity, translationFromSource)
}

// Import TS content previously exported for OneSky.
// - Undoes the source->string key transformation from tsExport().
// - Restores the disambiguation comment from the string key (OneSky does not
//   reliably preserve this)
// - Leaves the translation as-is; this should be a translated message at this
//   point.
function tsImport(tsContent) {
  return recontextTsContent(tsContent, contextIdentity, getStringKeySource,
                            commentFromStringKey, translationIdentity)
}

// Used in node.js for legacy_ts_update.js - export when loaded in node
if(typeof module !== 'undefined') {
  module.exports.tsExport = tsExport
  module.exports.tsImport = tsImport
}
