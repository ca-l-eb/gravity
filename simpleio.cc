#include <fstream>

#include "simpleio.h"

//
// Created by dechant on 9/1/16.
//

std::string read_file(const char *filename)
{
    auto fs = std::ifstream{filename};
    std::string contents;

    fs.seekg(0, std::ios::end);
    contents.reserve(fs.tellg());
    fs.seekg(0, std::ios::beg);

    contents.assign((std::istreambuf_iterator<char>(fs)), std::istreambuf_iterator<char>());
    return contents;
}
