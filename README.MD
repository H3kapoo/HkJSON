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
```
### Notes
 - Windows/MacOS not supported in CMakeLists but if you know what to put there, go on. No posix dependencies.