#include "json_parser.h"
#include <memory>

struct JsonData {
    std::vector<std::optional<double>> numbers;
    std::vector<std::optional<std::string>> strings;
    std::vector<std::optional<JsonObject>> objects;
    std::vector<std::optional<std::vector<double>>> arrays;

    JsonData() = default;
};

std::optional<double> const JsonValue::as_num() const{
    static std::optional<double> const empty;
    if(type==Type::Num){
        return data->numbers[idx];
    } else {
        return empty;
    }
}

std::optional<std::string> const JsonValue::as_str() const{
    static std::optional<std::string> const empty;
    if(type==Type::Str){
        return data->strings[idx];
    } else {
        return empty;
    }
}

std::optional<JsonObject> const JsonValue::as_obj() const {
    static std::optional<JsonObject> const empty;
    if(type==Type::Obj){
        return data->objects[idx];
    } else {
        return empty;
    }
}

std::optional<std::vector<double>> const JsonValue::as_array() const{
    static std::optional<std::vector<double>> const empty;
    if(type==Type::Arr){
        return data->arrays[idx];
    } else {
        return empty;
    }
}

void JsonParser::load(const std::string& file_path, std::string& output){
    std::ifstream file(file_path);
    std::stringstream buffer;
    buffer << file.rdbuf();

    output = buffer.str();
}

JsonList JsonParser::parse(std::string input){
    std::istringstream in(input);

    data = std::make_shared<JsonData>();
    JsonList jsonList;
    char c = readChar(in);
    assert(c=='[');

    // Get first element of the array, "s72-v1"
    std::string str = parseStr(in);
    jsonList.push_back(std::make_shared<JsonValue>(data, Type::Str, data->strings.size()));
    data->strings.push_back(str);

    do {
        c = readChar(in);
        if(c==']')
            break;
        assert(c==',');
        jsonList.push_back(parseObj(in));
    } while (true);

    return jsonList;
}


std::shared_ptr<JsonValue> JsonParser::parseArr(std::istringstream& in) {
    u_int32_t idx = data->arrays.size();
    std::shared_ptr<JsonValue> value = std::make_shared<JsonValue>(data, Type::Arr, idx);
    data->arrays.emplace_back(std::in_place);
    char c = readChar(in);
    assert(c=='[');

    while(true){
        skipWhitespace(in);
        data->arrays[idx].value().push_back(parseNum(in));
        c = readChar(in);
        if(c==']')
            break;
        assert(c==',');
    }
    return value;
}

std::shared_ptr<JsonValue> JsonParser::parseObj(std::istringstream& in) {
    u_int32_t idx = data->objects.size();
    std::shared_ptr<JsonValue> root = std::make_shared<JsonValue>(data,Type::Obj, idx);
    data->objects.emplace_back(std::in_place);

    char c = readChar(in);
    assert(c=='{');
    if(in.peek()=='}'){
        c = readChar(in);
        return root;
    }
    while(true){
        std::string key = parseStr(in);
        std::shared_ptr<JsonValue> val;

        c = readChar(in);
        assert(c==':');
        skipWhitespace(in);

        c = in.peek();
        if(c=='"'){
            std::string str = parseStr(in);
            val = std::make_shared<JsonValue>(data, Type::Str, data->strings.size());
            data->strings.push_back(str);
        } else if (c=='['){
            val = parseArr(in);
        } else if (c=='{'){
            val = parseObj(in);
        } else {
            double num = parseNum(in);
            val = std::make_shared<JsonValue>(data, Type::Num, data->numbers.size());
            data->numbers.push_back(num);
        }
        data->objects[idx].value()[key] = val;

        c = readChar(in);
        if(c=='}')
            break;
        assert(c==',');
        skipWhitespace(in);
    } 

    return root;
}