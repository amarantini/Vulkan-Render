//
//  arg_parser.h
//  VulkanTesting
//
//  Created by qiru hu on 1/29/24.
//

#ifndef arg_parser_h
#define arg_parser_h

#include <cassert>
#include <stdexcept>
#include <unordered_map>
#include <string>
#include <unordered_set>
#include <memory>

struct ArgValue {
    std::vector<std::string> vals;
    bool required;
    bool supplied;
    int nargs;
    std::vector<std::string> accepted_vals;
    ArgValue(const bool required, 
            const int nargs,
            const std::string& default_val,
            const std::vector<std::string>& accepted_vals):
        required(required), supplied(false), nargs(nargs), accepted_vals(accepted_vals)  {
            vals.resize(nargs);
            if(!default_val.empty()){
                assert(nargs>=1);
                vals[0] = default_val;
                supplied = true;
            }
        }
};

class ArgParser {
private:
    std::unordered_map<std::string,std::unique_ptr<ArgValue> > options;

public:
    void add_option(const std::string& opt_name, 
                    bool required,
                    const std::string& default_val = "",
                    int nargs = 1,
                    const std::vector<std::string> accepted_vals = std::vector<std::string>()){
        options[opt_name] = std::make_unique<ArgValue>(required, nargs, default_val, accepted_vals);
    }

    void parse(int argc, char* argv[]){
        for(size_t i=1; i<argc; i++){
            char* arg = argv[i];
            if(options.count(arg)){
                int& nagrs = options[arg]->nargs;
                std::vector<std::string>& accepted_vals = options[arg]->accepted_vals;
                std::vector<std::string>& vals = options[arg]->vals;
                for(size_t k=0; k<nagrs; k++){
                    vals[k] = argv[++i];
                    if(!accepted_vals.empty() && 
                        std::find(accepted_vals.begin(),accepted_vals.end(),vals[k]) == accepted_vals.end()) {
                        throw std::runtime_error("Invalid argument value: "+std::string(vals[k])+"\n");
                    }
                }
                options[arg]->supplied = true;
            } else {
                throw std::runtime_error("Invalid argument: "+std::string(arg)+"\n");
            }
        }
        for(auto& p:options){
            if(p.second->required && !p.second->supplied){
                throw std::runtime_error(p.first+" required but missing\n");
            }
        }
    }

    const std::vector<std::string>* get_option(const std::string& opt_name) {
        if(!options.count(opt_name)){
            throw std::runtime_error("Invalid argument name: "+std::string(opt_name)+"\n");
        }
        if(options[opt_name]->supplied)
            return &options[opt_name]->vals;
        else
            return nullptr;
    }
};

#endif /* arg_parser_h */
