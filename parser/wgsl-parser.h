#include <stdio.h>
#include <stdlib.h>

typedef struct {
    
    int binding;
    char *type;
    int group;

} BindingInfo;

BindingInfo *parse_wgsl(const char *filename, int *num_bindings);

// File IO
// -----------------------------------------------
// Function to read a shader into a string
char *read_file(const char *filename);

