# Import/export TS files for use in OneSky.  Adjusts string identifiers,
# comments, etc. to improve behavior in OneSky - see tsExport() / tsImport().
#
# This whole file was translated from QBS-style JS, so it could stand to have
# some refactoring into better Ruby.
module PiaOneSky
    def self.grabLineBreak(str)
        endlMatch = str.match(/(\r?\n)/)
        endlMatch != nil ? endlMatch[1] : '\n'
    end

    # Given a series of XML tags (with no attributes, like the <context> and
    # <message> tags), split them up into an array of strings containing just
    # the contents of each tag
    def self.splitTagSeries(xmlTags, tagName)
        delim = Regexp.new('[\r\n]* *</?' + tagName + '> *[\r\n]*')
        xmlTags.split(delim).select {|m| m != ''}
    end

    # Get the source string from a message
    def self.getMessageSource(message)
        message.match(/<source>(.*)<\/source>/m)[1]
    end

    ## Context transformations ##

    # Identity context transformation; we don't actually need to transform the
    # context any more since disambiguation is now in the string key
    def self.contextIdentity(ctxName, message)
        ctxName
    end

    ## Source transformations ##

    # These delimiters are assumed to never occur in a context or
    # disambiguation, but they can occur in the message itself.
    # The delimiters are different because we might not have the disambiguation
    # delimiter at all if there is no disambiguation comment.
    SourceMessageDelim = ' --- '
    SourceDisambiguationDelim = ' -- '

    # Build a unique string key out of a context and message
    def self.buildStringKey(contextName, message)
        stringKey = String.new(contextName)

        commentMatch = message.match(/<comment>(.*)<\/comment>/m)
        if(commentMatch != nil)
            stringKey << SourceDisambiguationDelim
            stringKey << commentMatch[1]
        end

        stringKey << SourceMessageDelim
        stringKey << getMessageSource(message)
    end

    # Extract the original source from a string key
    def self.getStringKeySource(contextName, message)
        source = getMessageSource(message)

        delimPos = source.index(SourceMessageDelim)
        if(delimPos != nil)
            source[(delimPos+SourceMessageDelim.length)..-1]
        else
            source
        end
    end

    ## Comment transformations ##

    # Keep the original comment (for export)
    def self.commentIdentity(contextName, message)
        commentMatch = message.match(/<comment>(.*)<\/comment>/m)
        if(commentMatch != nil)
            commentMatch[1]
        end
        nil # The message doesn't have a comment
    end

    # Extract the comment from the exported string key
    def self.commentFromStringKey(contextName, message)
        source = getMessageSource(message)
        startIdx = source.index(SourceDisambiguationDelim)
        endIdx = source.index(SourceMessageDelim)
        if(startIdx == nil || endIdx == nil)
            nil # Doesn't have a comment
        else
            source[(startIdx + SourceDisambiguationDelim.length)..endIdx-1]
        end
    end

    ## Translation transformations ##

    # Set the translation to the source (for export)
    def self.translationFromSource(contextName, message)
        "<translation>#{getMessageSource(message)}</translation>"
    end

    # Return the translation as-is (for import)
    def self.translationIdentity(contextName, message)
        message.match(/<translation.*<\/translation>/m)[0]
    end

    # Transform the content of a message with source/translation transformations
    def self.transformMsg(contextName, message, msgSourceFunc, msgCommentFunc,
                          msgTranslationFunc, endl)
        newSource = msgSourceFunc.call(contextName, message)
        newComment = msgCommentFunc.call(contextName, message)
        newTranslationTag = msgTranslationFunc.call(contextName, message)
        # Don't modify caller's message
        message = String.new(message)

        # Replace the source value
        message.gsub!(/<source.*<\/source>/m, "<source>#{newSource}</source>")

        # Update the comment (replace an existing tag, or add <comment> if it's
        # not there)
        # Remove the existing comment tag, if there is one
        message.gsub!(/<comment.*<\/comment>/m, '')
        if(newComment != nil)
            # Append the new comment tag
            message << endl
            message << '        <comment>'
            message << newComment
            message << '</comment>'
        end

        # Replace the translation tag
        message.gsub!(/<translation.*<\/translation>/m, newTranslationTag)
    end

    # Given the content of a context (everything between <context>...</context>,
    # the <name> and all <message> tags), regroup its messages into new contexts
    # determined by msgContextFunc.
    #
    # (Used to disambiguate messages with disambiguateMsgContext or to strip
    # context disambiguations with stripCtxDisambiguation.)
    #
    # Parameters:
    # - contextContent - XML content of a context tag (not including the <context>
    #   and </context> itself)
    # - msgContextFunc - Function to determine the new context name for a message;
    #   called with the original context name and message XML; returns the new
    #   context name.
    # - msgSourceFunc - Function to determine the new "source" value for a message;
    #   called with the original context name and message XML; returns the new
    #   "source" string (just the value).
    # - msgCommentFunc - Function to determine the new "comment" value for a
    #   message; called with the original context name and message XML; returns the
    #   new "comment" string (just the value).
    # - msgTranslationFunc - Function to determien the new <transation> tag for a
    #   message; called with the original context name and message XML; returns the
    #   new <translation> tag (the entire tag from <translation> to </translation>)
    # - newContextMap - map of new context names to arrays of messages (keys are
    #   strings, values are arrays of strings containing the <message>..</message>
    #   XML)
    # - endl - line break ('\r\n' or '\n')
    #
    # Adds the messages from contextContent to newContextMap under their new
    # contexts.  (Creates contexts that haven't been observed before.)
    def self.recontextMessages(contextContent, msgContextFunc, msgSourceFunc,
                               msgCommentFunc, msgTranslationFunc,
                               newContextMap, endl)
        # Extract the name
        name = contextContent.match(/<name>(.*)<\/name>/m)[1]

        # Look for messages that have disambiguation comments, and split them
        # out to new contexts
        allMessageXml = contextContent.match(/(<message>.*<\/message>)/m)[1]
        allMessages = splitTagSeries(allMessageXml, 'message')

        # Keep track of how many contexts we contribute to from each original
        # context just for diagnostics
        contributedContexts = {}
        allMessages.each do |m|
            newCtxName = msgContextFunc.call(name, m)

            # Add to that context's messages
            if(newContextMap[newCtxName] == nil)
                newContextMap[newCtxName] = []
            end

            transformedMsg = transformMsg(name, m, msgSourceFunc,
                                          msgCommentFunc, msgTranslationFunc,
                                          endl)
            newContextMap[newCtxName] << transformedMsg
            contributedContexts[newCtxName] = true
        end

        if(contributedContexts.length > 1)
            puts "Context #{name} exported to #{contributedContexts.length} disambiguated contexts"
        end
    end

    # Rebuilds the context XML from the new context map.
    # Returns the new XML content.
    def self.rebuildContextXml(newContextMap, endl)
        rebuiltXml = ''
        newContextMap.each do |context, messages|
            # Write out a context
            rebuiltXml << '<context>' << endl
            rebuiltXml << '    <name>' << context << '</name>' << endl
            # Write the messages
            messages.each do |m|
                rebuiltXml << '    <message>' << endl
                rebuiltXml << m << endl
                rebuiltXml << '    </message>' << endl
            end
            rebuiltXml << '</context>' << endl
        end
        rebuiltXml
    end

    # Regroup message contexts in a TS file according to msgContextFunc.
    # - tsContent - content of the .ts file
    # - msgContextFunc, msgSourceFunc, msgTranslationFunc - Functions to transform
    #   the context/source/translation for a message, see recontextMessages().
    #
    # Returns the new content to be written to the exported file.
    #
    # This is implemented with regexes due to having been translated from
    # QBS-style JS.  In particular, this assumes that the <context> and
    # <message> tags never have any attributes.  (This appears to be correct but
    # it'd cause breakage if the .ts format changes in a future version of Qt.)
    def self.recontextTsContent(tsContent, msgContextFunc, msgSourceFunc,
                                msgCommentFunc, msgTranslationFunc)
        # Capture a line break so we don't create mixed line endings
        endl = grabLineBreak(tsContent)

        # Split up the entire document into a "header", "footer", and all the
        # "context" content
        match = tsContent.match(/^(.*<TS[^>]*>)\r?\n(<context>.*<\/context>)\r?\n(.*)$/m)
        header = match[1]
        allContextXml = match[2]
        footer = match[3]

        # Separate all contexts
        allContexts = splitTagSeries(allContextXml, 'context')

        # Regroup all messages with the new contexts
        newContextMap = {}
        allContexts.each do |c|
            recontextMessages(c, msgContextFunc, msgSourceFunc, msgCommentFunc,
                              msgTranslationFunc, newContextMap, endl)
        end

        # Rebuild the XML with the new contexts
        # This takes advantage of the fact that Hash objects iterate keys in the
        # order they were added, so the rebuilt XML is similar to the original
        # (contexts appear in the order first observed, and messages within
        # those contexts appear in their original order)
        rebuiltTsXml = header + endl
        rebuiltTsXml << rebuildContextXml(newContextMap, endl)
        rebuiltTsXml << footer
    end

    def self.tsExport(tsContent)
        recontextTsContent(tsContent, method(:contextIdentity),
                           method(:buildStringKey), method(:commentIdentity),
                           method(:translationFromSource))
    end

    # Import TS content previously exported for OneSky.
    # - Undoes the source->string key transformation from tsExport().
    # - Restores the disambiguation comment from the string key (OneSky does not
    #   reliably preserve this)
    # - Leaves the translation as-is, this should be a translated message at
    #   this point
    def self.tsImport(tsContent)
        recontextTsContent(tsContent, method(:contextIdentity),
                           method(:getStringKeySource),
                           method(:commentFromStringKey),
                           method(:translationIdentity))
    end
end
