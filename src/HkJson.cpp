#include "HkJson.hpp"

#include "Utility.hpp"
#include <memory>
#include <sstream>

namespace hk
{
Json::JsonResult Json::loadFromFile(const std::string& path)
{
    std::ifstream jsonFile{path};
    if (jsonFile.fail())
    {
        std::string errBuff;
        sprint(errBuff, "Failed to load: %s", path.c_str());
        return {.json = nullptr, .error = errBuff};
    }

    /* Empty file */
    if (utils::isNextEOF(jsonFile))
    {
        return {.json = std::make_shared<JsonRootNode>(JsonObjectNode{}), .error = ""};
    }

    return parseStream(jsonFile);
}

Json::JsonResult Json::loadFromString(const std::string& data)
{
    /* Empty string */
    if (data.empty())
    {
        return {.json = std::make_shared<JsonRootNode>(JsonObjectNode{}), .error = ""};
    }

    std::istringstream inputStringStream(data);
    return parseStream(inputStringStream);
}

Json::JsonResult Json::parseStream(std::istream& stream, const bool returnEarly)
{
    char currentChar{0};
    std::string primaryAcc{};
    std::string secondaryAcc{};
    bool holdsObject{false};
    bool maybeCommaExactlyBeforeEndToken{false};

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

        if (currentChar == ',')
        {
            maybeCommaExactlyBeforeEndToken = true;
        }
        else if (currentChar != ' ' && currentChar != '\n' && currentChar != '}' && currentChar != ']')
        {
            maybeCommaExactlyBeforeEndToken = false;
        }

        switch (currentChar)
        {
            /* Dump away any spaces (unless collecting strings) and new lines */
            case ' ': {
                if (state == State::GOT_STRING_KEY_VALUE_OPENING_QUOTE ||
                    state == State::GETTING_STRING_KEY_VALUE_CHARS)
                {
                    secondaryAcc += currentChar;
                }
                else if (state == State::GOT_BRAKET_STRING_OPENING_QUOTE || state == State::GETTING_BRAKET_STRING_CHARS)
                {
                    primaryAcc += currentChar;
                }
                break;
            }
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

                    std::get<JsonObjectNode>(*currentNode)[primaryAcc] = std::move(std::get<JsonObjectNode>(*out.json));
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

                    std::get<JsonListNode>(*currentNode).emplace_back(std::move(std::get<JsonObjectNode>(*out.json)));
                    JSON_CHANGE_STATE(State::GOT_BRAKET_CURLY_CLOSING_TOKEN);
                }
                else
                {
                    return {.json = nullptr, .error = sinkCharAndGetError(currentChar, state)};
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

                    std::get<JsonObjectNode>(*currentNode)[primaryAcc] = std::move(std::get<JsonListNode>(*out.json));
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

                    if (maybeCommaExactlyBeforeEndToken)
                    {
                        return {.json = nullptr, .error = sinkCharAndGetError(currentChar, state)};
                    }

                    if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                    {
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
                        return {currentNode, ""};
                    }
                }
                break;
            }
            case ']': {
                if (state == State::GOT_BRAKET_OPENING_TOKEN || state == State::GOT_BRAKET_STRING_CLOSING_QUOTE ||
                    state == State::GOT_BRAKET_CURLY_CLOSING_TOKEN || state == State::GOT_BRAKET_BRAKET_CLOSING_TOKEN ||
                    state == State::GETTING_NUMBER_KEY_VALUE_CHARS || state == State::GOT_TRUE_TOKEN ||
                    state == State::GOT_FALSE_TOKEN || state == State::GOT_NULL_TOKEN)
                {
                    if (maybeCommaExactlyBeforeEndToken)
                    {
                        return {.json = nullptr, .error = sinkCharAndGetError(currentChar, state)};
                    }

                    if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                    {
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

                    JSON_CHANGE_STATE(State::GOT_BRAKET_CLOSING_TOKEN);
                    if (returnEarly)
                    {
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
                }
                else if (state == State::GOT_DOUBLE_DOT_SEPATATOR)
                {
                    JSON_CHANGE_STATE(State::GOT_STRING_KEY_VALUE_OPENING_QUOTE);
                }
                // TODO: Quotes are not escaped!
                else if (state == State::GETTING_STRING_KEY_VALUE_CHARS ||
                         state == State::GOT_STRING_KEY_VALUE_OPENING_QUOTE)
                {
                    JSON_CHANGE_STATE(State::GOT_STRING_KEY_VALUE_CLOSING_QUOTE);

                    std::get<JsonObjectNode>(*currentNode)[primaryAcc] = std::move(secondaryAcc);
                    primaryAcc.clear();
                    secondaryAcc.clear();
                }
                else if (state == State::GOT_BRAKET_OPENING_TOKEN)
                {
                    JSON_CHANGE_STATE(State::GOT_BRAKET_STRING_OPENING_QUOTE);
                }
                // TODO: Quotes are not escaped!
                else if (state == State::GETTING_BRAKET_STRING_CHARS || state == State::GOT_BRAKET_STRING_OPENING_QUOTE)
                {
                    JSON_CHANGE_STATE(State::GOT_BRAKET_STRING_CLOSING_QUOTE);

                    std::get<JsonListNode>(*currentNode).emplace_back(primaryAcc);
                    primaryAcc.clear();
                }
                break;
            }
            case ':': {
                if (state == State::GOT_KEY_NAME_CLOSING_QUOTE)
                {
                    JSON_CHANGE_STATE(State::GOT_DOUBLE_DOT_SEPATATOR);
                }
                else if (state == State::GOT_STRING_KEY_VALUE_OPENING_QUOTE ||
                         state == State::GETTING_STRING_KEY_VALUE_CHARS)
                {
                    secondaryAcc += currentChar;
                }
                else if (state == State::GOT_BRAKET_STRING_OPENING_QUOTE || state == State::GETTING_BRAKET_STRING_CHARS)
                {
                    primaryAcc += currentChar;
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
                    else if (currentChar == 'n' && isSpecialString(stream, "ull")) // null
                    {

                        if (state == State::GOT_DOUBLE_DOT_SEPATATOR)
                        {
                            std::get<JsonObjectNode>(*currentNode)[primaryAcc] = std::move(JsonNull{});
                            primaryAcc.clear();
                        }
                        else if (state == State::GOT_BRAKET_OPENING_TOKEN)
                        {
                            std::get<JsonListNode>(*currentNode).emplace_back(JsonNull{});
                        }

                        JSON_CHANGE_STATE(State::GOT_NULL_TOKEN);
                    }
                    else if (currentChar == 't' && isSpecialString(stream, "rue")) // true
                    {
                        if (state == State::GOT_DOUBLE_DOT_SEPATATOR)
                        {
                            std::get<JsonObjectNode>(*currentNode)[primaryAcc] = true;
                            primaryAcc.clear();
                        }
                        else if (state == State::GOT_BRAKET_OPENING_TOKEN)
                        {
                            std::get<JsonListNode>(*currentNode).emplace_back(true);
                        }

                        JSON_CHANGE_STATE(State::GOT_TRUE_TOKEN);
                    }
                    else if (currentChar == 'f' && isSpecialString(stream, "alse")) // false
                    {

                        if (state == State::GOT_DOUBLE_DOT_SEPATATOR)
                        {
                            std::get<JsonObjectNode>(*currentNode)[primaryAcc] = false;
                            primaryAcc.clear();
                        }
                        else if (state == State::GOT_BRAKET_OPENING_TOKEN)
                        {
                            std::get<JsonListNode>(*currentNode).emplace_back(false);
                        }

                        JSON_CHANGE_STATE(State::GOT_FALSE_TOKEN);
                    }
                    else
                    {
                        return {.json = nullptr, .error = sinkCharAndGetError(currentChar, state)};
                    }
                }
                else if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                {
                    secondaryAcc += currentChar;
                }
                else
                {
                    return {.json = nullptr, .error = sinkCharAndGetError(currentChar, state)};
                }
                break;
            }
        }
    }

    if (currentChar != 0 && state != State::GOT_CURLY_CLOSING_TOKEN && state != State::GOT_BRAKET_CLOSING_TOKEN)
    {
        return {nullptr, sinkCharAndGetError(currentChar, state, true)};
    }

    return {currentNode, ""};
}

std::string Json::sinkCharAndGetError(const char currentChar, State& state, const bool fileEnded)
{
    std::string errorStr;

    if (fileEnded)
    {
        if (state == State::GETTING_BRAKET_STRING_CHARS)
        {
            sprint(errorStr, "List value incomplete. Couldn't finish list");
        }
        else if (currentChar != 0 && state != State::GOT_CURLY_CLOSING_TOKEN &&
                 state != State::GOT_BRAKET_CLOSING_TOKEN)
        {
            errorStr = "Ending } or ] not found";
        }
    }
    else if (fileEnded && state == State::GETTING_KEY_NAME_CHARS)
    {
        errorStr = "File ended without getting object's key closing quote";
    }
    else if (state == State::GOT_BRAKET_CURLY_CLOSING_TOKEN)
    {
        errorStr = "Missing comma between } and {";
    }
    else if (state == State::GOT_BRAKET_BRAKET_CLOSING_TOKEN)
    {
        errorStr = "Missing comma between ] and [";
    }
    else if (state == State::GOT_KEY_NAME_CLOSING_QUOTE)
    {
        errorStr = "Cannot close object which has key but no separator value (missing ':')";
    }
    else if (state == State::GOT_DOUBLE_DOT_SEPATATOR)
    {
        errorStr = "Cannot close object which has key but no valid value (got separator, missing value)";
    }
    else if (state == State::GOT_STRING_KEY_VALUE_CLOSING_QUOTE)
    {
        errorStr = "Missing ',' separator between object entries";
    }
    else if (state == State::GOT_STRING_KEY_VALUE_OPENING_QUOTE || state == State::GETTING_STRING_KEY_VALUE_CHARS ||
             state == State::GETTING_BRAKET_STRING_CHARS)
    {
        errorStr = "Missing end quote for key's string value";
    }
    else if (state == State::GOT_CURLY_OPENING_TOKEN)
    {
        if (currentChar == '}')
        {
            sprint(errorStr, "Missplaced comma right before object close");
        }
        else
        {
            sprint(errorStr, "Name of object key needs to be surrounded by quotes, but got '%c' instead", currentChar);
        }
    }
    else if (state == State::GOT_BRAKET_STRING_CLOSING_QUOTE)
    {
        sprint(errorStr, "Missing comma after list element end");
    }
    else if (state == State::GETTING_BRAKET_STRING_CHARS)
    {
        sprint(errorStr, "List value incomplete. Couldn't finish list");
    }
    else if (state == State::GOT_BRAKET_OPENING_TOKEN)
    {
        if (currentChar == ']')
        {
            sprint(errorStr, "Missplaced comma right before list close");
        }
        else
        {
            sprint(errorStr, "Invalid list value inside, got: '%c'", currentChar);
        }
    }
    else if (state == State::GOT_MAP_KEY_VALUE_CLOSING_CURLY)
    {
        sprint(errorStr, "Curly opening token is missing ending token '}', got '%c' instead", currentChar);
    }
    else if (state == State::GET_OPENING_TOKEN)
    {
        sprint(errorStr, "Cannot start json file with '%c', it can only start with '[' or '{'", currentChar);
    }
    else
    {
        sprint(errorStr, "Invalid for char '%c' to follow next at: %s", currentChar, getStateString(state).c_str());
    }

    JSON_CHANGE_STATE(State::ERROR);
    return errorStr;
}

bool Json::isSpecialString(std::istream& stream, const std::string& specialStr)
{
    uint8_t readChars{0};
    for (const char& ch : specialStr)
    {
        if (utils::read1(stream) != ch)
        {
            break;
        }
        readChars++;
    }

    if (readChars != specialStr.size())
    {
        stream.seekg(-readChars, std::ios::cur);
        return false;
    }
    return true;
}

void Json::printJson(const JsonRootNode& node)
{
    if (std::holds_alternative<JsonObjectNode>(node))
    {
        printJsonObject(std::get<JsonObjectNode>(node), 0);
    }
    else if (std::holds_alternative<JsonListNode>(node))
    {
        printJsonList(std::get<JsonListNode>(node), 0);
    }
    printf("\n");
}

void Json::printJsonObject(const JsonObjectNode& objNode, uint32_t depth)
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

void Json::printJsonList(const JsonListNode& objNode, uint32_t depth)
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

void Json::changeState(State& state, State newState, uint32_t line)
{
    if (state == newState)
    {
        // println("[%d] State is already %s", line, getStateString(newState).c_str());
        return;
    }
    println("[%d] Switching from %s to %s", line, getStateString(state).c_str(), getStateString(newState).c_str());
    state = newState;
}

std::string Json::getStateString(const State& state)
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

} // namespace hk