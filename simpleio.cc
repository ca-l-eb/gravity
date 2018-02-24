#include <string.h>
#include <iostream>

#include "simpleio.h"

//
// Created by dechant on 9/1/16.
//

char *read_file(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    size_t size = ftell(fp);
    char *buffer = new char[size + 1];

    fseek(fp, 0L, SEEK_SET);
    size_t len = fread(buffer, sizeof(char), size, fp);
    buffer[len] = '\0';

    if (len != size) {
        std::cerr << "Error reading " << filename << "Got " << len << " byes, but expected " << size
                  << "." << std::endl;
    }

    fclose(fp);
    return buffer;
}