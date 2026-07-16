#include "PatinaLexer.h"

namespace cw::patina
{
namespace
{
TokenKind keywordKindFor(const juce::String& text)
{
    if (text == "package") return TokenKind::keywordPackage;
    if (text == "version") return TokenKind::keywordVersion;
    if (text == "import") return TokenKind::keywordImport;
    if (text == "as") return TokenKind::keywordAs;
    if (text == "graph") return TokenKind::keywordGraph;
    if (text == "node") return TokenKind::keywordNode;
    if (text == "connect") return TokenKind::keywordConnect;
    if (text == "export") return TokenKind::keywordExport;
    if (text == "instrument") return TokenKind::keywordInstrument;
    if (text == "effect") return TokenKind::keywordEffect;
    if (text == "modulator") return TokenKind::keywordModulator;
    if (text == "let") return TokenKind::keywordLet;
    if (text == "param") return TokenKind::keywordParam;
    if (text == "true") return TokenKind::keywordTrue;
    if (text == "false") return TokenKind::keywordFalse;
    return TokenKind::identifier;
}

bool isIdentifierStart(juce::juce_wchar character)
{
    return juce::CharacterFunctions::isLetter(character) || character == '_';
}

bool isIdentifierContinue(juce::juce_wchar character)
{
    return juce::CharacterFunctions::isLetterOrDigit(character) || character == '_';
}

void addToken(juce::Array<Token>& tokens, TokenKind kind, const juce::String& text, int line, int column)
{
    tokens.add({ kind, text, line, column });
}

juce::String stripComment(const juce::String& line)
{
    bool insideString = false;
    juce::String result;

    for (int index = 0; index < line.length(); ++index)
    {
        auto character = line[index];
        if (character == '"')
            insideString = ! insideString;

        if (! insideString && character == '#')
            break;

        result << juce::String::charToString(character);
    }

    return result;
}
} // namespace

LexResult Lexer::tokenize(const juce::String& sourceText) const
{
    LexResult result;
    juce::Array<int> indentStack;
    indentStack.add(0);

    auto lines = juce::StringArray::fromLines(sourceText);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        auto lineNumber = lineIndex + 1;
        auto rawLine = lines[lineIndex];

        if (rawLine.containsChar('\t'))
            result.diagnostics.add({ lineNumber, 1, "Tabs are not allowed in Patina source. Use spaces for indentation." });

        auto commentStripped = stripComment(rawLine);
        auto trimmedLine = commentStripped.trimEnd();

        if (trimmedLine.trim().isEmpty())
            continue;

        int indentWidth = 0;
        while (indentWidth < trimmedLine.length() && trimmedLine[indentWidth] == ' ')
            ++indentWidth;

        auto currentIndent = indentStack.getLast();
        if (indentWidth > currentIndent)
        {
            indentStack.add(indentWidth);
            addToken(result.tokens, TokenKind::indent, {}, lineNumber, 1);
        }
        else if (indentWidth < currentIndent)
        {
            while (indentStack.size() > 1 && indentWidth < indentStack.getLast())
            {
                indentStack.removeLast();
                addToken(result.tokens, TokenKind::dedent, {}, lineNumber, 1);
            }

            if (indentWidth != indentStack.getLast())
                result.diagnostics.add({ lineNumber, 1, "Indentation does not match any previous block level." });
        }

        int column = indentWidth + 1;
        int characterIndex = indentWidth;
        while (characterIndex < trimmedLine.length())
        {
            auto character = trimmedLine[characterIndex];

            if (character == ' ')
            {
                ++characterIndex;
                ++column;
                continue;
            }

            if (character == '"' )
            {
                int startColumn = column;
                juce::String text;
                ++characterIndex;
                ++column;
                bool terminated = false;
                while (characterIndex < trimmedLine.length())
                {
                    auto stringCharacter = trimmedLine[characterIndex];
                    if (stringCharacter == '"')
                    {
                        terminated = true;
                        ++characterIndex;
                        ++column;
                        break;
                    }

                    text << juce::String::charToString(stringCharacter);
                    ++characterIndex;
                    ++column;
                }

                if (! terminated)
                {
                    result.diagnostics.add({ lineNumber, startColumn, "Unterminated string literal." });
                    break;
                }

                addToken(result.tokens, TokenKind::stringLiteral, text, lineNumber, startColumn);
                continue;
            }

            if (character == '-' && characterIndex + 1 < trimmedLine.length() && trimmedLine[characterIndex + 1] == '>')
            {
                addToken(result.tokens, TokenKind::arrow, "->", lineNumber, column);
                characterIndex += 2;
                column += 2;
                continue;
            }

            if (isIdentifierStart(character))
            {
                int startIndex = characterIndex;
                int startColumn = column;
                while (characterIndex < trimmedLine.length() && isIdentifierContinue(trimmedLine[characterIndex]))
                {
                    ++characterIndex;
                    ++column;
                }

                auto text = trimmedLine.substring(startIndex, characterIndex);
                addToken(result.tokens, keywordKindFor(text), text, lineNumber, startColumn);
                continue;
            }

            if (juce::CharacterFunctions::isDigit(character))
            {
                int startIndex = characterIndex;
                int startColumn = column;
                bool sawDot = false;
                while (characterIndex < trimmedLine.length())
                {
                    auto numberCharacter = trimmedLine[characterIndex];
                    if (juce::CharacterFunctions::isDigit(numberCharacter))
                    {
                        ++characterIndex;
                        ++column;
                        continue;
                    }

                    if (numberCharacter == '.' && ! sawDot)
                    {
                        sawDot = true;
                        ++characterIndex;
                        ++column;
                        continue;
                    }

                    break;
                }

                auto text = trimmedLine.substring(startIndex, characterIndex);
                addToken(result.tokens, sawDot ? TokenKind::floatLiteral : TokenKind::integerLiteral, text, lineNumber, startColumn);
                continue;
            }

            switch (character)
            {
                case ':': addToken(result.tokens, TokenKind::colon, ":", lineNumber, column); break;
                case ',': addToken(result.tokens, TokenKind::comma, ",", lineNumber, column); break;
                case '.': addToken(result.tokens, TokenKind::dot, ".", lineNumber, column); break;
                case '=': addToken(result.tokens, TokenKind::equals, "=", lineNumber, column); break;
                case '(': addToken(result.tokens, TokenKind::lParen, "(", lineNumber, column); break;
                case ')': addToken(result.tokens, TokenKind::rParen, ")", lineNumber, column); break;
                case '<': addToken(result.tokens, TokenKind::less, "<", lineNumber, column); break;
                case '>': addToken(result.tokens, TokenKind::greater, ">", lineNumber, column); break;
                default:
                    result.diagnostics.add({ lineNumber, column, "Unexpected character: '" + juce::String::charToString(character) + "'." });
                    break;
            }

            ++characterIndex;
            ++column;
        }

        addToken(result.tokens, TokenKind::lineBreak, {}, lineNumber, trimmedLine.length() + 1);
    }

    while (indentStack.size() > 1)
    {
        indentStack.removeLast();
        addToken(result.tokens, TokenKind::dedent, {}, juce::jmax(1, lines.size()), 1);
    }

    addToken(result.tokens, TokenKind::endOfFile, {}, juce::jmax(1, lines.size()), 1);
    return result;
}
} // namespace cw::patina
