#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

#include "Utility.hpp"

namespace hk
{

struct Json
{
#define JSON_CHANGE_STATE(x) changeState(state, x, __LINE__);

#define IS_TYPE(type, name)                                                                                            \
    bool name() const                                                                                                  \
    {                                                                                                                  \
        return std::holds_alternative<type>(var);                                                                      \
    }

#define _GET_TYPE_REF(type, name, constToken)                                                                          \
    constToken type& name() constToken                                                                                 \
    {                                                                                                                  \
        return std::get<type>(var);                                                                                    \
    }

    struct JsonObjectNode;
    struct JsonListNode;
    struct JsonNull
    {};

#define GET_TYPE_CONST_REF(type, name) _GET_TYPE_REF(type, name, const)
#define GET_TYPE_REF(type, name) _GET_TYPE_REF(type, name, )

    using FieldValueVariant = std::variant<bool, double, int64_t, std::string, JsonObjectNode, JsonListNode, JsonNull>;

    template <typename T> struct _InternalFieldValue
    {
        _InternalFieldValue()
            : var{}
        {}

        template <typename VariantType>
        _InternalFieldValue(VariantType&& val)
            : var{std::forward<VariantType>(val)}
        {}

        template <typename VariantType> _InternalFieldValue& operator=(VariantType&& rhs)
        {
            var = std::move(rhs);
            return *this;
        }

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

        _InternalFieldValue<FieldValueVariant>& operator[](const std::string& key)
        {
            return std::get<JsonObjectNode>(var).at(key);
        }

        _InternalFieldValue<FieldValueVariant>& operator[](const uint64_t key)
        {
            return std::get<JsonListNode>(var).at(key);
        }

        const _InternalFieldValue<FieldValueVariant>& operator[](const std::string& key) const
        {
            return std::get<JsonObjectNode>(var).at(key);
        }

        const _InternalFieldValue<FieldValueVariant>& operator[](const uint64_t key) const
        {
            return std::get<JsonListNode>(var).at(key);
        }

        T var;
    };

    using JsonFieldValue = _InternalFieldValue<FieldValueVariant>;

    struct JsonObjectNode : public std::unordered_map<std::string, JsonFieldValue>
    {};

    struct JsonListNode : public std::vector<JsonFieldValue>
    {};

    struct JsonRootNode : public std::variant<JsonObjectNode, JsonListNode>
    {
        JsonFieldValue& operator[](const std::string& key)
        {
            return std::get<JsonObjectNode>(*this).at(key);
        }

        JsonObjectNode& rootObject()
        {
            if (!std::holds_alternative<JsonObjectNode>(*this))
            {
                throw std::runtime_error("operator[] container not an object");
            }

            return std::get<JsonObjectNode>(*this);
        }

        JsonListNode& rootList()
        {
            if (!std::holds_alternative<JsonListNode>(*this))
            {
                throw std::runtime_error("operator[] container not a list");
            }

            return std::get<JsonListNode>(*this);
        }
    };

    using JsonNodeSPtr = std::shared_ptr<JsonRootNode>;
    using JsonNodeWPtr = std::weak_ptr<JsonRootNode>;

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

    struct JsonResult
    {
        JsonNodeSPtr json;
        std::string error;
    };

    bool loadFrom(const std::string& path)
    {
        std::ifstream jsonFile{path};
        if (jsonFile.fail())
        {
            printlne("Failed to load: %s", path.c_str());
            return false;
        }

        const auto result = parseStream(jsonFile);

        if (result.error.empty() && result.json != nullptr)
        {
            printJson(*result.json);
            printf("\n");

            JsonRootNode& obj = *result.json;

            try
            {
                std::string& myStr = obj["company"]["departments"][0]["name"].getString();
                const JsonObjectNode& oNode = obj["company"].getObject();
                // for (const auto& [k, v] : oNode) {
                printJsonObject(oNode);
                // }
            }
            catch (const std::exception& e)
            {
                printlne("what -> %s", e.what());
            }
        }
        else
        {
            printlne("%s", result.error.c_str());
            printf("\n");
        }
        return true;
    }

    JsonResult parseStream(std::ifstream& stream, const bool returnEarly = false)
    {
        char currentChar{0};
        std::string primaryAcc{};
        std::string secondaryAcc{};
        bool holdsObject{false};

        State state{State::GET_OPENING_TOKEN};

        JsonNodeSPtr currentNode;
        while (!utils::isNextEOF(stream))
        {
            currentChar = utils::read1(stream);

            /* If the list/object has closed and we still get some unwanted tokens, error. */
            if (currentChar != ' ' && currentChar != '\n' &&
                (state == State::GOT_CURLY_CLOSING_TOKEN || state == State::GOT_BRAKET_CLOSING_TOKEN))
            {
                JSON_CHANGE_STATE(State::ERROR);
                std::string errBuff;
                sprint(errBuff, "Unexpected token after object/list end: '%c'", currentChar);
                return {nullptr, errBuff};
            }

            switch (currentChar)
            {
                /* Dump away any spaces and new lines */
                // TODO: accumulate spaces when in "string gathering" mode
                case ' ':
                case '\n': {
                    break;
                }

                /* Understand the type of outter region we are dealing with */
                case '{': {
                    if (state == State::GET_OPENING_TOKEN)
                    {
                        JSON_CHANGE_STATE(State::GOT_CURLY_OPENING_TOKEN);
                        currentNode = std::make_shared<JsonRootNode>(JsonObjectNode{});
                        holdsObject = true;
                    }
                    else if (state == State::GOT_DOUBLE_DOT_SEPATATOR)
                    {
                        JSON_CHANGE_STATE(State::GOT_MAP_KEY_VALUE_OPENING_CURLY);
                        stream.seekg(-1, std::ios::cur); // "un-cosume" the { token

                        const JsonResult out = parseStream(stream, true);
                        if (!out.error.empty())
                        {
                            return {nullptr, out.error};
                        }

                        std::get<JsonObjectNode>(*currentNode)[primaryAcc] =
                            std::move(std::get<JsonObjectNode>(*out.json));
                        primaryAcc.clear();

                        JSON_CHANGE_STATE(State::GOT_MAP_KEY_VALUE_CLOSING_CURLY);
                    }
                    else if (state == State::GOT_BRAKET_OPENING_TOKEN)
                    {
                        JSON_CHANGE_STATE(State::GOT_BRAKET_CURLY_OPENING_TOKEN);
                        stream.seekg(-1, std::ios::cur); // "un-cosume" the { token

                        const JsonResult out = parseStream(stream, true);
                        if (!out.error.empty())
                        {
                            return {nullptr, out.error};
                        }

                        std::get<JsonListNode>(*currentNode)
                            .emplace_back(std::move(std::get<JsonObjectNode>(*out.json)));
                        JSON_CHANGE_STATE(State::GOT_BRAKET_CURLY_CLOSING_TOKEN);
                    }
                    break;
                }
                case '[': {
                    if (state == State::GET_OPENING_TOKEN)
                    {
                        JSON_CHANGE_STATE(State::GOT_BRAKET_OPENING_TOKEN);
                        currentNode = std::make_shared<JsonRootNode>(JsonListNode{});
                        holdsObject = false;
                    }
                    else if (state == State::GOT_DOUBLE_DOT_SEPATATOR)
                    {
                        JSON_CHANGE_STATE(State::GOT_LIST_KEY_VALUE_OPENING_BRACKET);
                        stream.seekg(-1, std::ios::cur); // "un-cosume" the [ token

                        const JsonResult out = parseStream(stream, true);
                        if (!out.error.empty())
                        {
                            return {nullptr, out.error};
                        }

                        std::get<JsonObjectNode>(*currentNode)[primaryAcc] =
                            std::move(std::get<JsonListNode>(*out.json));
                        primaryAcc.clear();

                        JSON_CHANGE_STATE(State::GOT_LIST_KEY_VALUE_CLOSING_BRAKET);
                    }
                    else if (state == State::GOT_BRAKET_OPENING_TOKEN)
                    {
                        JSON_CHANGE_STATE(State::GOT_BRAKET_BRAKET_OPENING_TOKEN);
                        stream.seekg(-1, std::ios::cur); // "un-cosume" the [ token

                        const JsonResult out = parseStream(stream, true);
                        if (!out.error.empty())
                        {
                            return {nullptr, out.error};
                        }

                        std::get<JsonListNode>(*currentNode).emplace_back(std::move(std::get<JsonListNode>(*out.json)));

                        JSON_CHANGE_STATE(State::GOT_BRAKET_BRAKET_CLOSING_TOKEN);
                    }
                    break;
                }
                case '}': {
                    if (state == State::GOT_CURLY_OPENING_TOKEN || state == State::GOT_STRING_KEY_VALUE_CLOSING_QUOTE ||
                        state == State::GOT_MAP_KEY_VALUE_CLOSING_CURLY ||
                        state == State::GOT_LIST_KEY_VALUE_CLOSING_BRAKET ||
                        state == State::GETTING_NUMBER_KEY_VALUE_CHARS || state == State::GOT_TRUE_TOKEN ||
                        state == State::GOT_FALSE_TOKEN || state == State::GOT_NULL_TOKEN)
                    {
                        if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                        {
                            // printlne("number: %s", secondaryAcc.c_str());
                            if (secondaryAcc.contains('.'))
                            {
                                double theNumber = std::stod(secondaryAcc);
                                std::get<JsonObjectNode>(*currentNode)[primaryAcc] = theNumber;
                            }
                            else
                            {
                                int64_t theNumber = std::stoi(secondaryAcc);
                                std::get<JsonObjectNode>(*currentNode)[primaryAcc] = theNumber;
                            }
                            primaryAcc.clear();
                            secondaryAcc.clear();
                        }

                        JSON_CHANGE_STATE(State::GOT_CURLY_CLOSING_TOKEN);
                        if (returnEarly)
                        {
                            // printlne("return early");
                            return {currentNode, ""};
                        }
                    }
                    break;
                }
                case ']': {
                    if (state == State::GOT_BRAKET_OPENING_TOKEN || state == State::GOT_BRAKET_STRING_CLOSING_QUOTE ||
                        state == State::GOT_BRAKET_CURLY_CLOSING_TOKEN ||
                        state == State::GOT_BRAKET_BRAKET_CLOSING_TOKEN ||
                        state == State::GETTING_NUMBER_KEY_VALUE_CHARS || state == State::GOT_TRUE_TOKEN ||
                        state == State::GOT_FALSE_TOKEN || state == State::GOT_NULL_TOKEN)
                    {
                        if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                        {
                            // printlne("number: %s", secondaryAcc.c_str());

                            if (secondaryAcc.contains('.'))
                            {
                                double theNumber = std::stod(secondaryAcc);
                                std::get<JsonListNode>(*currentNode).push_back(JsonFieldValue{theNumber});
                            }
                            else
                            {
                                int64_t theNumber = std::stoi(secondaryAcc);
                                std::get<JsonListNode>(*currentNode).push_back(JsonFieldValue{theNumber});
                            }
                            primaryAcc.clear();
                            secondaryAcc.clear();
                        }

                        JSON_CHANGE_STATE(State::GOT_BRAKET_CLOSING_TOKEN);
                        if (returnEarly)
                        {
                            // printlne("return early list ");
                            return {currentNode, ""};
                        }
                    }
                    break;
                }
                case '"': {
                    if (state == State::GOT_CURLY_OPENING_TOKEN)
                    {
                        JSON_CHANGE_STATE(State::GOT_KEY_NAME_OPENING_QUOTE);
                    }
                    else if (state == State::GETTING_KEY_NAME_CHARS)
                    {
                        JSON_CHANGE_STATE(State::GOT_KEY_NAME_CLOSING_QUOTE);
                        // printlne("map key name is: %s", primaryAcc.c_str());
                    }
                    else if (state == State::GOT_DOUBLE_DOT_SEPATATOR)
                    {
                        JSON_CHANGE_STATE(State::GOT_STRING_KEY_VALUE_OPENING_QUOTE);
                    }
                    else if (state == State::GETTING_STRING_KEY_VALUE_CHARS)
                    {
                        JSON_CHANGE_STATE(State::GOT_STRING_KEY_VALUE_CLOSING_QUOTE);
                        // printlne("map key value is: %s", secondaryAcc.c_str());

                        std::get<JsonObjectNode>(*currentNode)[primaryAcc] = secondaryAcc;
                        primaryAcc.clear();
                        secondaryAcc.clear();
                    }
                    else if (state == State::GOT_BRAKET_OPENING_TOKEN)
                    {
                        JSON_CHANGE_STATE(State::GOT_BRAKET_STRING_OPENING_QUOTE);
                    }
                    else if (state == State::GETTING_BRAKET_STRING_CHARS)
                    {
                        JSON_CHANGE_STATE(State::GOT_BRAKET_STRING_CLOSING_QUOTE);
                        // printlne("list value is: %s", primaryAcc.c_str());

                        std::get<JsonListNode>(*currentNode).push_back(primaryAcc);
                        primaryAcc.clear();
                    }
                    break;
                }
                case ':': {
                    if (state == State::GOT_KEY_NAME_CLOSING_QUOTE)
                    {
                        JSON_CHANGE_STATE(State::GOT_DOUBLE_DOT_SEPATATOR);
                    }
                    break;
                }
                case ',': {
                    if (state == State::GOT_STRING_KEY_VALUE_CLOSING_QUOTE ||
                        state == State::GOT_MAP_KEY_VALUE_CLOSING_CURLY ||
                        state == State::GOT_LIST_KEY_VALUE_CLOSING_BRAKET ||
                        ((state == State::GOT_TRUE_TOKEN || state == State::GOT_FALSE_TOKEN ||
                             state == State::GOT_NULL_TOKEN || state == State::GETTING_NUMBER_KEY_VALUE_CHARS) &&
                            holdsObject))
                    {
                        if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                        {
                            // printlne("number: %s", secondaryAcc.c_str());
                            if (secondaryAcc.contains('.'))
                            {
                                double theNumber = std::stod(secondaryAcc);
                                std::get<JsonObjectNode>(*currentNode)[primaryAcc] = theNumber;
                            }
                            else
                            {
                                int64_t theNumber = std::stoi(secondaryAcc);
                                std::get<JsonObjectNode>(*currentNode)[primaryAcc] = theNumber;
                            }
                            primaryAcc.clear();
                            secondaryAcc.clear();
                        }

                        JSON_CHANGE_STATE(State::GOT_CURLY_OPENING_TOKEN);
                        break;
                    }
                    else if (state == State::GOT_BRAKET_STRING_CLOSING_QUOTE ||
                             state == State::GOT_BRAKET_CURLY_CLOSING_TOKEN ||
                             state == State::GOT_BRAKET_BRAKET_CLOSING_TOKEN ||
                             ((state == State::GOT_TRUE_TOKEN || state == State::GOT_FALSE_TOKEN ||
                                  state == State::GOT_NULL_TOKEN || state == State::GETTING_NUMBER_KEY_VALUE_CHARS) &&
                                 !holdsObject))
                    {
                        if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                        {
                            // printlne("number: %s", secondaryAcc.c_str());
                            if (secondaryAcc.contains('.'))
                            {
                                double theNumber = std::stod(secondaryAcc);
                                std::get<JsonListNode>(*currentNode).push_back(theNumber);
                            }
                            else
                            {
                                int64_t theNumber = std::stoi(secondaryAcc);
                                std::get<JsonListNode>(*currentNode).push_back(theNumber);
                            }
                            primaryAcc.clear();
                            secondaryAcc.clear();
                        }

                        JSON_CHANGE_STATE(State::GOT_BRAKET_OPENING_TOKEN);
                        break;
                    }

                    /* If it doesn't separate things special tokens, it's just a normal char. Fall. */
                    [[fallthrough]];
                }
                /* Default non-special chars aggregator */
                default: {
                    if (state == State::GOT_KEY_NAME_OPENING_QUOTE)
                    {
                        JSON_CHANGE_STATE(State::GETTING_KEY_NAME_CHARS);
                        primaryAcc += currentChar;
                    }
                    else if (state == State::GETTING_KEY_NAME_CHARS)
                    {
                        primaryAcc += currentChar;
                    }
                    else if (state == State::GOT_STRING_KEY_VALUE_OPENING_QUOTE)
                    {
                        JSON_CHANGE_STATE(State::GETTING_STRING_KEY_VALUE_CHARS);
                        secondaryAcc += currentChar;
                    }
                    else if (state == State::GETTING_STRING_KEY_VALUE_CHARS)
                    {
                        secondaryAcc += currentChar;
                    }
                    else if (state == State::GOT_BRAKET_STRING_OPENING_QUOTE)
                    {
                        JSON_CHANGE_STATE(State::GETTING_BRAKET_STRING_CHARS);
                        primaryAcc += currentChar;
                    }
                    else if (state == State::GETTING_BRAKET_STRING_CHARS)
                    {
                        primaryAcc += currentChar;
                    }
                    else if (state == State::GOT_DOUBLE_DOT_SEPATATOR || state == State::GOT_BRAKET_OPENING_TOKEN)
                    {
                        if ((currentChar >= '0' && currentChar <= '9') || currentChar == '-')
                        {
                            JSON_CHANGE_STATE(State::GETTING_NUMBER_KEY_VALUE_CHARS);
                            secondaryAcc += currentChar;
                        }
                        else if (currentChar == 'n')
                        {
                            int8_t readBackChars{0};
                            if (utils::read1(stream) == 'u')
                            {
                                readBackChars++;
                                if (utils::read1(stream) == 'l')
                                {
                                    readBackChars++;
                                    if (utils::read1(stream) == 'l')
                                    {
                                        readBackChars++;
                                        if (state == State::GOT_DOUBLE_DOT_SEPATATOR)
                                        {
                                            std::get<JsonObjectNode>(*currentNode)[primaryAcc] = JsonNull{};
                                            primaryAcc.clear();
                                        }
                                        else if (state == State::GOT_BRAKET_OPENING_TOKEN)
                                        {
                                            std::get<JsonListNode>(*currentNode).push_back(JsonNull{});
                                        }

                                        JSON_CHANGE_STATE(State::GOT_NULL_TOKEN);
                                    }
                                }
                            }

                            if (state != State::GOT_TRUE_TOKEN)
                            {
                                stream.seekg(-readBackChars, std::ios::cur);
                            }
                        }
                        else if (currentChar == 't')
                        {
                            int8_t readBackChars{0};
                            if (utils::read1(stream) == 'r')
                            {
                                readBackChars++;
                                if (utils::read1(stream) == 'u')
                                {
                                    readBackChars++;
                                    if (utils::read1(stream) == 'e')
                                    {
                                        readBackChars++;
                                        if (state == State::GOT_DOUBLE_DOT_SEPATATOR)
                                        {
                                            std::get<JsonObjectNode>(*currentNode)[primaryAcc] = true;
                                            primaryAcc.clear();
                                        }
                                        else if (state == State::GOT_BRAKET_OPENING_TOKEN)
                                        {
                                            std::get<JsonListNode>(*currentNode).push_back(true);
                                        }

                                        JSON_CHANGE_STATE(State::GOT_TRUE_TOKEN);
                                    }
                                }
                            }

                            if (state != State::GOT_TRUE_TOKEN)
                            {
                                stream.seekg(-readBackChars, std::ios::cur);
                            }
                        }
                        else if (currentChar == 'f')
                        {
                            int8_t readBackChars{0};
                            if (utils::read1(stream) == 'a')
                            {
                                readBackChars++;
                                if (utils::read1(stream) == 'l')
                                {
                                    readBackChars++;
                                    if (utils::read1(stream) == 's')
                                    {
                                        readBackChars++;
                                        if (utils::read1(stream) == 'e')
                                        {
                                            readBackChars++;
                                            if (state == State::GOT_DOUBLE_DOT_SEPATATOR)
                                            {
                                                std::get<JsonObjectNode>(*currentNode)[primaryAcc] = false;
                                                primaryAcc.clear();
                                            }
                                            else if (state == State::GOT_BRAKET_OPENING_TOKEN)
                                            {
                                                std::get<JsonListNode>(*currentNode).push_back(false);
                                            }

                                            JSON_CHANGE_STATE(State::GOT_FALSE_TOKEN);
                                        }
                                    }
                                }
                            }

                            if (state != State::GOT_FALSE_TOKEN)
                            {
                                stream.seekg(-readBackChars, std::ios::cur);
                            }
                        }
                    }
                    else if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                    {
                        secondaryAcc += currentChar;
                    }
                    break;
                }
            }
        }

        if (currentChar != 0 && state != State::GOT_CURLY_CLOSING_TOKEN && state != State::GOT_BRAKET_CLOSING_TOKEN)
        {
            JSON_CHANGE_STATE(State::ERROR);
            return {nullptr, "Ending curly or square bracket not found"};
        }

        return {currentNode, ""};
    }

    void printJson(const JsonRootNode& node)
    {
        if (std::holds_alternative<JsonObjectNode>(node))
        {
            printJsonObject(std::get<JsonObjectNode>(node), 0);
        }
        else if (std::holds_alternative<JsonListNode>(node))
        {
            printJsonList(std::get<JsonListNode>(node), 0);
        }
    }

    void printJsonObject(const JsonObjectNode& objNode, uint32_t depth = 0)
    {
        printf("{");
        const auto& kvMap = objNode;
        const uint64_t mapSize = kvMap.size();
        for (uint32_t i{0}; auto& [k, v] : kvMap)
        {
            if (v.isString())
            {
                printf("\"%s\":\"%s\"", k.c_str(), v.getString().c_str());
            }
            else if (v.isBool())
            {
                printf("\"%s\":%s", k.c_str(), v.getBool() ? "true" : "false");
            }
            else if (v.isNull())
            {
                printf("\"%s\":null", k.c_str());
            }
            else if (v.isInt())
            {
                printf("\"%s\":%ld", k.c_str(), v.getInt());
            }
            else if (v.isDouble())
            {
                printf("\"%s\":%lf", k.c_str(), v.getDouble());
            }
            else if (v.isObject())
            {
                printf("\"%s\":", k.c_str());
                printJsonObject(v.getObject(), depth + 1);
            }
            else if (v.isList())
            {
                printf("\"%s\":", k.c_str());
                printJsonList(v.getList(), depth + 1);
            }

            i++;
            if (i != mapSize)
            {
                printf(",");
            }
        }
        printf("}");
    }

    void printJsonList(const JsonListNode& objNode, uint32_t depth = 0)
    {
        printf("[");
        const auto& theList = objNode;
        const uint64_t mapSize = theList.size();
        for (uint32_t i{0}; const auto& v : theList)
        {
            if (v.isString())
            {
                printf("\"%s\"", v.getString().c_str());
            }
            else if (v.isBool())
            {
                printf("%s", v.getBool() ? "true" : "false");
            }
            else if (v.isNull())
            {
                printf("null");
            }
            else if (v.isInt())
            {
                printf("%ld", v.getInt());
            }
            else if (v.isDouble())
            {
                printf("%lf", v.getDouble());
            }
            else if (v.isObject())
            {
                printJsonObject(v.getObject(), depth + 1);
            }
            else if (v.isList())
            {
                printJsonList(v.getList(), depth + 1);
            }
            i++;
            if (i != mapSize)
            {
                printf(",");
            }
        }
        printf("]");
    }

    void changeState(State& state, State newState, uint32_t line)
    {
        if (state == newState)
        {
            // println("[%d] State is already %s", line, getStateString(newState).c_str());
            return;
        }
        // println("[%d] Switching from %s to %s", line, getStateString(state).c_str(),
        // getStateString(newState).c_str());
        state = newState;
    }

    std::string getStateString(const State& state)
    {
#define STATE_CASE(x)                                                                                                  \
    case x:                                                                                                            \
        return #x

        switch (state)
        {
            STATE_CASE(State::ERROR);
            STATE_CASE(State::GET_OPENING_TOKEN);
            STATE_CASE(State::GOT_CURLY_OPENING_TOKEN);
            STATE_CASE(State::GOT_BRAKET_OPENING_TOKEN);
            STATE_CASE(State::GOT_CURLY_CLOSING_TOKEN);
            STATE_CASE(State::GOT_BRAKET_CLOSING_TOKEN);
            STATE_CASE(State::GOT_KEY_NAME_OPENING_QUOTE);
            STATE_CASE(State::GETTING_KEY_NAME_CHARS);
            STATE_CASE(State::GOT_KEY_NAME_CLOSING_QUOTE);
            STATE_CASE(State::GOT_DOUBLE_DOT_SEPATATOR);
            STATE_CASE(State::GOT_STRING_KEY_VALUE_OPENING_QUOTE);
            STATE_CASE(State::GETTING_STRING_KEY_VALUE_CHARS);
            STATE_CASE(State::GOT_STRING_KEY_VALUE_CLOSING_QUOTE);
            STATE_CASE(State::GOT_MAP_KEY_VALUE_OPENING_CURLY);
            STATE_CASE(State::GOT_MAP_KEY_VALUE_CLOSING_CURLY);
            STATE_CASE(State::GOT_LIST_KEY_VALUE_OPENING_BRACKET);
            STATE_CASE(State::GOT_LIST_KEY_VALUE_CLOSING_BRAKET);
            STATE_CASE(State::GOT_BRAKET_STRING_OPENING_QUOTE);
            STATE_CASE(State::GETTING_BRAKET_STRING_CHARS);
            STATE_CASE(State::GOT_BRAKET_STRING_CLOSING_QUOTE);
            STATE_CASE(State::GOT_BRAKET_CURLY_OPENING_TOKEN);
            STATE_CASE(State::GOT_BRAKET_CURLY_CLOSING_TOKEN);
            STATE_CASE(State::GOT_BRAKET_BRAKET_OPENING_TOKEN);
            STATE_CASE(State::GOT_BRAKET_BRAKET_CLOSING_TOKEN);
            STATE_CASE(State::GOT_NUMBER_KEY_VALUE_OPENING);
            STATE_CASE(State::GETTING_NUMBER_KEY_VALUE_CHARS);
            STATE_CASE(State::GOT_NULL_TOKEN);
            STATE_CASE(State::GOT_TRUE_TOKEN);
            STATE_CASE(State::GOT_FALSE_TOKEN);
        }
#undef STATE_CASE
        return "<state unknown>";
    }
}; // namespace hk

} // namespace hk
int main(int argc, char** argv)
{
    hk::Json json;
    json.loadFrom("/home/hekapoo/Documents/probe/json/test.json");
    return 0;
}