#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace hk
{

/* DISCLAIMER:
    - very basic error checks and notifiers
    - " character is not escaped inside tag values
    - Sure, it can be way more optimized but this wasn't the goal.
    - Except for a bit of back and forth, for all intents and purposes the algorithm is O(N)
      and the data is streamed directly from the file. Small adaptations can be done to read
      and process data from an in memory buffer.
    - Not intented to be used in any commercial product. Experimental only.
*/

class Json
{
    /* DEFINE REGION */
#define JSON_CHANGE_STATE(x) changeState(state, x, __LINE__);

#define IS_TYPE(type, name)                                                                                            \
    bool name() const                                                                                                  \
    {                                                                                                                  \
        return std::holds_alternative<type>(internalVariant);                                                          \
    }

#define _GET_TYPE_REF(type, name, constToken)                                                                          \
    constToken type& name() constToken                                                                                 \
    {                                                                                                                  \
        return std::get<type>(internalVariant);                                                                        \
    }

#define GET_TYPE_CONST_REF(type, name) _GET_TYPE_REF(type, name, const)
#define GET_TYPE_REF(type, name) _GET_TYPE_REF(type, name, )

#define _INIT_IMPLICIT_CTOR(type, constToken)                                                                          \
    _InternalFieldValue(constToken type val)                                                                           \
        : internalVariant{val}                                                                                         \
    {}

#define INIT_IMPLICIT_CTOR(type) _INIT_IMPLICIT_CTOR(type, )
#define INIT_IMPLICIT_CTOR_CONST_REF(type) _INIT_IMPLICIT_CTOR(type, const)

#define INIT_COPY_ASSIGN(type)                                                                                         \
    _InternalFieldValue& operator=(const type& rhs)                                                                    \
    {                                                                                                                  \
        internalVariant = std::forward<FieldValueVariant>(rhs);                                                        \
        return *this;                                                                                                  \
    }
#define INIT_MOVE_ASSIGN(type)                                                                                         \
    _InternalFieldValue& operator=(type&& rhs)                                                                         \
    {                                                                                                                  \
        internalVariant = std::forward<FieldValueVariant>(rhs);                                                        \
        return *this;                                                                                                  \
    }
    /* DEFINE REGION END*/

public:
    struct JsonObjectNode;
    struct JsonListNode;
    struct JsonNull
    {};

    using FieldValueVariant = std::variant<bool, double, int64_t, std::string, JsonObjectNode, JsonListNode, JsonNull>;

    template <typename T> struct _InternalFieldValue
    {

        _InternalFieldValue()
            : internalVariant{}
        {}

        // implicit copy ctors
        INIT_IMPLICIT_CTOR(bool);
        INIT_IMPLICIT_CTOR(double);
        INIT_IMPLICIT_CTOR(int32_t);
        INIT_IMPLICIT_CTOR(int64_t);
        INIT_IMPLICIT_CTOR(JsonNull);
        INIT_IMPLICIT_CTOR_CONST_REF(std::string);
        INIT_IMPLICIT_CTOR_CONST_REF(JsonObjectNode);
        INIT_IMPLICIT_CTOR_CONST_REF(JsonListNode);

        // copy assignment
        INIT_COPY_ASSIGN(JsonNull);
        INIT_COPY_ASSIGN(std::string);
        INIT_COPY_ASSIGN(JsonObjectNode);
        INIT_COPY_ASSIGN(JsonListNode);

        // move ctors
        _InternalFieldValue(FieldValueVariant&& val)
            : internalVariant{std::forward<FieldValueVariant>(val)}
        {}

        _InternalFieldValue(const char* val)
            : internalVariant{std::forward<FieldValueVariant>(val)}
        {}

        // move assignment
        INIT_MOVE_ASSIGN(std::string);
        INIT_MOVE_ASSIGN(const char*);
        INIT_MOVE_ASSIGN(JsonListNode);
        INIT_MOVE_ASSIGN(JsonObjectNode);
        INIT_MOVE_ASSIGN(JsonNull);

        IS_TYPE(bool, isBool);
        IS_TYPE(int64_t, isInt);
        IS_TYPE(double, isDouble);
        IS_TYPE(std::string, isString);
        IS_TYPE(JsonNull, isNull);
        IS_TYPE(JsonObjectNode, isObject);
        IS_TYPE(JsonListNode, isList);

        GET_TYPE_REF(bool, getBool);
        GET_TYPE_REF(int64_t, getInt);
        GET_TYPE_REF(double, getDouble);
        GET_TYPE_REF(std::string, getString);
        GET_TYPE_REF(JsonObjectNode, getObject);
        GET_TYPE_REF(JsonListNode, getList);

        GET_TYPE_CONST_REF(bool, getBool);
        GET_TYPE_CONST_REF(int64_t, getInt);
        GET_TYPE_CONST_REF(double, getDouble);
        GET_TYPE_CONST_REF(std::string, getString);
        GET_TYPE_CONST_REF(JsonObjectNode, getObject);
        GET_TYPE_CONST_REF(JsonListNode, getList);

#undef IS_TYPE
#undef _GET_TYPE_REF
#undef GET_TYPE_CONST_REF
#undef GET_TYPE_REF
#undef _INIT_IMPLICIT_CTOR
#undef INIT_IMPLICIT_CTOR
#undef INIT_IMPLICIT_CTOR_CONST_REF
#undef INIT_COPY_ASSIGN
#undef INIT_MOVE_ASSIGN

        _InternalFieldValue<FieldValueVariant>& operator[](const std::string& key)
        {
            return std::get<JsonObjectNode>(internalVariant)[key];
        }

        _InternalFieldValue<FieldValueVariant>& operator[](const uint64_t key)
        {
            return std::get<JsonListNode>(internalVariant)[key];
        }

        const _InternalFieldValue<FieldValueVariant>& operator[](const std::string& key) const
        {
            return std::get<JsonObjectNode>(internalVariant)[key];
        }

        const _InternalFieldValue<FieldValueVariant>& operator[](const uint64_t key) const
        {
            return std::get<JsonListNode>(internalVariant)[key];
        }

        /* Some types of FieldValueVariant are needed but are incomplete at this point so T generic is used. */
        T internalVariant;
    };

    using JsonFieldValue = _InternalFieldValue<FieldValueVariant>;

    struct JsonObjectNode : public std::unordered_map<std::string, JsonFieldValue>
    {
        JsonObjectNode() = default;

        JsonObjectNode(std::initializer_list<std::pair<const std::string, JsonFieldValue>> init)
            : std::unordered_map<std::string, JsonFieldValue>(init)
        {}
    };

    struct JsonListNode : public std::vector<JsonFieldValue>
    {
        JsonListNode() = default;

        JsonListNode(std::initializer_list<JsonFieldValue> initList)
            : std::vector<JsonFieldValue>(initList)
        {}
    };

    struct JsonRootNode : public std::variant<JsonObjectNode, JsonListNode>
    {
        JsonFieldValue& operator[](const std::string& key)
        {
            return std::get<JsonObjectNode>(*this)[key];
        }

        JsonFieldValue& operator[](const uint64_t key)
        {
            return std::get<JsonListNode>(*this)[key];
        }

        JsonObjectNode& getObject()
        {
            return std::get<JsonObjectNode>(*this);
        }

        JsonListNode& getList()
        {
            return std::get<JsonListNode>(*this);
        }

        bool isObject()
        {
            return std::holds_alternative<JsonObjectNode>(*this);
        }

        bool isList()
        {
            return std::holds_alternative<JsonListNode>(*this);
        }
    };

    using JsonNodeSPtr = std::shared_ptr<JsonRootNode>;

    struct JsonResult
    {
        JsonNodeSPtr json;
        const std::string error;
    };

    JsonResult loadFromFile(const std::string& path);
    JsonResult loadFromString(const std::string& data);
    JsonResult parseStream(std::istream& stream, const bool returnEarly = false);

    void printJson(const JsonRootNode& node);
    void printJsonObject(const JsonObjectNode& objNode, uint32_t depth = 0);
    void printJsonList(const JsonListNode& objNode, uint32_t depth = 0);

private:
    enum class State
    {
        ERROR,
        GET_OPENING_TOKEN,
        GOT_CURLY_OPENING_TOKEN,
        GOT_BRAKET_OPENING_TOKEN,
        GOT_CURLY_CLOSING_TOKEN,
        GOT_BRAKET_CLOSING_TOKEN,

        // map key
        GOT_KEY_NAME_OPENING_QUOTE,
        GETTING_KEY_NAME_CHARS,
        GOT_KEY_NAME_CLOSING_QUOTE,
        GOT_DOUBLE_DOT_SEPATATOR,

        // map string value
        GOT_STRING_KEY_VALUE_OPENING_QUOTE,
        GETTING_STRING_KEY_VALUE_CHARS,
        GOT_STRING_KEY_VALUE_CLOSING_QUOTE,

        // map number value
        GOT_NUMBER_KEY_VALUE_OPENING,
        GETTING_NUMBER_KEY_VALUE_CHARS,

        // map map value
        GOT_MAP_KEY_VALUE_OPENING_CURLY,
        GOT_MAP_KEY_VALUE_CLOSING_CURLY,

        // map list value
        GOT_LIST_KEY_VALUE_OPENING_BRACKET,
        GOT_LIST_KEY_VALUE_CLOSING_BRAKET,

        // list string value
        GOT_BRAKET_STRING_OPENING_QUOTE,
        GETTING_BRAKET_STRING_CHARS,
        GOT_BRAKET_STRING_CLOSING_QUOTE,

        // list map value
        GOT_BRAKET_CURLY_OPENING_TOKEN,
        GOT_BRAKET_CURLY_CLOSING_TOKEN,

        // list list value
        GOT_BRAKET_BRAKET_OPENING_TOKEN,
        GOT_BRAKET_BRAKET_CLOSING_TOKEN,

        // special tokens
        GOT_NULL_TOKEN,
        GOT_TRUE_TOKEN,
        GOT_FALSE_TOKEN
    };

    std::string sinkCharAndGetError(const char currentChar, State& currentState, const bool fileEnded = false);
    bool isSpecialString(std::istream& stream, const std::string& specialStr);
    void changeState(State& state, State newState, uint32_t line);
    std::string getStateString(const State& state);
}; // namespace hk

} // namespace hk