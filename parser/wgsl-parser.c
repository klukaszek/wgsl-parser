#include "wgsl-parser.h"

// File IO
// -----------------------------------------------
// Function to read a shader into a string
char *read_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Could not open file %s\n", filename);
        return NULL;
    }

    // Get the length of the file so we can allocate the correct amount of
    // memory for the shader string buffer
    fseek(file, 0, SEEK_END);
    size_t length = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    // It is important to free the buffer after using it, it might be a good
    // idea to implement an arena structure into the Nano STL for better
    // handling of dynamic memory allocations.
    char *buffer = (char *)malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    return buffer;
}
