#pragma once

#include <JuceHeader.h>

namespace cw::patina
{
enum class TokenKind
{
    endOfFile,
    lineBreak,
    indent,
    dedent,
    identifier,
    stringLiteral,
    integerLiteral,
    floatLiteral,
    keywordPackage,
    keywordVersion,
    keywordImport,
    keywordAs,
    keywordGraph,
    keywordNode,
    keywordConnect,
    keywordExport,
    keywordInstrument,
    keywordEffect,
    keywordModulator,
    keywordLet,
    keywordParam,
    keywordTrue,
    keywordFalse,
    colon,
    comma,
    dot,
    equals,
    arrow,
    lParen,
    rParen,
    less,
    greater
};

struct LexDiagnostic
{
    int line = 0;
    int column = 0;
    juce::String message;
};

struct Token
{
    TokenKind kind = TokenKind::endOfFile;
    juce::String text;
    int line = 0;
    int column = 0;
};

struct LexResult
{
    juce::Array<Token> tokens;
    juce::Array<LexDiagnostic> diagnostics;
};

class Lexer final
{
public:
    LexResult tokenize(const juce::String& sourceText) const;
};
} // namespace cw::patina
