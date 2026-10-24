#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace json {

// Minimal DOM parser without external dependencies.
// Enough for config.json / offsets.json: null/bool/number/string/array/object.
struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Value> array;
    std::unordered_map<std::string, Value> object;

    bool is_null()   const { return type == Type::Null; }
    bool is_bool()   const { return type == Type::Bool; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_array()  const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }

    const Value* find(const std::string& key) const {
        if (type != Type::Object) return nullptr;
        const auto it = object.find(key);
        return it == object.end() ? nullptr : &it->second;
    }
};

// On failure, error contains a message of the form "line 12, column 9: expected ':'".
std::optional<Value> parse(const std::string& text, std::string& error);

} // namespace json
