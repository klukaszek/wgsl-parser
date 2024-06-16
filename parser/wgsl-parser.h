#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../slre/slre.h"

#define OVECCOUNT 120

#define MAX_GROUPS 8
#define MAX_BINDINGS 8

#ifndef WGSL_PARSER_H
    #define WGSL_PARSER_H

// Represents a binding in the shader
typedef struct {
    int binding;
    char type[256];
    int group;
} BindingInfo;

// Represents a group of bindings
typedef struct {
    int group;
    BindingInfo bindings[MAX_BINDINGS];
    int num_bindings;
} GroupInfo;

// Represents a compute shader
typedef struct {
    char entry[256];
    GroupInfo groups[MAX_GROUPS];
    int workgroup_size[3];
} ComputeInfo;

// Parsing functions
// -----------------------------------------------
int parse_wgsl_compute(char *shader, ComputeInfo *info);

// File IO
// -----------------------------------------------
// Function to read a shader into a string
char *read_file(const char *filename);

// Helper functions
// -----------------------------------------------
void print_compute_info(ComputeInfo *info);

#endif
