//
//  json_parser.h
//  VulkanTesting
//
//  Created by qiru hu on 1/31/24.
//  Referenced https://github.com/ixchow/sejp/tree/main

#ifndef json_parser_h
#define json_parser_h

#include <_ctype.h>
#include <cassert>
#include <exception>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <optional>
#include <unordered_map>
#include <utility>

#include <iostream>

enum class Type {Num, Str, Obj, Arr};

struct JsonData;
struct JsonValue {
    std::shared_ptr< JsonData const > data;
    Type type;
    u_int32_t idx;

    JsonValue(std::shared_ptr< JsonData const > _data, Type _type, u_int32_t _idx):
    data(_data), type(_type), idx(_idx) {}

    std::optional<double> const as_num() const;
    std::optional<std::string> const as_str() const;
    std::optional<std::unordered_map<std::string, std::shared_ptr<JsonValue>>> const as_obj() const;
    std::optional<std::vector<double>> const as_array() const;
};
typedef std::vector<std::shared_ptr<JsonValue> > JsonList;
typedef std::unordered_map<std::string, std::shared_ptr<JsonValue> > JsonObject;

class JsonParser {
public:
    void load(const std::string& file_path, std::string& output);
    JsonList parse(std::string input);

private:
    std::shared_ptr<JsonData> data;

    
    std::shared_ptr<JsonValue> parseArr(std::istringstream& in);
    std::shared_ptr<JsonValue> parseObj(std::istringstream& in);

    void skipWhitespace(std::istringstream& in){
        while(true){
            std::istream::int_type c = in.peek();
            if(c == ' ' || c == '\t' || c == '\n' || c == '\r'){
                in.get();
            } else {
                break;
            }
        }
    }

    char readChar(std::istringstream& in) {
        skipWhitespace(in);
        char c;
        in.get(c);
        return c;
    }

    double parseNum(std::istringstream& in){
        std::string str = "";
        while(true){
            std::istream::int_type c = in.peek();
            if(('0' <= c && c <= '9') || c == 'e' || c == '.' || c=='+' || c=='-'){
                str += readChar(in);
            } else {
                break;
            }
        }
        return std::stod(str);
    };

    std::string parseStr(std::istringstream& in){
        char c = readChar(in);
        assert(c=='"');
        std::string str = "";
        while(true){
            c = readChar(in);
            if(c!='"'){
                str += char(c);
            } else {
                break;
            }
        }
        return str;
    };
};





#endif /* json_parser_h */
