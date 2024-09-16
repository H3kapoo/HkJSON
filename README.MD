# Simple Json File Parser in C++

### Requirements
 - CMake installed.
 - C++17 or higher (change inside CMakeLists.txt). 
 - Ninja make generator (but you can change and use any other generator).

### Compile

```bash
./build.sh
```

### Usage
Please check ```src/main.cpp```.

```cpp
    hk::Json json;
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
```
### Notes
 - Windows/MacOS not supported in CMakeLists but if you know what to put there, go on. No posix dependencies.