// Copyright (c) 2026 Private Internet Access, Inc.
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

#include "extras/openvpn/mac/scutil_parser.h"
#include <QJsonArray>
#include <QJsonObject>
#include <unordered_map>
#include <regex>
#include <stdexcept>

// This file is responsible for parsing scutil object syntax.
// An example scutil object is the following:
//
// <dictionary> {
//   ARPResolvedHardwareAddress : 30:5a:3a:6d:a1:e0
//   ARPResolvedIPAddress : 192.168.1.1
//   AdditionalRoutes : <array> {
//     0 : <dictionary> {
//       DestinationAddress : 192.168.1.41
//       SubnetMask : 255.255.255.255
//     }
//     1 : <dictionary> {
//       DestinationAddress : 169.254.0.0
//       SubnetMask : 255.255.0.0
//     }
//   }
//   Addresses : <array> {
//     0 : 192.168.1.41
//   }
//   ConfirmedInterfaceName : en0
//   InterfaceName : en0
//   NetworkSignature : IPv4.Router=192.168.1.1;IPv4.RouterHardwareAddress=30:5a:3a:6d:a1:e0
//   Router : 192.168.1.1
//   SubnetMasks : <array> {
//     0 : 255.255.255.0
//   }
// }
//
// The grammar we derived to parse it is:
//
// container : dictionary | array
// dictionary: DictionaryOpen NewLine dict_items BlockClose
// dict_items: (dict_item)* // 0 or more
// dict_item:  dict_key Colon value NewLine
// value: Identifier | container
// array: ArrayOpen NewLine array_items BlockClose
// array_items: (array_item)*
// array_item: array_index Colon value NewLine
// dict_key: ('aA'..'Zz')+
// array_index: (0..9)+
//
// We use a hand-written LL(1) parser to parse it and output a QJsonValue result that represents the
// parsed scutil object. The object will either be a QJsonObject, QJsonArray, QJsonValue::Null
// or JsonValue::Undefined
namespace {

// Scutil object syntax tokens
enum class TokenType
{
    DictionaryOpen,  // "<dictionary> {"
    ArrayOpen,       // "<array> {"
    BlockClose,      // "}"
    Colon,           // ":"
    Identifier,      // Key or Item names
    NewLine,
    NoKeyError,      // Represents a failure by scutil to lookup a key, see Lexer::NoKeyErrorText
    EndOfInput
};

// Represents a token
// - type - the token type (see above)
// - text - the text that generated the token match, aka the lexeme
//   for example, DictionaryOpen is the token but "<dictionary> {" is the lexeme
struct Token
{
    TokenType type{};
    QString text;
};

// Associate the name of token with the token type.
// This is slightly different to Token::text which shows the
// lexeme (i.e string which matched the token) rather than the token
// name itself.
std::unordered_map<TokenType, QString> tokenMap {
    {TokenType::DictionaryOpen, "DictionaryOpen"},
    {TokenType::ArrayOpen, "ArrayOpen"},
    {TokenType::BlockClose, "BlockClose"},
    {TokenType::Colon, "Colon"},
    {TokenType::Identifier, "Identifier"},
    {TokenType::NewLine, "NewLine"},
    {TokenType::NoKeyError, "NoKeyError"},
    {TokenType::EndOfInput, "EndOfInput"},
};

// Custom error that represents a parsing/lexing failure.
class ParseError : public std::runtime_error
{
public:
    explicit ParseError(const QString &reason)
    : std::runtime_error(reason.toStdString())
    {}
};

// The Lexer is reponsible for breaking up a string of text into tokens. These
// tokens represent units in the scutil object grammar, such as the start of a dictionary
// definition or the start of an array definition; as well as their keys/values.
class Lexer
{
    // Lexemes used to match tokens
    inline static const QString DictionaryOpenText = "<dictionary> {";
    inline static const QString ArrayOpenText = "<array> {";
    inline static const QString BlockCloseText = "}";
    inline static const QString ColonText = " : ";
    inline static const QString NewLineText = "\n";
    inline static const QString NoKeyErrorText = "  No such key\n";

public:
    explicit Lexer(const QString &input)
    : _input{input}
    , _currentIndex{0}
    {
        // Lexable content must either start with a container "<" or
        // have the key error content
        if(!_input.startsWith("<") && !_input.startsWith(NoKeyErrorText))
        {
            throw ParseError("Unlexable content, got input: " + _input);
        }
    }

    // Return the next available token from the input
    Token nextToken();

private:
    QString _input;
    int _currentIndex{};
};

// A Parser for the scutil object grammar.
// This parser reads from the Lexer token stream and validates whether a scutil dictionary
// or array is present. If one of these is present it returns the QJsonValue representation of that object
// i.e a QJsonObject (for a dictionary) or a QJsonArray (for an array).
// Since it's designed to read one scutil object output from a scutil command it stops reading
// after it finds a valid object.
class Parser
{
public:
    explicit Parser(Lexer &lexer)
    : _lexer{lexer}
    {}

public:
    // Parse the token stream
    QJsonValue parse();
    // Parse the token stream with verbose tracing
    QJsonValue parseTrace()
    {
        _shouldTrace = true;
        return parse();
    }

private:
    // Dictionary helpers
    QJsonValue dictionary();
    void dictionaryElements(QJsonObject &dict);
    void dictionaryElement(QJsonObject &dict);
    QString dictionaryKey();
    QJsonValue containerValue();

    // Array helpers
    QJsonValue array();
    void arrayElements(QJsonArray &array);
    void arrayElement(QJsonArray &array);
    quint32 arrayIndex();

    // Token processing
    // Asserts that a given token must come next and increments the cursor on success
    // throws a ParseError on failure
    void match(TokenType type);
    // Consumes one token and moves the cursor
    void consume()
    {
        _lookahead = _lexer.nextToken();
    }

private:
    Lexer &_lexer;
    Token _lookahead;
    bool _shouldTrace{false};
};

Token Lexer::nextToken()
{
    // End of input
    if(_currentIndex >= _input.length())
        return {TokenType::EndOfInput, "EOF"};

    // Tokenize a dictionary start
    if(_input.mid(_currentIndex, DictionaryOpenText.length()) == DictionaryOpenText)
    {
        _currentIndex += DictionaryOpenText.length();
        return {TokenType::DictionaryOpen, DictionaryOpenText};
    }
    // Tokenize an array start
    else if(_input.mid(_currentIndex, ArrayOpenText.length()) == ArrayOpenText)
    {
        _currentIndex += ArrayOpenText.length();
        return {TokenType::ArrayOpen, ArrayOpenText};
    }
    // Tokenize a block close ("}")
    else if(_input.at(_currentIndex) == BlockCloseText)
    {
        _currentIndex++;
        return {TokenType::BlockClose, BlockCloseText};
    }
    // Tokenize a colon - represents the separator for key/values in a dictionary or array
    else if(_input.mid(_currentIndex, ColonText.length()) == ColonText)
    {
        _currentIndex += ColonText.length();
        return {TokenType::Colon, ColonText};
    }
    // Newlines are tokens as they're significant in this grammar
    else if(_input.at(_currentIndex) == '\n')
    {
        // After a new-line, clear out the white-space until the next lexeme.
        // In this grammar new-lines ARE significant, but spaces (except those around a ':' and/or between identifiers)
        // can be safely ignored. This won't impact the required spaces around a ':' as
        // a key (or index for an array) identifier must occur first.
        while(_currentIndex < _input.length() && _input.at(_currentIndex).isSpace())
            _currentIndex++;

        return {TokenType::NewLine, "NewLine"};
    }
    // Tokenize a No Key Error - occurs when scutil can't find a key for a dict
    else if(_input.mid(_currentIndex, NoKeyErrorText.length()) == NoKeyErrorText)
    {
        _currentIndex += NoKeyErrorText.length();
        return {TokenType::NoKeyError, "NoKeyError"};
    }
    // Ignore non-newline whitespace between lexemes.  This allows multi-token
    // scalar values and trailing spaces before a newline without getting stuck
    // on an empty identifier token.
    else if(_input.at(_currentIndex).isSpace())
    {
        while(_currentIndex < _input.length() && _input.at(_currentIndex).isSpace() &&
              _input.at(_currentIndex) != '\n')
        {
            _currentIndex++;
        }

        return nextToken();
    }
    // Treat all remaining text as "identifiers", these include dictionary keys, values, and array indices
    else
    {
        int start = _currentIndex;

        // Identifiers cannot contain spaces, read remaining text until the next space
        // and store it in an indentifier token.
        while(_currentIndex < _input.length() && !_input.at(_currentIndex).isSpace())
            _currentIndex++;

        return {TokenType::Identifier, _input.mid(start, _currentIndex - start)};
    }
}

void Parser::match(TokenType type)
{
    if(_shouldTrace)
        qInfo() << "Expecting a token of type:" << tokenMap.at(type);

    if(_lookahead.type == type)
    {
        if(_shouldTrace)
            qInfo() << "Matched a token of type:" << tokenMap.at(type) << "text: " << _lookahead.text;

        consume();
    }
    else
    {
        auto tokenStr{tokenMap.at(_lookahead.type)};
        throw ParseError{"Got wrong token type: " + tokenStr + " " + _lookahead.text};
    }
}

QJsonValue Parser::dictionary()
{
    QJsonObject dict;
    match(TokenType::DictionaryOpen);
    match(TokenType::NewLine);
    dictionaryElements(dict);
    match(TokenType::BlockClose);

    if(dict.contains(QLatin1String("PIAEmpty")))
        return QJsonValue::Null;
    else
        return dict;
}

void Parser::dictionaryElements(QJsonObject &dict)
{
    // Allow empty dictionaries
    if(_lookahead.type == TokenType::BlockClose)
    {
        return;
    }
    dictionaryElement(dict);
    while(_lookahead.type == TokenType::NewLine)
    {
        // Mandatory new line separator between dictionary elements.
        // The scutil grammar does not use commas.
        match(TokenType::NewLine);

        // Because a newline could indicate either start of another element
        // or the end of the entire dict
        if(_lookahead.type == TokenType::BlockClose)
          return;

        dictionaryElement(dict);
    }
}

void Parser::dictionaryElement(QJsonObject &dict)
{
    auto key = dictionaryKey();
    match(TokenType::Colon);
    auto value = containerValue();

    if(_shouldTrace)
        qInfo() << "Adding a key of" << key << "to dictionary";

    dict.insert(key, value);
}

QString Parser::dictionaryKey()
{
    if(_lookahead.type == TokenType::Identifier)
    {
        // Restrict keys to alphabetic characters
        QRegularExpression regex("^[A-Za-z_][A-Za-z0-9_]*$");

        // Check if the lookahead text matches the regular expression
        QRegularExpressionMatch result = regex.match(_lookahead.text);
        if(result.hasMatch())
        {
            // Must store str here as following match will otherwise consume it
            auto str = _lookahead.text;
            match(TokenType::Identifier);
            return str;
        }
    }

    throw ParseError{"Not a valid dictionary identifier, got: " + _lookahead.text};
}

QJsonValue Parser::containerValue()
{
    // Values can be any kind of identifier (not just alphabetic)
    if(_lookahead.type == TokenType::Identifier)
    {
        QStringList parts;
        do
        {
            parts.push_back(_lookahead.text);
            match(TokenType::Identifier);
        } while(_lookahead.type == TokenType::Identifier);

        return parts.join(' ');
    }
    else if(_lookahead.type == TokenType::DictionaryOpen)
        return dictionary();
    else if(_lookahead.type == TokenType::ArrayOpen)
        return array();
    else
        throw ParseError{"Unexpected token type for container value, got:" + _lookahead.text};
}

QJsonValue Parser::array()
{
    QJsonArray array;
    match(TokenType::ArrayOpen);
    match(TokenType::NewLine);
    arrayElements(array);
    match(TokenType::BlockClose);

    return array;
}

void Parser::arrayElements(QJsonArray &array)
{
    // Allow empty arrays
    if(_lookahead.type == TokenType::BlockClose)
    {
        return;
    }
    arrayElement(array);
    while(_lookahead.type == TokenType::NewLine)
    {
        // Mandatory new line separator between array elements.
        // The scutil grammar does not use commas.
        match(TokenType::NewLine);

        // Because a newline could indicate either start of another element
        // or the end of the entire array
        if(_lookahead.type == TokenType::BlockClose)
          return;

        arrayElement(array);
    }
}

void Parser::arrayElement(QJsonArray &array)
{
    auto index = arrayIndex();
    match(TokenType::Colon);
    auto value = containerValue();

    array.insert(index, value);
}

quint32 Parser::arrayIndex()
{
    if(_lookahead.type == TokenType::Identifier)
    {
        // Restrict indices to numeric characters
        QRegularExpression regex("^[0-9]+$");

        // Check if the lookahead text matches the regular expression
        QRegularExpressionMatch result = regex.match(_lookahead.text);
        if(result.hasMatch())
        {
            // Must store str here as following match will otherwise consume it
            auto str = _lookahead.text;
            match(TokenType::Identifier);

            bool ok{false};
            quint32 index = str.toUInt(&ok, 10);

            return index;
        }
    }

    throw ParseError{"Not a valid array index, got: " + _lookahead.text};
}

QJsonValue Parser::parse()
{
    try
    {
        // Get initial token
        consume();

        if(_lookahead.type == TokenType::DictionaryOpen)
        {
            auto dict = dictionary();
            qInfo() << "Accepted a dictionary";
            return dict;
        }
        else if(_lookahead.type == TokenType::ArrayOpen)
        {
            auto arr = array();
            qInfo() << "Accepted an array";
            return arr;
        }
        else if(_lookahead.type == TokenType::NoKeyError)
        {
            qInfo() << "Got a key error, returning null";
            return QJsonValue::Null;
        }
        else
        {
            qCritical() << "Root object must be an array or a dictionary";
            return QJsonValue::Undefined;
        }
    }
    catch(const std::exception &ex)
    {
        qCritical() << "Failed to parse scutil output: " << ex.what();
        return QJsonValue::Undefined;
    }
    catch(...)
    {
        qCritical() << "Failed to parse scutil output: Unknown error";
        return QJsonValue::Undefined;
    }
}
}

// Exposed function
QJsonValue scutilParse(const QString &text)
{
    try
    {
        Lexer lex{text};
        // Temporarily keep tracing at verbose level for chartering and initial release.
        return Parser{lex}.parse();
    }
    catch(const std::exception &ex)
    {
        qCritical() << "Failed to parse scutil output" << ex.what();
        return QJsonValue::Undefined;
    }
}
