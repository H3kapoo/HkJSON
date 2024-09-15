#include "HkJson.hpp"
#include "Utility.hpp"

int main(int argc, char** argv)
{
    hk::Json json;
    hk::Json::JsonResult result = json.loadFromFile("/home/hekapoo/Documents/probe/json/test.json");

    if (result.error.empty() && result.json != nullptr)
    {
        json.printJson(*result.json);
        printf("\n");

        hk::Json::JsonRootNode& obj = *result.json;

        try
        {
            // JsonObjectNode obj;
            // obj["ce"] = JsonListNode{JsonListNode{}, JsonListNode{1, 32}, JsonNull{},
            //     JsonObjectNode{{"ceva", JsonListNode{}}}, 34.4, 34.44, 34.4453};

            // printJsonObject(obj);
            // // printJson(obj);
            // printf("data: %s\n", obj["items"][0]["kind"].getString().c_str());
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
    return 0;
}