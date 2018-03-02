#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

struct arg_opt {
    std::string_view name;
    std::string_view description;
    int num_params;
};

inline bool operator<(const arg_opt &l, const arg_opt &r)
{
    return l.name < r.name;
}

struct parsed_opt {
    std::string value;
    bool empty;

    template<typename T>
    T get(T default_value)
    {
        if (empty)
            return default_value;
        return (T) value;
    }
};

template<>
int parsed_opt::get(int default_value)
{
    if (empty)
        return default_value;
    return std::stoi(value);
}

template<>
float parsed_opt::get(float default_value)
{
    if (empty)
        return default_value;
    return std::stof(value);
}

template<>
double parsed_opt::get(double default_value)
{
    if (empty)
        return default_value;
    return std::stod(value);
}

template<>
std::string parsed_opt::get(std::string default_value)
{
    if (empty)
        return default_value;
    return value;
}

template<>
bool parsed_opt::get(bool)
{
    return !empty;
}

class arg_parser
{
private:
    std::set<arg_opt> arg_set;
    std::map<arg_opt, parsed_opt> parsed_options;
    std::string_view description;
    size_t max_name_length;

public:
    arg_parser(std::string_view program_description)
        : description{program_description}, max_name_length{0}
    {
    }

    inline void add_arg(arg_opt option)
    {
        arg_set.insert(option);
        max_name_length = std::max(max_name_length, option.name.size());
    }

    inline void parse(int argc, char *argv[])
    {
        for (int i = 1; i < argc; i++) {
            std::string_view view{argv[i]};
            auto it = arg_set.find({view, "", 0});
            if (it != arg_set.end()) {
                int n = it->num_params;
                parsed_opt parsed;
                parsed.empty = false;
                if (n-- && i + 1 < argc) {
                    parsed.value = argv[++i];
                }
                parsed_options[*it] = parsed;
            }
        }
    }

    inline parsed_opt find(std::string_view param)
    {
        auto it = parsed_options.find({param, "", 0});
        if (it != parsed_options.end()) {
            return it->second;
        }
        return {"", true};
    }

    inline void show_help()
    {
        std::cout << description << "\n";
        for (auto &option : arg_set) {
            std::cout << "  " << std::left << std::setw(max_name_length + 2) << option.name
                      << option.description << "\n";
        }
        std::cout << std::internal;
    }
};
