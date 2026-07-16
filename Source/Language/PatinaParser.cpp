#include "PatinaParser.h"

namespace cw::patina
{
namespace
{
class ParserState
{
public:
    explicit ParserState(const juce::Array<Token>& inputTokens) : tokens(inputTokens) {}

    ParseResult run()
    {
        skipLineBreaks();
        while (! isAt(TokenKind::endOfFile))
        {
            if (match(TokenKind::keywordPackage))
                parsePackage();
            else if (match(TokenKind::keywordVersion))
                parseVersion();
            else if (match(TokenKind::keywordImport))
                parseImport();
            else if (match(TokenKind::keywordGraph))
                parseGraph();
            else if (match(TokenKind::keywordExport))
                parseExport();
            else
            {
                addDiagnostic(peek(), "Expected package, version, import, graph, or export declaration.");
                advanceToNextLine();
            }

            skipLineBreaks();
        }

        validate();
        return result;
    }

private:
    const juce::Array<Token>& tokens;
    int position = 0;
    ParseResult result;

    const Token& peek(int offset = 0) const
    {
        auto index = juce::jlimit(0, tokens.size() - 1, position + offset);
        return tokens.getReference(index);
    }

    bool isAt(TokenKind kind) const { return peek().kind == kind; }

    bool match(TokenKind kind) const { return isAt(kind); }

    const Token& advance()
    {
        auto& token = tokens.getReference(juce::jlimit(0, tokens.size() - 1, position));
        if (position < tokens.size())
            ++position;
        return token;
    }

    bool accept(TokenKind kind)
    {
        if (! isAt(kind))
            return false;

        advance();
        return true;
    }

    void skipLineBreaks()
    {
        while (isAt(TokenKind::lineBreak))
            advance();
    }

    void advanceToNextLine()
    {
        while (! isAt(TokenKind::lineBreak) && ! isAt(TokenKind::endOfFile))
            advance();
        skipLineBreaks();
    }

    void addDiagnostic(const Token& token, const juce::String& message)
    {
        result.diagnostics.add({ token.line, token.column, message });
    }

    const Token* require(TokenKind kind, const juce::String& message)
    {
        if (! isAt(kind))
        {
            addDiagnostic(peek(), message);
            return nullptr;
        }

        return &advance();
    }

    juce::String parseStringLiteral(const juce::String& message)
    {
        if (auto* token = require(TokenKind::stringLiteral, message))
            return token->text;
        return {};
    }

    juce::String parseIdentifier(const juce::String& message)
    {
        if (auto* token = require(TokenKind::identifier, message))
            return token->text;
        return {};
    }

    QualifiedName parseQualifiedName(const juce::String& message)
    {
        QualifiedName name;
        auto firstSegment = parseIdentifier(message);
        if (firstSegment.isEmpty())
            return name;

        name.segments.add(firstSegment);
        while (accept(TokenKind::dot))
        {
            auto segment = parseIdentifier("Expected identifier after '.'.");
            if (segment.isEmpty())
                break;
            name.segments.add(segment);
        }
        return name;
    }

    Expression parseExpression()
    {
        Expression expression;
        auto token = peek();
        switch (token.kind)
        {
            case TokenKind::stringLiteral:
                expression.kind = Expression::Kind::stringLiteral;
                expression.text = advance().text;
                break;
            case TokenKind::integerLiteral:
                expression.kind = Expression::Kind::integerLiteral;
                expression.text = advance().text;
                break;
            case TokenKind::floatLiteral:
                expression.kind = Expression::Kind::floatLiteral;
                expression.text = advance().text;
                break;
            case TokenKind::keywordTrue:
            case TokenKind::keywordFalse:
                expression.kind = Expression::Kind::booleanLiteral;
                expression.text = advance().text;
                break;
            case TokenKind::identifier:
            {
                auto qualifiedName = parseQualifiedName("Expected expression.");
                expression.qualifiedName = qualifiedName;
                expression.text = qualifiedName.toString();
                expression.kind = qualifiedName.segments.size() > 1 ? Expression::Kind::qualifiedName
                                                                    : Expression::Kind::identifier;
                break;
            }
            default:
                addDiagnostic(token, "Expected expression.");
                break;
        }

        return expression;
    }

    TypeExpression parseTypeExpression()
    {
        TypeExpression typeExpression;
        typeExpression.baseName = parseQualifiedName("Expected type name.");

        if (accept(TokenKind::less))
        {
            while (! isAt(TokenKind::greater) && ! isAt(TokenKind::endOfFile))
            {
                if (isAt(TokenKind::identifier))
                    typeExpression.genericArguments.add(parseQualifiedName("Expected type argument.").toString());
                else if (isAt(TokenKind::integerLiteral))
                    typeExpression.genericArguments.add(advance().text);
                else
                {
                    addDiagnostic(peek(), "Expected type argument.");
                    break;
                }

                if (! accept(TokenKind::comma))
                    break;
            }

            require(TokenKind::greater, "Expected '>' to close type argument list.");
        }

        return typeExpression;
    }

    PortReference parsePortReference()
    {
        PortReference reference;
        reference.line = peek().line;
        reference.nodeName = parseIdentifier("Expected node name in port reference.");
        require(TokenKind::dot, "Expected '.' in port reference.");
        reference.portName = parseIdentifier("Expected port name in port reference.");
        return reference;
    }

    juce::Array<NamedArgument> parseArgumentList()
    {
        juce::Array<NamedArgument> arguments;

        if (isAt(TokenKind::rParen))
            return arguments;

        while (! isAt(TokenKind::rParen) && ! isAt(TokenKind::endOfFile))
        {
            NamedArgument argument;
            argument.name = parseIdentifier("Expected argument name.");
            require(TokenKind::colon, "Expected ':' after argument name.");
            argument.value = parseExpression();
            arguments.add(argument);

            if (! accept(TokenKind::comma))
                break;
        }

        return arguments;
    }

    void parsePackage()
    {
        auto packageToken = advance();
        juce::ignoreUnused(packageToken);
        result.sourceFile.packageName = parseStringLiteral("Expected package string literal.");
        require(TokenKind::lineBreak, "Expected end of line after package declaration.");
    }

    void parseVersion()
    {
        advance();
        result.sourceFile.version = parseStringLiteral("Expected version string literal.");
        require(TokenKind::lineBreak, "Expected end of line after version declaration.");
    }

    void parseImport()
    {
        auto importToken = advance();
        ImportDeclaration importDeclaration;
        importDeclaration.line = importToken.line;
        importDeclaration.path = parseStringLiteral("Expected import path string literal.");
        if (accept(TokenKind::keywordAs))
            importDeclaration.alias = parseIdentifier("Expected alias after 'as'.");

        result.sourceFile.imports.add(importDeclaration);
        require(TokenKind::lineBreak, "Expected end of line after import declaration.");
    }

    void parseGraph()
    {
        auto graphToken = advance();
        GraphDeclaration graph;
        graph.line = graphToken.line;
        graph.name = parseIdentifier("Expected graph name.");
        require(TokenKind::colon, "Expected ':' after graph name.");
        require(TokenKind::lineBreak, "Expected line break after graph declaration.");
        require(TokenKind::indent, "Expected indented block after graph declaration.");

        while (! isAt(TokenKind::dedent) && ! isAt(TokenKind::endOfFile))
        {
            if (isAt(TokenKind::lineBreak))
            {
                advance();
                continue;
            }

            if (match(TokenKind::keywordNode))
                parseNode(graph);
            else if (match(TokenKind::keywordConnect))
                parseConnection(graph);
            else if (match(TokenKind::keywordLet))
                parseLet(graph);
            else if (match(TokenKind::keywordParam))
                parseParam(graph);
            else
            {
                addDiagnostic(peek(), "Expected node, connect, let, or param inside graph block.");
                advanceToNextLine();
                continue;
            }

            require(TokenKind::lineBreak, "Expected end of line after graph statement.");
        }

        require(TokenKind::dedent, "Expected end of graph block.");
        result.sourceFile.graphs.add(graph);
    }

    void parseNode(GraphDeclaration& graph)
    {
        auto nodeToken = advance();
        NodeDeclaration node;
        node.line = nodeToken.line;
        node.name = parseIdentifier("Expected node name.");
        require(TokenKind::equals, "Expected '=' after node name.");
        node.callee = parseQualifiedName("Expected qualified node kind.");
        require(TokenKind::lParen, "Expected '(' after node kind.");
        node.arguments = parseArgumentList();
        require(TokenKind::rParen, "Expected ')' after node arguments.");
        graph.nodes.add(node);
    }

    void parseConnection(GraphDeclaration& graph)
    {
        auto connectToken = advance();
        ConnectionDeclaration connection;
        connection.line = connectToken.line;
        connection.source = parsePortReference();
        require(TokenKind::arrow, "Expected '->' in connection statement.");
        connection.destination = parsePortReference();
        graph.connections.add(connection);
    }

    void parseLet(GraphDeclaration& graph)
    {
        auto letToken = advance();
        LetDeclaration declaration;
        declaration.line = letToken.line;
        declaration.name = parseIdentifier("Expected let binding name.");
        require(TokenKind::equals, "Expected '=' after let binding name.");
        declaration.value = parseExpression();
        graph.lets.add(declaration);
    }

    void parseParam(GraphDeclaration& graph)
    {
        auto paramToken = advance();
        ParameterDeclaration declaration;
        declaration.line = paramToken.line;
        declaration.name = parseIdentifier("Expected parameter name.");
        require(TokenKind::colon, "Expected ':' after parameter name.");
        declaration.type = parseTypeExpression();
        if (accept(TokenKind::equals))
        {
            declaration.hasDefaultValue = true;
            declaration.defaultValue = parseExpression();
        }

        graph.parameters.add(declaration);
    }

    void parseExport()
    {
        auto exportToken = advance();
        ExportDeclaration exportDeclaration;
        exportDeclaration.line = exportToken.line;

        if (accept(TokenKind::keywordInstrument))
            exportDeclaration.kind = "instrument";
        else if (accept(TokenKind::keywordEffect))
            exportDeclaration.kind = "effect";
        else if (accept(TokenKind::keywordModulator))
            exportDeclaration.kind = "modulator";
        else if (accept(TokenKind::keywordGraph))
            exportDeclaration.kind = "graph";
        else
            addDiagnostic(peek(), "Expected instrument, effect, modulator, or graph after export.");

        exportDeclaration.graphName = parseIdentifier("Expected graph name after export kind.");
        result.sourceFile.exports.add(exportDeclaration);
        require(TokenKind::lineBreak, "Expected end of line after export declaration.");
    }

    void validate()
    {
        juce::StringArray graphNames;
        for (const auto& graph : result.sourceFile.graphs)
        {
            if (graphNames.contains(graph.name))
                result.diagnostics.add({ graph.line, 1, "Duplicate graph name '" + graph.name + "'." });
            else
                graphNames.add(graph.name);

            juce::StringArray localNames;
            for (const auto& parameter : graph.parameters)
            {
                if (localNames.contains(parameter.name))
                    result.diagnostics.add({ parameter.line, 1, "Duplicate graph-local name '" + parameter.name + "'." });
                else
                    localNames.add(parameter.name);
            }

            for (const auto& letDeclaration : graph.lets)
            {
                if (localNames.contains(letDeclaration.name))
                    result.diagnostics.add({ letDeclaration.line, 1, "Duplicate graph-local name '" + letDeclaration.name + "'." });
                else
                    localNames.add(letDeclaration.name);
            }

            juce::StringArray nodeNames;
            for (const auto& node : graph.nodes)
            {
                if (nodeNames.contains(node.name))
                    result.diagnostics.add({ node.line, 1, "Duplicate node name '" + node.name + "'." });
                else
                    nodeNames.add(node.name);

                if (localNames.contains(node.name))
                    result.diagnostics.add({ node.line, 1, "Node name '" + node.name + "' collides with another graph-local symbol." });
            }

            for (const auto& connection : graph.connections)
            {
                if (! nodeNames.contains(connection.source.nodeName))
                    result.diagnostics.add({ connection.source.line, 1, "Unknown source node '" + connection.source.nodeName + "' in connection." });
                if (! nodeNames.contains(connection.destination.nodeName))
                    result.diagnostics.add({ connection.destination.line, 1, "Unknown destination node '" + connection.destination.nodeName + "' in connection." });
            }
        }

        for (const auto& exportDeclaration : result.sourceFile.exports)
            if (! graphNames.contains(exportDeclaration.graphName))
                result.diagnostics.add({ exportDeclaration.line, 1, "Export references unknown graph '" + exportDeclaration.graphName + "'." });
    }
};
} // namespace

ParseResult Parser::parse(const juce::Array<Token>& tokens) const
{
    ParserState parserState(tokens);
    return parserState.run();
}
} // namespace cw::patina
