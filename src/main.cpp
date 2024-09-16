#include "HkJson.hpp"
#include "Utility.hpp"

int main(int, char**)
{
    hk::Json json;
    // hk::Json::JsonResult result = json.loadFromFile("/home/hekapoo/Documents/probe/json/test.json");
    hk::Json::JsonResult result = json.loadFromString(R"(
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
        return -1;
    }

    json.printJson(*result.json);

    hk::Json::JsonRootNode& obj = *result.json;

    if (obj.isObject() && obj.getObject().size() > 0 && !obj[0].isObject())
    {
        return -2;
    }

    hk::Json::JsonObjectNode& theObject = obj[0].getObject();

    for (const auto& [k, v] : theObject)
    {
        if (v.isString())
        {
            println("kv: {%s : %s}", k.c_str(), v.getString().c_str());
        }
    }

    return 0;
}