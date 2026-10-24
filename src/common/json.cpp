#include "common/json.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace json {
namespace {

struct Parser {
    const std::string& text;
    std::size_t pos = 0;
    std::size_t line = 1;
    std::size_t col = 1;

    explicit Parser(const std::string& t) : text(t) {}

    void advance() {
        if (pos >= text.size()) return;
        if (text[pos] == '\n') {
            ++line;
            col = 1;
        } else {
            ++col;
        }
        ++pos;
    }

    [[noreturn]] void fail(const std::string& msg) {
        throw std::runtime_error("line " + std::to_string(line) +
                                 ", column " + std::to_string(col) + ": " + msg);
    }

    void skip_ws() {
        while (pos < text.size() &&
               (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' || text[pos] == '\r')) {
            advance();
        }
    }

    char peek() const {
        return pos < text.size() ? text[pos] : '\0';
    }

    void expect(char c, const char* what) {
        if (peek() != c) {
            fail(std::string("expected '") + c + std::string("' (") + what + ")");
        }
        advance();
    }

    Value parse_value(int depth) {
        if (depth > 64) fail("nesting too deep");
        skip_ws();
        switch (peek()) {
        case '{': return parse_object(depth);
        case '[': return parse_array(depth);
        case '"': { Value v; v.type = Value::Type::String; v.string = parse_string(); return v; }
        case 't': parse_true();  { Value v; v.type = Value::Type::Bool; v.boolean = true;  return v; }
        case 'f': parse_false(); { Value v; v.type = Value::Type::Bool; v.boolean = false; return v; }
        case 'n': parse_null();  { Value v; v.type = Value::Type::Null; return v; }
        default:  return parse_number();
        }
    }

    Value parse_object(int depth) {
        Value v;
        v.type = Value::Type::Object;
        expect('{', "object start");
        skip_ws();
        if (peek() == '}') { advance(); return v; }
        for (;;) {
            skip_ws();
            if (peek() != '"') fail("expected object key string");
            std::string key = parse_string();
            skip_ws();
            expect(':', "after object key");
            v.object[key] = parse_value(depth + 1);
            skip_ws();
            const char c = peek();
            if (c == ',') { advance(); continue; }
            if (c == '}') { advance(); break; }
            fail("expected ',' or '}' in object");
        }
        return v;
    }

    Value parse_array(int depth) {
        Value v;
        v.type = Value::Type::Array;
        expect('[', "array start");
        skip_ws();
        if (peek() == ']') { advance(); return v; }
        for (;;) {
            v.array.push_back(parse_value(depth + 1));
            skip_ws();
            const char c = peek();
            if (c == ',') { advance(); continue; }
            if (c == ']') { advance(); break; }
            fail("expected ',' or ']' in array");
        }
        return v;
    }

    std::string parse_string() {
        expect('"', "string start");
        std::string out;
        for (;;) {
            if (pos >= text.size()) fail("unterminated string");
            const char c = text[pos];
            if (c == '"') { advance(); break; }
            if (c == '\\') {
                advance();
                if (pos >= text.size()) fail("unterminated escape");
                const char e = text[pos];
                advance();
                switch (e) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u':  out += parse_unicode_escape(); break;
                default:   fail("invalid escape character");
                }
                continue;
            }
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            out += c;
            advance();
        }
        return out;
    }

    // decoding \uXXXX (simple BMP characters; surrogate pairs come as two tokens)
    std::string parse_unicode_escape() {
        unsigned code = 0;
        for (int i = 0; i < 4; ++i) {
            if (pos >= text.size()) fail("unterminated \\u escape");
            const char c = text[pos];
            unsigned d;
            if (c >= '0' && c <= '9')      d = static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') d = static_cast<unsigned>(c - 'A' + 10);
            else fail("invalid hex digit in \\u escape");
            code = code * 16 + d;
            advance();
        }
        std::string out;
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        return out;
    }

    void parse_true() {
        const std::string lit = "true";
        if (text.compare(pos, lit.size(), lit) != 0) fail("invalid literal");
        for (std::size_t i = 0; i < lit.size(); ++i) advance();
    }
    void parse_false() {
        const std::string lit = "false";
        if (text.compare(pos, lit.size(), lit) != 0) fail("invalid literal");
        for (std::size_t i = 0; i < lit.size(); ++i) advance();
    }
    void parse_null() {
        const std::string lit = "null";
        if (text.compare(pos, lit.size(), lit) != 0) fail("invalid literal");
        for (std::size_t i = 0; i < lit.size(); ++i) advance();
    }

    Value parse_number() {
        const std::size_t start = pos;
        if (peek() == '-') advance();
        bool any_digit = false;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
            advance();
            any_digit = true;
        }
        if (peek() == '.') {
            advance();
            while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) advance();
        }
        if (!any_digit) fail("expected number");
        Value v;
        v.type = Value::Type::Number;
        v.number = std::strtod(text.substr(start, pos - start).c_str(), nullptr);
        return v;
    }
};

} // namespace

std::optional<Value> parse(const std::string& text, std::string& error) {
    try {
        Parser p(text);
        Value v = p.parse_value(0);
        p.skip_ws();
        if (p.pos != text.size()) p.fail("trailing characters after JSON value");
        error.clear();
        return v;
    } catch (const std::exception& e) {
        error = e.what();
        return std::nullopt;
    }
}

} // namespace json
