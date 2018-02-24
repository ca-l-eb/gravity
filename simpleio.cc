#include <string.h>
#include <iostream>

#include "simpleio.h"

//
// Created by dechant on 9/1/16.
//

std::string read_file(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    auto size = ftell(fp);
    auto contents = std::string(size, '\0');

    fseek(fp, 0L, SEEK_SET);
    auto len = fread(const_cast<char *>(contents.c_str()), sizeof(char), size, fp);

    if (len != size) {
        std::cerr << "Error reading " << filename << "Got " << len << " byes, but expected " << size
                  << "." << std::endl;
    }

    fclose(fp);
    return contents;
}
