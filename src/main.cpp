#include "HkJson.hpp"
#include "Utility.hpp"

int main(int, char**)
{
    using namespace hk;

    Json json;
    // Json::JsonResult result = json.loadFromFile("file");
    Json::JsonResult result = json.loadFromString(R"(
    [
        {
            "releaseYear": 2021,
            "supportedPlatforms": [
                "Linux",
                "Windows",
                "macOS"
            ],
            "category": "Software",
            "subscriptionBased": "yes",
            "name": "CloudInfrastructure",
            "price": 999.99,
            "id": 1001
        }
    ]
    )");

    if (!result.error.empty())
    {
        printlne("%s", result.error.c_str());
        return 1;
    }

    Json::JsonRootNode& obj = *result.json;

    if (!obj.isList())
    {
        return 2;
    }

    if (obj.getList().size() < 1)
    {
        return 3;
    }

    if (!obj[0].isObject())
    {
        return 4;
    }

    Json::JsonObjectNode& theObject = obj[0].getObject();

    // addition of arbitrary values at runtime/compile time is possible
    theObject["some_key"] = Json::JsonListNode{
        Json::JsonListNode{34, "something"}, Json::JsonListNode{Json::JsonNull{}}};

    // iteration like on a normal container, which type checking
    for (const auto& [k, v] : theObject)
    {
        if (v.isString())
        {
            println("kv: {%s : %s}", k.c_str(), v.getString().c_str());
        }
    }

    // json printing
    json.printJson(*result.json);
    printf("\n");

    return 0;
}