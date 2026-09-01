#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>
#include <filesystem>
#include <cstdlib>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace fs = std::filesystem;

// .PG Language Interpreter v2.0

enum PgTokenType {
    PG_STORE, PG_AS, PG_IMPORT, PG_CREATEFOLDER, PG_CF,
    PG_IN, PG_SCRIPT, PG_PRINT, PG_IF, PG_ELSE, PG_WHILE,
    PG_FOR, PG_FN, PG_RETURN, PG_BREAK, PG_CONTINUE,
    PG_TRUE, PG_FALSE, PG_NULL, PG_AND, PG_OR, PG_NOT,
    PG_EQUAL, PG_NOTEQUAL, PG_LESS, PG_GREATER, PG_LESSEQUAL,
    PG_GREATEREQUAL, PG_PLUS, PG_MINUS, PG_STAR, PG_SLASH,
    PG_PERCENT, PG_PLUSPLUS, PG_MINUSMINUS,
    PG_COLON, PG_LBRACKET, PG_RBRACKET, PG_LBRACE, PG_RBRACE,
    PG_LPAREN, PG_RPAREN, PG_COMMA, PG_DOT, PG_SEMICOLON,
    PG_STRING, PG_NUMBER, PG_IDENT, PG_EOF, PG_COLONEQUAL,
    PG_ASSIGN, PG_NEWLINE
};

struct PgToken {
    PgTokenType type;
    std::string value;
    int line;
    PgToken() : type(PG_EOF), line(0) {}
    PgToken(PgTokenType t, const std::string& v, int l) : type(t), value(v), line(l) {}
};

static std::string currentFile;

void pgError(int line, const std::string& msg) {
    std::cerr << "\033[1;31m[.PG Error]\033[0m ";
    if (!currentFile.empty()) std::cerr << "in '" << currentFile << "' ";
    std::cerr << "Line " << line << ": " << msg << std::endl;
    exit(1);
}

static PgToken makeToken(PgTokenType t, const std::string& v, int l) {
    return PgToken(t, v, l);
}

// Lexer
class PgLexer {
public:
    PgLexer(const std::string& s, int sl = 1) : src(s), pos(0), line(sl) {}

    std::vector<PgToken> tokenize() {
        std::vector<PgToken> tokens;
        while (pos < src.size()) {
            char c = src[pos];
            if (c == '\n') { line++; pos++; continue; }
            if (c == ' ' || c == '\t' || c == '\r') { pos++; continue; }
            if (c == '-' && peek() == '-') { skipLine(); continue; }
            if (c == '/' && peek() == '/') { skipLine(); continue; }
            if (c == '/' && peek() == '*') { skipBlockComment(); continue; }
            if (c == '"') { tokens.push_back(readString()); continue; }
            if (isdigit(c) || (c == '.' && pos + 1 < src.size() && isdigit(src[pos + 1]))) {
                tokens.push_back(readNumber()); continue;
            }
            if (isalpha(c) || c == '_') { tokens.push_back(readIdent()); continue; }

            int tl = line;
            switch (c) {
                case ':': pos++; tokens.push_back(makeToken(PG_COLON, ":", tl)); continue;
                case '=':
                    pos++;
                    if (peek() == '=') { pos++; tokens.push_back(makeToken(PG_EQUAL, "==", tl)); }
                    else tokens.push_back(makeToken(PG_ASSIGN, "=", tl));
                    continue;
                case '!':
                    pos++;
                    if (peek() == '=') { pos++; tokens.push_back(makeToken(PG_NOTEQUAL, "!=", tl)); }
                    else tokens.push_back(makeToken(PG_NOT, "!", tl));
                    continue;
                case '<':
                    pos++;
                    if (peek() == '=') { pos++; tokens.push_back(makeToken(PG_LESSEQUAL, "<=", tl)); }
                    else tokens.push_back(makeToken(PG_LESS, "<", tl));
                    continue;
                case '>':
                    pos++;
                    if (peek() == '=') { pos++; tokens.push_back(makeToken(PG_GREATEREQUAL, ">=", tl)); }
                    else tokens.push_back(makeToken(PG_GREATER, ">", tl));
                    continue;
                case '+':
                    pos++;
                    if (peek() == '+') { pos++; tokens.push_back(makeToken(PG_PLUSPLUS, "++", tl)); }
                    else tokens.push_back(makeToken(PG_PLUS, "+", tl));
                    continue;
                case '-':
                    pos++;
                    if (peek() == '-') { pos++; tokens.push_back(makeToken(PG_MINUSMINUS, "--", tl)); }
                    else tokens.push_back(makeToken(PG_MINUS, "-", tl));
                    continue;
                case '*': pos++; tokens.push_back(makeToken(PG_STAR, "*", tl)); continue;
                case '/': pos++; tokens.push_back(makeToken(PG_SLASH, "/", tl)); continue;
                case '%': pos++; tokens.push_back(makeToken(PG_PERCENT, "%", tl)); continue;
                case '[': pos++; tokens.push_back(makeToken(PG_LBRACKET, "[", tl)); continue;
                case ']': pos++; tokens.push_back(makeToken(PG_RBRACKET, "]", tl)); continue;
                case '{': pos++; tokens.push_back(makeToken(PG_LBRACE, "{", tl)); continue;
                case '}': pos++; tokens.push_back(makeToken(PG_RBRACE, "}", tl)); continue;
                case '(': pos++; tokens.push_back(makeToken(PG_LPAREN, "(", tl)); continue;
                case ')': pos++; tokens.push_back(makeToken(PG_RPAREN, ")", tl)); continue;
                case ',': pos++; tokens.push_back(makeToken(PG_COMMA, ",", tl)); continue;
                case '.': pos++; tokens.push_back(makeToken(PG_DOT, ".", tl)); continue;
                case ';': pos++; tokens.push_back(makeToken(PG_SEMICOLON, ";", tl)); continue;
                default:
                    pgError(tl, std::string("Unexpected '") + c + "'");
            }
        }
        tokens.push_back(makeToken(PG_EOF, "", line));
        return tokens;
    }

private:
    std::string src;
    size_t pos;
    int line;
    char peek(int o = 1) const { return (pos + o < src.size()) ? src[pos + o] : '\0'; }
    void skipLine() { pos += 2; while (pos < src.size() && src[pos] != '\n') pos++; }
    void skipBlockComment() {
        pos += 2;
        while (pos < src.size() && !(src[pos] == '*' && peek() == '/')) {
            if (src[pos] == '\n') line++;
            pos++;
        }
        if (pos < src.size()) pos += 2;
    }

    PgToken readString() {
        int sl = line;
        pos++;
        std::string str;
        while (pos < src.size() && src[pos] != '"') {
            if (src[pos] == '\\' && pos + 1 < src.size()) {
                pos++;
                switch (src[pos]) {
                    case 'n': str += '\n'; break;
                    case 't': str += '\t'; break;
                    case '\\': str += '\\'; break;
                    case '"': str += '"'; break;
                    default: str += src[pos]; break;
                }
            } else {
                if (src[pos] == '\n') line++;
                str += src[pos];
            }
            pos++;
        }
        if (pos >= src.size()) pgError(sl, "Unterminated string");
        pos++;
        return makeToken(PG_STRING, str, sl);
    }

    PgToken readNumber() {
        int sl = line;
        std::string num;
        bool hasDot = false;
        while (pos < src.size() && (isdigit(src[pos]) || src[pos] == '.')) {
            if (src[pos] == '.') { if (hasDot) break; hasDot = true; }
            num += src[pos];
            pos++;
        }
        return makeToken(PG_NUMBER, num, sl);
    }

    PgToken readIdent() {
        int sl = line;
        std::string word;
        while (pos < src.size() && (isalnum(src[pos]) || src[pos] == '_' || src[pos] == '.' || src[pos] == '-' || src[pos] == '/')) {
            word += src[pos];
            pos++;
        }

        if (word == "store") return makeToken(PG_STORE, word, sl);
        if (word == "as") return makeToken(PG_AS, word, sl);
        if (word == "import") return makeToken(PG_IMPORT, word, sl);
        if (word == "createFolder") return makeToken(PG_CREATEFOLDER, word, sl);
        if (word == "cf") return makeToken(PG_CF, word, sl);
        if (word == "in") return makeToken(PG_IN, word, sl);
        if (word == "script") return makeToken(PG_SCRIPT, word, sl);
        if (word == "print") return makeToken(PG_PRINT, word, sl);
        if (word == "if") return makeToken(PG_IF, word, sl);
        if (word == "else") return makeToken(PG_ELSE, word, sl);
        if (word == "while") return makeToken(PG_WHILE, word, sl);
        if (word == "for") return makeToken(PG_FOR, word, sl);
        if (word == "fn") return makeToken(PG_FN, word, sl);
        if (word == "return") return makeToken(PG_RETURN, word, sl);
        if (word == "break") return makeToken(PG_BREAK, word, sl);
        if (word == "continue") return makeToken(PG_CONTINUE, word, sl);
        if (word == "true") return makeToken(PG_TRUE, word, sl);
        if (word == "false") return makeToken(PG_FALSE, word, sl);
        if (word == "null") return makeToken(PG_NULL, word, sl);
        if (word == "and") return makeToken(PG_AND, word, sl);
        if (word == "or") return makeToken(PG_OR, word, sl);
        if (word == "not") return makeToken(PG_NOT, word, sl);
        return makeToken(PG_IDENT, word, sl);
    }
};

// Value type
struct PgValue {
    std::string str;
    double num;
    bool boolean;
    bool isNum;
    bool isBool;

    PgValue() : num(0), boolean(false), isNum(false), isBool(false) {}
    PgValue(const std::string& s) : str(s), num(0), boolean(false), isNum(false), isBool(false) {}
    PgValue(double n) : num(n), boolean(false), isNum(true), isBool(false) {}
    PgValue(bool b) : num(0), boolean(b), isNum(false), isBool(true) {}

    std::string toString() const {
        if (isBool) return boolean ? "true" : "false";
        if (isNum) {
            if (num == (long long)num) return std::to_string((long long)num);
            return std::to_string(num);
        }
        return str;
    }
    double toNumber() const {
        if (isNum) return num;
        if (isBool) return boolean ? 1.0 : 0.0;
        try { return std::stod(str); } catch (...) { return 0; }
    }
    bool toBool() const {
        if (isBool) return boolean;
        if (isNum) return num != 0;
        return !str.empty() && str != "false" && str != "null";
    }
};

// Interpreter
class PgInterpreter {
public:
    PgInterpreter(const std::string& f) : filename(f) { currentFile = f; }

    void run() {
        std::ifstream file(filename);
        if (!file.is_open()) pgError(0, "Cannot open: " + filename);
        std::stringstream buf;
        buf << file.rdbuf();
        std::string source = buf.str();
        file.close();

        PgLexer lexer(source);
        tokens = lexer.tokenize();
        pos = 0;

        while (pos < tokens.size() && tokens[pos].type != PG_EOF) {
            execStmt();
        }
        std::cout << "\033[1;32m[.PG]\033[0m Done." << std::endl;
    }

private:
    std::string filename;
    std::vector<PgToken> tokens;
    size_t pos;
    std::map<std::string, PgValue> vars;

    PgToken& cur() { return tokens[pos]; }
    bool check(PgTokenType t) { return pos < tokens.size() && tokens[pos].type == t; }
    bool match(PgTokenType t) { if (check(t)) { pos++; return true; } return false; }

    void expect(PgTokenType t, const std::string& msg) {
        if (!check(t)) pgError(cur().line, msg);
        pos++;
    }

    void skipSemi() { while (match(PG_SEMICOLON)) {} }

    void execStmt() {
        switch (cur().type) {
            case PG_STORE: parseStore(); break;
            case PG_IMPORT: parseImport(); break;
            case PG_CREATEFOLDER: parseCreateFolder(); break;
            case PG_CF: parseCreateFile(); break;
            case PG_PRINT: parsePrint(); break;
            case PG_IF: parseIf(); break;
            case PG_WHILE: parseWhile(); break;
            case PG_FOR: parseFor(); break;
            case PG_FN: parseFn(); break;
            case PG_RETURN: pos++; while (!check(PG_SEMICOLON) && !check(PG_EOF) && !check(PG_RBRACE)) pos++; break;
            case PG_BREAK: pos++; break;
            case PG_CONTINUE: pos++; break;
            case PG_LBRACE: parseBlock(); break;
            default: parseExpr(); break;
        }
        skipSemi();
    }

    void parseBlock() {
        expect(PG_LBRACE, "Expected '{'");
        while (!check(PG_RBRACE) && !check(PG_EOF)) execStmt();
        expect(PG_RBRACE, "Expected '}'");
    }

    void parseStore() {
        int line = cur().line;
        pos++;
        expect(PG_LBRACKET, "Expected '['");
        std::string type = "Local";
        if (check(PG_IDENT) && cur().value == "type") {
            pos++;
            expect(PG_ASSIGN, "Expected '='");
            expect(PG_STRING, "Expected type string");
            type = tokens[pos - 1].value;
        }
        expect(PG_RBRACKET, "Expected ']'");
        expect(PG_COLON, "Expected ':'");
        PgValue val = parseExpr();
        expect(PG_AS, "Expected 'as'");
        expect(PG_IDENT, "Expected variable name");
        std::string name = tokens[pos - 1].value;
        if (type == "FromServerToLocal") {
            std::cout << "\033[1;36m[.PG]\033[0m Fetching: " << val.toString() << std::endl;
            val = PgValue(fetchUrl(val.toString()));
        }
        vars[name] = val;
        std::cout << "\033[1;36m[.PG]\033[0m " << name << " = \"" << val.toString() << "\"" << std::endl;
    }

    void parseImport() {
        pos++;
        expect(PG_LBRACKET, "Expected '['");
        expect(PG_IDENT, "Expected lib path");
        std::string lib = tokens[pos - 1].value;
        expect(PG_RBRACKET, "Expected ']'");
        std::cout << "\033[1;33m[.PG]\033[0m Import: " << lib << std::endl;
        if (fs::exists(lib + ".pglib"))
            std::cout << "\033[1;33m[.PG]\033[0m Loaded: " << lib << std::endl;
        else
            std::cout << "\033[1;33m[.PG]\033[0m Not found (stub): " << lib << std::endl;
    }

    void parseCreateFolder() {
        pos++;
        PgValue name = parseExpr();
        std::error_code ec;
        fs::create_directories(name.toString(), ec);
        if (ec) pgError(cur().line, "Failed: " + ec.message());
        std::cout << "\033[1;32m[.PG]\033[0m Folder: " << name.toString() << std::endl;
    }

    void parseCreateFile() {
        int line = cur().line;
        pos++;
        std::string fileName;
        if (check(PG_IDENT)) { fileName = cur().value; pos++; }
        else if (check(PG_STRING)) { fileName = cur().value; pos++; }
        else pgError(line, "Expected file name");
        expect(PG_IN, "Expected 'in'");
        expect(PG_LBRACKET, "Expected '['");
        std::string path;
        if (check(PG_STRING)) { path = cur().value; pos++; }
        else pgError(line, "Expected path string");
        expect(PG_RBRACKET, "Expected ']'");
        expect(PG_SCRIPT, "Expected 'script'");
        expect(PG_COLON, "Expected ':'");
        expect(PG_LBRACE, "Expected '{'");
        int depth = 1;
        std::string content;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].type == PG_LBRACE) depth++;
            if (tokens[pos].type == PG_RBRACE) { depth--; if (depth == 0) break; }
            if (!content.empty()) content += " ";
            content += tokens[pos].value;
            pos++;
        }
        expect(PG_RBRACE, "Expected '}'");
        while (!content.empty() && content.back() == ' ') content.pop_back();
        fs::path fullPath = fs::path(path) / fileName;
        if (fullPath.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(fullPath.parent_path(), ec);
        }
        std::ofstream out(fullPath);
        if (!out.is_open()) pgError(line, "Cannot create: " + fullPath.string());
        out << content;
        out.close();
        std::cout << "\033[1;32m[.PG]\033[0m File: " << fullPath.string() << std::endl;
    }

    void parsePrint() {
        pos++;
        PgValue val = parseExpr();
        std::cout << val.toString() << std::endl;
    }

    void parseIf() {
        pos++;
        PgValue cond = parseExpr();
        if (cond.toBool()) {
            if (check(PG_LBRACE)) parseBlock(); else execStmt();
            if (check(PG_ELSE)) { pos++; if (check(PG_LBRACE)) skipBlock(); else execStmt(); }
        } else {
            if (check(PG_LBRACE)) skipBlock(); else { pos++; skipSemi(); }
            if (check(PG_ELSE)) { pos++; if (check(PG_LBRACE)) parseBlock(); else execStmt(); }
        }
    }

    void skipBlock() {
        expect(PG_LBRACE, "Expected '{'");
        int d = 1;
        while (pos < tokens.size() && d > 0) {
            if (tokens[pos].type == PG_LBRACE) d++;
            if (tokens[pos].type == PG_RBRACE) d--;
            pos++;
        }
    }

    void parseWhile() {
        size_t start = pos;
        pos++;
        size_t cs = pos;
        PgValue cond = parseExpr();
        while (cond.toBool()) {
            if (check(PG_LBRACE)) parseBlock(); else execStmt();
            pos = cs;
            cond = parseExpr();
        }
        if (check(PG_LBRACE)) skipBlock();
    }

    void parseFor() {
        pos++;
        expect(PG_LPAREN, "Expected '('");
        // Init: store or assign or expr
        if (!check(PG_SEMICOLON)) {
            if (check(PG_STORE)) {
                parseStore();
            } else if (check(PG_IDENT) && pos + 1 < tokens.size() && tokens[pos + 1].type == PG_ASSIGN) {
                std::string n = tokens[pos].value;
                pos += 2;
                vars[n] = parseExpr();
            } else {
                parseExpr();
            }
        }
        expect(PG_SEMICOLON, "Expected ';'");
        size_t cs = pos; // start of condition
        // Parse condition expression
        PgValue cond = parseExpr();
        expect(PG_SEMICOLON, "Expected ';'");
        size_t is = pos; // start of increment
        // Skip increment expression to find closing paren
        int parenDepth = 1;
        while (pos < tokens.size() && parenDepth > 0) {
            if (tokens[pos].type == PG_LPAREN) parenDepth++;
            if (tokens[pos].type == PG_RPAREN) { parenDepth--; if (parenDepth == 0) break; }
            pos++;
        }
        expect(PG_RPAREN, "Expected ')'");
        // Execute loop
        while (cond.toBool()) {
            if (check(PG_LBRACE)) parseBlock(); else execStmt();
            // Execute increment
            size_t incStart = is;
            size_t savedPos = pos;
            pos = incStart;
            if (!check(PG_RPAREN)) {
                // Check if it's assignment: ident = expr
                if (check(PG_IDENT) && pos + 1 < tokens.size() && tokens[pos + 1].type == PG_ASSIGN) {
                    std::string n = tokens[pos].value;
                    pos += 2;
                    vars[n] = parseExpr();
                } else {
                    parseExpr();
                }
            }
            pos = savedPos;
            // Re-evaluate condition
            pos = cs;
            cond = parseExpr();
        }
        // Skip body after loop ends
        if (check(PG_LBRACE)) skipBlock();
    }

    void parseFn() {
        pos++;
        expect(PG_IDENT, "Expected fn name");
        std::string name = tokens[pos - 1].value;
        expect(PG_LPAREN, "Expected '('");
        expect(PG_RPAREN, "Expected ')'");
        expect(PG_LBRACE, "Expected '{'");
        int d = 1;
        while (pos < tokens.size() && d > 0) {
            if (tokens[pos].type == PG_LBRACE) d++;
            if (tokens[pos].type == PG_RBRACE) d--;
            pos++;
        }
        std::cout << "\033[1;35m[.PG]\033[0m fn '" << name << "' defined (stub)" << std::endl;
    }

    // Expressions
    PgValue parseExpr() { return parseOr(); }

    PgValue parseOr() {
        PgValue l = parseAnd();
        while (check(PG_OR)) { pos++; PgValue r = parseAnd(); l = PgValue(l.toBool() || r.toBool()); }
        return l;
    }

    PgValue parseAnd() {
        PgValue l = parseNot();
        while (check(PG_AND)) { pos++; PgValue r = parseNot(); l = PgValue(l.toBool() && r.toBool()); }
        return l;
    }

    PgValue parseNot() {
        if (check(PG_NOT)) { pos++; return PgValue(!parseComp().toBool()); }
        return parseComp();
    }

    PgValue parseComp() {
        PgValue l = parseAdd();
        while (check(PG_EQUAL) || check(PG_NOTEQUAL) || check(PG_LESS) ||
               check(PG_GREATER) || check(PG_LESSEQUAL) || check(PG_GREATEREQUAL)) {
            PgTokenType op = cur().type;
            pos++;
            PgValue r = parseAdd();
            if (op == PG_EQUAL) l = PgValue(l.toString() == r.toString());
            else if (op == PG_NOTEQUAL) l = PgValue(l.toString() != r.toString());
            else if (op == PG_LESS) l = PgValue(l.toNumber() < r.toNumber());
            else if (op == PG_GREATER) l = PgValue(l.toNumber() > r.toNumber());
            else if (op == PG_LESSEQUAL) l = PgValue(l.toNumber() <= r.toNumber());
            else if (op == PG_GREATEREQUAL) l = PgValue(l.toNumber() >= r.toNumber());
        }
        return l;
    }

    PgValue parseAdd() {
        PgValue l = parseMul();
        while (check(PG_PLUS) || check(PG_MINUS)) {
            PgTokenType op = cur().type;
            pos++;
            PgValue r = parseMul();
            if (op == PG_PLUS) {
                if (l.isNum && r.isNum) l = PgValue(l.num + r.num);
                else l = PgValue(l.toString() + r.toString());
            } else l = PgValue(l.toNumber() - r.toNumber());
        }
        return l;
    }

    PgValue parseMul() {
        PgValue l = parseUnary();
        while (check(PG_STAR) || check(PG_SLASH) || check(PG_PERCENT)) {
            PgTokenType op = cur().type;
            pos++;
            PgValue r = parseUnary();
            if (op == PG_STAR) l = PgValue(l.toNumber() * r.toNumber());
            else if (op == PG_SLASH) l = PgValue(l.toNumber() / r.toNumber());
            else l = PgValue(fmod(l.toNumber(), r.toNumber()));
        }
        return l;
    }

    PgValue parseUnary() {
        if (check(PG_MINUS)) { pos++; return PgValue(-parsePrimary().toNumber()); }
        return parsePrimary();
    }

    PgValue parsePrimary() {
        if (check(PG_NUMBER)) { double v = std::stod(cur().value); pos++; return PgValue(v); }
        if (check(PG_STRING)) {
            std::string raw = cur().value;
            int strLine = cur().line;
            pos++;
            std::string result;
            size_t i = 0;
            while (i < raw.size()) {
                if (raw[i] == '$' && i + 1 < raw.size() && raw[i + 1] == '{') {
                    i += 2;
                    std::string expr;
                    int depth = 1;
                    while (i < raw.size() && depth > 0) {
                        if (raw[i] == '{') depth++;
                        if (raw[i] == '}') { depth--; if (depth == 0) break; }
                        expr += raw[i];
                        i++;
                    }
                    i++; // skip }
                    PgLexer exprLexer(expr, strLine);
                    auto exprTokens = exprLexer.tokenize();
                    size_t savedPos = pos;
                    auto savedTokens = tokens;
                    tokens = exprTokens;
                    pos = 0;
                    PgValue val = parseExpr();
                    tokens = savedTokens;
                    pos = savedPos;
                    result += val.toString();
                } else { result += raw[i]; i++; }
            }
            return PgValue(result);
        }
        if (check(PG_TRUE)) { pos++; return PgValue(true); }
        if (check(PG_FALSE)) { pos++; return PgValue(false); }
        if (check(PG_NULL)) { pos++; return PgValue(std::string("null")); }
        if (check(PG_LPAREN)) {
            pos++;
            PgValue v = parseExpr();
            expect(PG_RPAREN, "Expected ')'");
            return v;
        }
        if (check(PG_PRINT)) {
            pos++;
            PgValue v = parseExpr();
            std::cout << v.toString() << std::endl;
            return v;
        }
        if (check(PG_IDENT)) {
            std::string name = cur().value;
            pos++;
            if (check(PG_COLON)) {
                pos++;
                expect(PG_IDENT, "Expected method");
                std::string method = tokens[pos - 1].value;
                expect(PG_LBRACKET, "Expected '['");
                int depth = 1;
                std::string argContent;
                while (pos < tokens.size() && depth > 0) {
                    if (tokens[pos].type == PG_LBRACKET) depth++;
                    if (tokens[pos].type == PG_RBRACKET) { depth--; if (depth == 0) break; }
                    if (!argContent.empty()) argContent += " ";
                    argContent += tokens[pos].value;
                    pos++;
                }
                expect(PG_RBRACKET, "Expected ']'");
                PgValue arg(argContent);
                return callMethod(name, method, arg);
            }
            if (vars.count(name)) return vars[name];
            pgError(cur().line, "Undefined: " + name);
        }
        pgError(cur().line, "Unexpected: " + cur().value);
        return PgValue();
    }

    PgValue callMethod(const std::string& varName, const std::string& method, const PgValue& arg) {
        std::string obj;
        if (vars.count(varName)) obj = vars[varName].toString();
        else obj = varName;

        if (method == "GetJSON") return PgValue(arg.toString());
        if (method == "Length") return PgValue((double)obj.size());
        if (method == "Upper") { std::string s = obj; std::transform(s.begin(), s.end(), s.begin(), ::toupper); return PgValue(s); }
        if (method == "Lower") { std::string s = obj; std::transform(s.begin(), s.end(), s.begin(), ::tolower); return PgValue(s); }
        if (method == "Contains") return PgValue(obj.find(arg.toString()) != std::string::npos);
        if (method == "Substr") { int n = (int)arg.toNumber(); return PgValue(obj.substr(0, n < (int)obj.size() ? n : obj.size())); }
        std::cout << "\033[1;33m[.PG]\033[0m Method '" << method << "' (stub)" << std::endl;
        return PgValue();
    }

    std::string fetchUrl(const std::string& url) {
#ifdef _WIN32
        URL_COMPONENTS uc = {};
        uc.dwStructSize = sizeof(uc);
        uc.dwSchemeLength = 1; uc.dwHostNameLength = 1; uc.dwUrlPathLength = 1; uc.dwExtraInfoLength = 1;
        std::wstring wurl(url.begin(), url.end());
        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return "{\"error\":\"bad url\"}";
        std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
        std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
        HINTERNET hs = WinHttpOpen(L".PG/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hs) return "{\"error\":\"session\"}";
        HINTERNET hc = WinHttpConnect(hs, host.c_str(), uc.nPort, 0);
        if (!hc) { WinHttpCloseHandle(hs); return "{\"error\":\"connect\"}"; }
        HINTERNET hr = WinHttpOpenRequest(hc, L"GET", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
        if (!hr) { WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return "{\"error\":\"request\"}"; }
        WinHttpSendRequest(hr, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        std::string resp;
        if (WinHttpReceiveResponse(hr, NULL)) {
            DWORD avail = 0;
            while (WinHttpQueryDataAvailable(hr, &avail) && avail > 0) {
                std::vector<char> buf(avail + 1, 0);
                DWORD read = 0;
                WinHttpReadData(hr, buf.data(), avail, &read);
                resp.append(buf.data(), read);
            }
        }
        WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
        return resp.empty() ? "{\"error\":\"empty\"}" : resp;
#else
        return R"({"Users":[{"Username":"Player1","Id":1}]})";
#endif
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) { std::cout << "Usage: dot.pg.exe <file.pg>" << std::endl; return 1; }
    std::string file = argv[1];
    if (!fs::exists(file) && file.find(".pg") == std::string::npos) file += ".pg";
    if (!fs::exists(file)) { std::cerr << "\033[1;31m[.PG]\033[0m Not found: " << file << std::endl; return 1; }

    std::cout << "\033[1;36m";
    std::cout << "  \xc9\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xbb" << std::endl;
    std::cout << "  \xba     .PG Interpreter v2.0      \xba" << std::endl;
    std::cout << "  \xc8\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xbc" << std::endl;
    std::cout << "\033[0m";
    std::cout << "  " << file << "\n" << std::endl;

    PgInterpreter interp(file);
    interp.run();
    return 0;
}
