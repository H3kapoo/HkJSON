#include "Utility.hpp"
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

namespace hk
{

struct Json
{
#define JSON_CHANGE_STATE(x) changeState(state, x, __LINE__);

    struct JsonObjectNode;
    struct JsonListNode;

    using JsonObjectNodeSPtr = std::shared_ptr<JsonObjectNode>;
    using JsonListNodeSPtr = std::shared_ptr<JsonListNode>;

    using FieldValue = std::variant<double, std::string, JsonObjectNode, JsonListNode>;

    struct JsonObjectNode : public std::unordered_map<std::string, FieldValue>
    {};

    using JsonNode = std::variant<JsonObjectNode, JsonListNode>;

    struct JsonListNode : public std::vector<FieldValue>
    {};

    using JsonNodeSPtr = std::shared_ptr<JsonNode>;
    using JsonNodeWPtr = std::weak_ptr<JsonNode>;

    enum class State
    {
        IDLE,
        AQUIRED_OPENING_CURLY_TOKEN,
        AQUIRED_OPENING_SQUARE_TOKEN,
        GET_CLOSING_CURLY_TOKEN,
        GET_CLOSING_SQUARE_TOKEN,
        AQUIRED_CLOSING_TOKEN,
        GET_KEY_NAME,
        AQUIRED_KEY_NAME,
        GET_KEY_VALUE,
        AQUIRED_KEY_VALUE,
        GET_KEY_STRING_VALUE,
        GET_KEY_MAP_VALUE,
        GET_KEY_LIST_VALUE,

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
        GOT_BRAKET_BRAKET_CLOSING_TOKEN
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

        if (result.error.empty())
        {
            printJson(*result.json);
        }
        else
        {
            printlne("%s", result.error.c_str());
        }
        printf("\n");
        return true;
    }

    JsonResult parseStream(std::ifstream& stream, const bool returnEarly = false)
    {
        char currentChar{};
        std::string primaryAcc{};
        std::string secondaryAcc{};

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
                case ' ':
                case '\n': {
                    break;
                }

                /* Understand the type of outter region we are dealing with */
                case '{': {
                    if (state == State::GET_OPENING_TOKEN)
                    {
                        JSON_CHANGE_STATE(State::GOT_CURLY_OPENING_TOKEN);
                        currentNode = std::make_shared<JsonNode>(JsonObjectNode{});
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

                        // TODO: we shall use pointers more to avoid copying/deref
                        std::get<JsonObjectNode>(*currentNode)[primaryAcc] = std::get<JsonObjectNode>(*out.json);
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

                        std::get<JsonListNode>(*currentNode).push_back(std::get<JsonObjectNode>(*out.json));
                        JSON_CHANGE_STATE(State::GOT_BRAKET_CURLY_CLOSING_TOKEN);
                    }
                    break;
                }
                case '[': {
                    if (state == State::GET_OPENING_TOKEN)
                    {
                        JSON_CHANGE_STATE(State::GOT_BRAKET_OPENING_TOKEN);
                        currentNode = std::make_shared<JsonNode>(JsonListNode{});
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

                        // TODO: we shall use pointers more to avoid copying/deref
                        std::get<JsonObjectNode>(*currentNode)[primaryAcc] = std::get<JsonListNode>(*out.json);
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

                        std::get<JsonListNode>(*currentNode).emplace_back(std::get<JsonListNode>(*out.json));

                        JSON_CHANGE_STATE(State::GOT_BRAKET_BRAKET_CLOSING_TOKEN);
                    }
                    break;
                }
                case '}': {
                    if (state == State::GOT_CURLY_OPENING_TOKEN || state == State::GOT_STRING_KEY_VALUE_CLOSING_QUOTE ||
                        state == State::GOT_MAP_KEY_VALUE_CLOSING_CURLY ||
                        state == State::GOT_LIST_KEY_VALUE_CLOSING_BRAKET ||
                        state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                    {
                        if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                        {
                            printlne("number: %s", secondaryAcc.c_str());
                            double theNumber = std::stod(secondaryAcc);
                            std::get<JsonObjectNode>(*currentNode)[primaryAcc] = theNumber;
                            primaryAcc.clear();
                            secondaryAcc.clear();
                        }

                        JSON_CHANGE_STATE(State::GOT_CURLY_CLOSING_TOKEN);
                        if (returnEarly)
                        {
                            printlne("return early");
                            return {currentNode, ""};
                        }
                    }
                    break;
                }
                case ']': {
                    if (state == State::GOT_BRAKET_OPENING_TOKEN || state == State::GOT_BRAKET_STRING_CLOSING_QUOTE ||
                        state == State::GOT_BRAKET_CURLY_CLOSING_TOKEN ||
                        state == State::GOT_BRAKET_BRAKET_CLOSING_TOKEN ||
                        state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                    {
                        if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                        {
                            printlne("number: %s", secondaryAcc.c_str());
                            double theNumber = std::stod(secondaryAcc);
                            std::get<JsonListNode>(*currentNode).push_back(theNumber);
                            primaryAcc.clear();
                            secondaryAcc.clear();
                        }

                        JSON_CHANGE_STATE(State::GOT_BRAKET_CLOSING_TOKEN);
                        if (returnEarly)
                        {
                            printlne("return early list ");
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
                        printlne("map key name is: %s", primaryAcc.c_str());
                    }
                    else if (state == State::GOT_DOUBLE_DOT_SEPATATOR)
                    {
                        JSON_CHANGE_STATE(State::GOT_STRING_KEY_VALUE_OPENING_QUOTE);
                    }
                    else if (state == State::GETTING_STRING_KEY_VALUE_CHARS)
                    {
                        JSON_CHANGE_STATE(State::GOT_STRING_KEY_VALUE_CLOSING_QUOTE);
                        printlne("map key value is: %s", secondaryAcc.c_str());

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
                        printlne("list value is: %s", primaryAcc.c_str());

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
                        (state == State::GETTING_NUMBER_KEY_VALUE_CHARS &&
                            std::holds_alternative<JsonObjectNode>(*currentNode)))
                    {
                        if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                        {
                            printlne("number: %s", secondaryAcc.c_str());
                            double theNumber = std::stod(secondaryAcc);
                            std::get<JsonObjectNode>(*currentNode)[primaryAcc] = theNumber;
                            primaryAcc.clear();
                            secondaryAcc.clear();
                        }

                        JSON_CHANGE_STATE(State::GOT_CURLY_OPENING_TOKEN);
                        break;
                    }
                    else if (state == State::GOT_BRAKET_STRING_CLOSING_QUOTE ||
                             state == State::GOT_BRAKET_CURLY_CLOSING_TOKEN ||
                             state == State::GOT_BRAKET_BRAKET_CLOSING_TOKEN ||
                             (state == State::GETTING_NUMBER_KEY_VALUE_CHARS &&
                                 std::holds_alternative<JsonListNode>(*currentNode)))
                    {
                        if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                        {
                            printlne("number: %s", secondaryAcc.c_str());
                            double theNumber = std::stod(secondaryAcc);
                            std::get<JsonListNode>(*currentNode).push_back(theNumber);
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
                    }
                    else if (state == State::GETTING_NUMBER_KEY_VALUE_CHARS)
                    {
                        secondaryAcc += currentChar;
                    }
                    break;
                }
            }
        }

        if (state != State::GOT_CURLY_CLOSING_TOKEN && state != State::GOT_BRAKET_CLOSING_TOKEN)
        {
            JSON_CHANGE_STATE(State::ERROR);
            return {nullptr, "Ending curly or square bracket not found"};
        }

        return {currentNode, ""};
    }

    JsonResult parseJsonObjectStream(std::ifstream& stream)
    {
        // while (!utils::isNextEOF(stream)) {}
        return {nullptr, ""};
    }

    // std::pair<JsonNodeSPtr, std::string> parseStream2(std::ifstream& stream)
    // {
    //     uint8_t ch{};
    //     std::string charAcc{};
    //     std::string keyNameAcc{};
    //     State state{State::GET_OPENING_TOKEN};
    //     bool aquiredOpeningTokenDueToComma{false};
    //     bool isCurly{false};
    //     JsonNodeSPtr currentNode; // = std::make_shared<JsonNode>();

    //     while (!utils::isNextEOF(stream))
    //     {
    //         if (state == State::ERROR)
    //         {
    //             return {nullptr, "some error occured"};
    //             break;
    //         }

    //         ch = utils::read1(stream);
    //         switch (ch)
    //         {
    //             case ' ':
    //             case '\n': {
    //                 // println("Skipped space or newline");
    //                 break;
    //             }
    //             case '{':
    //                 /* A valid object can start with { */
    //                 if (state == State::GET_OPENING_TOKEN)
    //                 {
    //                     JSON_CHANGE_STATE(State::AQUIRED_OPENING_CURLY_TOKEN);
    //                     isCurly = true;
    //                 }
    //                 /* Hitting { on this state means value of the key is another object. We need to recurse down.*/
    //                 if (state == State::GET_KEY_VALUE || state == State::AQUIRED_OPENING_SQUARE_TOKEN)
    //                 {
    //                     JSON_CHANGE_STATE(State::GET_KEY_MAP_VALUE);

    //                     stream.seekg(-1, std::ios::cur);
    //                     const auto& [returnedNode, error] = parseStream(stream);

    //                     if (state == State::GET_KEY_VALUE)
    //                     {
    //                         stream.seekg(-1, std::ios::cur);
    //                     }

    //                     if (!error.empty())
    //                     {
    //                         JSON_CHANGE_STATE(State::ERROR);
    //                         break;
    //                     }

    //                     if (!currentNode)
    //                     {
    //                         currentNode = std::make_shared<JsonNode>(JsonObjectNode{});
    //                     }

    //                     if (std::holds_alternative<JsonListNode>(*currentNode) &&
    //                         std::holds_alternative<JsonObjectNode>(*returnedNode))
    //                     {
    //                         std::get<JsonListNode>(*currentNode).emplace_back(std::get<JsonObjectNode>(*returnedNode));
    //                     }
    //                     else if (std::holds_alternative<JsonObjectNode>(*currentNode) &&
    //                              std::holds_alternative<JsonListNode>(*returnedNode))
    //                     {
    //                         std::get<JsonObjectNode>(*currentNode)[keyNameAcc] =
    //                         std::get<JsonListNode>(*returnedNode);
    //                     }

    //                     charAcc.clear();
    //                     keyNameAcc.clear();
    //                 }
    //                 break;
    //             case '}': {
    //                 /* Object should be completed in this case. */
    //                 if (state == State::AQUIRED_KEY_VALUE)
    //                 {
    //                     JSON_CHANGE_STATE(State::AQUIRED_CLOSING_TOKEN);
    //                     // we should stop right here. We have completed the object
    //                     println("RETURNING");
    //                     return {currentNode, ""};
    //                 }
    //                 /* Hitting } here means closing " is wrongly placed or missing. */
    //                 else if (state == State::GET_KEY_STRING_VALUE)
    //                 {
    //                     JSON_CHANGE_STATE(State::ERROR);
    //                     printlne("Missing closing \" while aquiring string value for key");
    //                 }
    //                 else if (state == State::AQUIRED_OPENING_CURLY_TOKEN)
    //                 {
    //                     /* } while we already got { AND holding some type in the variant it can only mean we have a
    //                     bad
    //                      * comma after a value of a key. */
    //                     if (aquiredOpeningTokenDueToComma)
    //                     {
    //                         JSON_CHANGE_STATE(State::ERROR);
    //                         printlne("Missplaced , after last element of object");
    //                     }
    //                     /* Otherwise it can be just the closing of an empty object. */
    //                     else
    //                     {
    //                         JSON_CHANGE_STATE(State::AQUIRED_CLOSING_TOKEN);
    //                         if (!currentNode)
    //                         {
    //                             currentNode = std::make_shared<JsonNode>(JsonObjectNode{});
    //                         }

    //                         println("RETURNING 2");
    //                         return {currentNode, ""};
    //                     }
    //                 }
    //                 /* Hitting } after we recursed down the object means we now need to close it. */
    //                 else if (state == State::GET_KEY_MAP_VALUE)
    //                 {
    //                     JSON_CHANGE_STATE(State::AQUIRED_KEY_VALUE);
    //                 }
    //                 break;
    //             }
    //             case '[':
    //                 /* A valid list can start with [ */
    //                 if (state == State::GET_OPENING_TOKEN)
    //                 {
    //                     if (!currentNode)
    //                     {
    //                         currentNode = std::make_shared<JsonNode>(JsonListNode{});
    //                     }
    //                     JSON_CHANGE_STATE(State::AQUIRED_OPENING_SQUARE_TOKEN);
    //                 }
    //                 else if (state == State::AQUIRED_OPENING_CURLY_TOKEN)
    //                 {
    //                     printlne("Cannot have list directly inside object");
    //                     JSON_CHANGE_STATE(State::ERROR);
    //                 }
    //                 else if (state == State::GET_KEY_VALUE)
    //                 {
    //                     JSON_CHANGE_STATE(State::GET_KEY_LIST_VALUE);

    //                     stream.seekg(-1, std::ios::cur);
    //                     const auto& [returnedNode, error] = parseStream(stream);
    //                     stream.seekg(-1, std::ios::cur);

    //                     if (!error.empty())
    //                     {
    //                         JSON_CHANGE_STATE(State::ERROR);
    //                         break;
    //                     }

    //                     if (!currentNode)
    //                     {
    //                         currentNode = std::make_shared<JsonNode>(JsonObjectNode{});
    //                     }

    //                     if (std::holds_alternative<JsonObjectNode>(*returnedNode))
    //                     {
    //                         std::get<JsonListNode>(*currentNode).emplace_back(std::get<JsonObjectNode>(*returnedNode));
    //                     }
    //                     else if (std::holds_alternative<JsonListNode>(*returnedNode))
    //                     {
    //                         std::get<JsonObjectNode>(*currentNode)[keyNameAcc] =
    //                         std::get<JsonListNode>(*returnedNode);
    //                     }

    //                     charAcc.clear();
    //                     keyNameAcc.clear();
    //                 }
    //                 break;
    //             case ']':
    //                 /* A valid list can end with ] */
    //                 if (state == State::AQUIRED_OPENING_SQUARE_TOKEN)
    //                 {
    //                     JSON_CHANGE_STATE(State::AQUIRED_CLOSING_TOKEN);
    //                     if (!currentNode)
    //                     {
    //                         currentNode = std::make_shared<JsonNode>(JsonListNode{});
    //                     }

    //                     println("RETURNING SQ 2");
    //                     return {currentNode, ""};
    //                 }
    //                 else if (state == State::AQUIRED_KEY_NAME)
    //                 {
    //                     if (!currentNode)
    //                     {
    //                         currentNode = std::make_shared<JsonNode>(JsonListNode{});
    //                     }
    //                     std::get<JsonListNode>(*currentNode).push_back(keyNameAcc);

    //                     charAcc.clear();
    //                     keyNameAcc.clear();
    //                     JSON_CHANGE_STATE(State::AQUIRED_CLOSING_TOKEN);

    //                     println("RETURNING LIST");
    //                     return {currentNode, ""};
    //                 }
    //                 else if (state == State::GET_KEY_LIST_VALUE)
    //                 {
    //                     JSON_CHANGE_STATE(State::AQUIRED_KEY_VALUE);
    //                 }
    //                 else if (state == State::GET_KEY_NAME)
    //                 {
    //                     JSON_CHANGE_STATE(State::ERROR);
    //                     printlne("Missing closing \" while aquiring string value for key list");
    //                 }
    //                 else if (state == State::GET_KEY_MAP_VALUE)
    //                 {
    //                     JSON_CHANGE_STATE(State::AQUIRED_CLOSING_TOKEN);
    //                     println("RETURNING LIST 2");
    //                     return {currentNode, ""};
    //                 }
    //                 break;

    //             case '"': {
    //                 /* Valid { needs to be continued by ", ignore spaces and new lines. */
    //                 if (state == State::AQUIRED_OPENING_CURLY_TOKEN || state == State::AQUIRED_OPENING_SQUARE_TOKEN)
    //                 {
    //                     JSON_CHANGE_STATE(State::GET_KEY_NAME);
    //                 }
    //                 /* After we accumulated the pair's key value, be sure there's a " to close the key.*/
    //                 else if (state == State::GET_KEY_NAME)
    //                 {
    //                     if (keyNameAcc.empty())
    //                     {
    //                         JSON_CHANGE_STATE(State::ERROR);
    //                         printlne("No name gathered. Missplaced start \" perhaps or really no name.");
    //                     }
    //                     else
    //                     {
    //                         JSON_CHANGE_STATE(State::AQUIRED_KEY_NAME);
    //                         aquiredOpeningTokenDueToComma = false;
    //                         printlne("name is: %s", keyNameAcc.c_str());

    //                         // node->value[keyNameAcc] = FieldValue{};
    //                     }
    //                 }
    //                 /* If we gather " before any opening token, we have a problem.*/
    //                 else if (state == State::GET_OPENING_TOKEN)
    //                 {
    //                     JSON_CHANGE_STATE(State::ERROR);
    //                     printlne("Sub-object must start with { or [ , not %c", ch);
    //                 }
    //                 /* On this state, hitting " means we need to gather value of key which is a string */
    //                 else if (state == State::GET_KEY_VALUE)
    //                 {
    //                     JSON_CHANGE_STATE(State::GET_KEY_STRING_VALUE);
    //                 }
    //                 /* Hitting " on this state means we gathered the key's value string. */
    //                 else if (state == State::GET_KEY_STRING_VALUE)
    //                 {
    //                     JSON_CHANGE_STATE(State::AQUIRED_KEY_VALUE);
    //                     printlne("value is: %s", charAcc.c_str());

    //                     if (!currentNode)
    //                     {
    //                         currentNode = std::make_shared<JsonNode>(JsonObjectNode{});
    //                     }
    //                     std::get<JsonObjectNode>(*currentNode)[keyNameAcc] = charAcc;

    //                     charAcc.clear();
    //                     keyNameAcc.clear();
    //                 }
    //                 /* Hitting " on this state the : separator is missing */
    //                 else if (state == State::AQUIRED_KEY_NAME)
    //                 {
    //                     JSON_CHANGE_STATE(State::ERROR);
    //                     printlne("Missing : between key and value");
    //                 }
    //                 /* Hitting " on this state the we forgot comma between pairs */
    //                 else if (state == State::AQUIRED_KEY_VALUE)
    //                 {
    //                     JSON_CHANGE_STATE(State::ERROR);
    //                     printlne("Missing , between KV pairs");
    //                 }
    //                 break;
    //             }
    //             case ':': {
    //                 /* If we gather : while getting key's name, mean there's " missing after name end. */
    //                 if (state == State::GET_KEY_NAME)
    //                 {
    //                     JSON_CHANGE_STATE(State::ERROR);
    //                     printlne("Missing end \" for name: %s .", charAcc.c_str());
    //                 }
    //                 /* If we aquired key name, switch to get it's value when encountering " */
    //                 else if (state == State::AQUIRED_KEY_NAME)
    //                 {
    //                     JSON_CHANGE_STATE(State::GET_KEY_VALUE);
    //                 }
    //                 /* If we gather : but there's nothing accumulated, means the key is empty, not good. */
    //                 else if (charAcc.empty())
    //                 {
    //                     JSON_CHANGE_STATE(State::ERROR);
    //                     printlne("No name gathered. Missplaced \" perhaps or really no name.");
    //                 }
    //                 break;
    //             }
    //             case ',': {
    //                 /* Getting to a , at this point means the object has more comma separated entries to be
    //                 processed.
    //                  * Switch to the point in time where we aquired the opening token.*/
    //                 if (state == State::AQUIRED_KEY_VALUE)
    //                 {
    //                     JSON_CHANGE_STATE(State::AQUIRED_OPENING_CURLY_TOKEN);
    //                     aquiredOpeningTokenDueToComma = true;
    //                     break;
    //                 }
    //                 else if (state == State::AQUIRED_KEY_NAME)
    //                 {
    //                     /* If its the curly bracket opener, then we have a problem. */
    //                     if (isCurly)
    //                     {
    //                         printlne("Unexpected , after key name");
    //                         JSON_CHANGE_STATE(State::ERROR);
    //                     }
    //                     /* Otherwise it's just comma between list elements */
    //                     else
    //                     {
    //                         if (!currentNode)
    //                         {
    //                             currentNode = std::make_shared<JsonNode>(JsonListNode{});
    //                         }
    //                         std::get<JsonListNode>(*currentNode).push_back(keyNameAcc);
    //                         keyNameAcc.clear();
    //                         charAcc.clear();
    //                         JSON_CHANGE_STATE(State::AQUIRED_OPENING_SQUARE_TOKEN);
    //                     }
    //                     break;
    //                 }
    //                 // else if (state == State::GET_KEY_MAP_VALUE)
    //                 // {
    //                 //     printlne("Unexpected , after key name list");
    //                 //     JSON_CHANGE_STATE(State::ERROR);
    //                 //     break;
    //                 // }

    //                 /* Justified fall. Collect comma into accumulator. */
    //                 [[fallthrough]];
    //             }
    //             default:
    //                 /* If we aquired opening token but we don't hit " as next char, we have a problem. */
    //                 if (state == State::AQUIRED_OPENING_CURLY_TOKEN || state == State::AQUIRED_OPENING_SQUARE_TOKEN)
    //                 {
    //                     JSON_CHANGE_STATE(State::ERROR);
    //                     printlne("Missing start \" before aquiring name");
    //                 }
    //                 /* If we are in the middle of getting the opening token but we encounter non-special char, we
    //                 have a
    //                  * problem. */
    //                 else if (state == State::GET_OPENING_TOKEN)
    //                 {
    //                     JSON_CHANGE_STATE(State::ERROR);
    //                     printlne("Sub-object must start with { or [ , not %c", ch);
    //                 }
    //                 /* If we get here, means that after key's : we hit a non special char, but maybe "null" token
    //                 comes
    //                  * next. */
    //                 else if (state == State::GET_KEY_VALUE)
    //                 {
    //                     /* Maybe "null" value is next*/
    //                     bool isNullValue{false};
    //                     if (ch == 'n')
    //                     {
    //                         int8_t readBackChars{0};
    //                         if (utils::read1(stream) == 'u')
    //                         {
    //                             readBackChars++;
    //                             if (utils::read1(stream) == 'l')
    //                             {
    //                                 readBackChars++;
    //                                 if (utils::read1(stream) == 'l')
    //                                 {
    //                                     readBackChars++;
    //                                     isNullValue = true;
    //                                 }
    //                             }
    //                         }
    //                     }

    //                     if (isNullValue)
    //                     {
    //                         printlne("value is null");
    //                         if (!currentNode)
    //                         {
    //                             currentNode = std::make_shared<JsonNode>(JsonObjectNode{});
    //                         }
    //                         std::get<JsonObjectNode>(*currentNode)[keyNameAcc] = charAcc;

    //                         charAcc.clear();
    //                         keyNameAcc.clear();

    //                         JSON_CHANGE_STATE(State::AQUIRED_KEY_VALUE);
    //                         break;
    //                     }

    //                     /* Otherwise error, if it was a string, it would of caught " and changed states. */
    //                     JSON_CHANGE_STATE(State::ERROR);
    //                     printlne("Key's value seems to be a string but start \" is missing");
    //                 }
    //                 /* Accumulate the key's value */
    //                 else if (state == State::GET_KEY_NAME)
    //                 {
    //                     keyNameAcc += ch;
    //                 }
    //                 /* Accumulate the key's string value. */
    //                 else if (state == State::GET_KEY_STRING_VALUE)
    //                 {
    //                     charAcc += ch;
    //                 }
    //                 /* There's a token other than , or } in this case, bad.*/
    //                 else if (state == State::AQUIRED_KEY_VALUE)
    //                 {
    //                     JSON_CHANGE_STATE(State::ERROR);
    //                     printlne("Unexpected character after aquring the key's value");
    //                 }
    //                 break;
    //         }
    //     }

    //     /* Handle case in which stream suddenly ends with no closing } */
    //     if (state != State::ERROR && state != State::AQUIRED_CLOSING_TOKEN)
    //     {
    //         printlne("Closing token } has not been found.");
    //         JSON_CHANGE_STATE(State::ERROR);
    //     }

    //     /* We shall never get here if no errors */
    //     return {nullptr, "very bad error"};
    // }

    void printJson(const JsonNode& node)
    {
        if (std::holds_alternative<JsonObjectNode>(node))
        {
            printJsonObject(std::get<JsonObjectNode>(node), 0);
        }
        else if (std::holds_alternative<JsonListNode>(node))
        {
            printJsonList(std::get<JsonListNode>(node), 0);
        }
        // else
        // {
        //     printf("what");
        // }
    }

    void printJsonObject(const JsonObjectNode& objNode, uint32_t depth = 0)
    {
        printf("{");
        const auto& kvMap = objNode;
        const uint64_t mapSize = kvMap.size();
        for (uint32_t i{0}; const auto& [k, v] : kvMap)
        {
            if (std::holds_alternative<std::string>(v))
            {
                printf("\"%s\":\"%s\"", k.c_str(), std::get<std::string>(v).c_str());
            }
            if (std::holds_alternative<double>(v))
            {
                printf("\"%s\":%lf", k.c_str(), std::get<double>(v));
            }
            else if (std::holds_alternative<JsonObjectNode>(v))
            {
                printf("\"%s\":", k.c_str());
                printJsonObject(std::get<JsonObjectNode>(v), depth + 1);
            }
            else if (std::holds_alternative<JsonListNode>(v))
            {
                printf("\"%s\":", k.c_str());
                printJsonList(std::get<JsonListNode>(v), depth + 1);
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
            if (std::holds_alternative<std::string>(v))
            {
                printf("\"%s\"", std::get<std::string>(v).c_str());
            }
            if (std::holds_alternative<double>(v))
            {
                printf("%lf", std::get<double>(v));
            }
            else if (std::holds_alternative<JsonObjectNode>(v))
            {
                // printf("\"%s\":", k.c_str());
                printJsonObject(std::get<JsonObjectNode>(v), depth + 1);
            }
            else if (std::holds_alternative<JsonListNode>(v))
            {
                // printf("\"%s\":", k.c_str());
                printJsonList(std::get<JsonListNode>(v), depth + 1);
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
            println("[%d] State is already %s", line, getStateString(newState).c_str());
            return;
        }
        println("[%d] Switching from %s to %s", line, getStateString(state).c_str(), getStateString(newState).c_str());
        state = newState;
    }

    std::string getStateString(const State& state)
    {
#define STATE_CASE(x)                                                                                                  \
    case x:                                                                                                            \
        return #x

        switch (state)
        {
            STATE_CASE(State::IDLE);
            STATE_CASE(State::AQUIRED_OPENING_CURLY_TOKEN);
            STATE_CASE(State::AQUIRED_OPENING_SQUARE_TOKEN);
            STATE_CASE(State::GET_CLOSING_CURLY_TOKEN);
            STATE_CASE(State::GET_CLOSING_SQUARE_TOKEN);
            STATE_CASE(State::AQUIRED_CLOSING_TOKEN);
            STATE_CASE(State::GET_KEY_NAME);
            STATE_CASE(State::AQUIRED_KEY_NAME);
            STATE_CASE(State::GET_KEY_VALUE);
            STATE_CASE(State::AQUIRED_KEY_VALUE);
            STATE_CASE(State::GET_KEY_STRING_VALUE);
            STATE_CASE(State::GET_KEY_MAP_VALUE);
            STATE_CASE(State::GET_KEY_LIST_VALUE);

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
        }
#undef STATE_CASE
        return "<state unknown>";
    }
};

} // namespace hk
int main(int argc, char** argv)
{
    hk::Json json;
    json.loadFrom("/home/hekapoo/Documents/probe/json/test.json");
    return 0;
}