#include "wgsl-parser.h"
#include <stdint.h>
#include <stdio.h>

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

// WGSL Parser
// -----------------------------------------------

// Helper function to parse the workgroup size and function name from the shader
int parse_function_decl(char *shader_code, int *workgroup_sizes,
                        char *function_name) {
    if (!shader_code) {
        printf("Error -> parse_function_decl(): Shader code is NULL\n");
        return -1;
    }

    // Define patterns for different numbers of workgroup sizes
    const char *patterns[] = {
        "@compute\\s*@workgroup_size\\((\\d+)\\)\\s*fn\\s+([A-Za-z0-9_]+)",
        "@compute\\s*@workgroup_size\\((\\d+),\\s*(\\d+)\\)\\s*fn\\s+([A-Za-z0-"
        "9_]+)",
        "@compute\\s*@workgroup_size\\((\\d+),\\s*(\\d+),\\s*(\\d+)\\)\\s*"
        "fn\\s+([A-Za-z0-9_]+)"};

    struct slre_cap caps[4]; // Up to three numbers and the function name
    int match_length;
    for (int i = 0; i < ((int)(sizeof(patterns) / sizeof(patterns[0]))); i++) {
        match_length = slre_match(patterns[i], shader_code,
                                  (int)strlen(shader_code), caps, 4, 0);
        if (match_length > 0) { // Match found
            // Extract the function name from the last capture group
            int fn_index = i + 1; // Function name index depends on the number
                                  // of workgroup sizes
            memcpy(function_name, caps[fn_index].ptr,
                   (size_t)caps[fn_index].len);
            function_name[caps[fn_index].len] = '\0';

            // Extract workgroup sizes
            for (int j = 0; j <= i; j++) {
                if (caps[j].len > 0) {
                    char size_str[10];
                    memcpy(size_str, caps[j].ptr, (size_t)caps[j].len);
                    size_str[caps[j].len] = '\0';
                    workgroup_sizes[j] = atoi(size_str);
                }
            }
            return 1; // Success
        }
    }

    printf("Error -> parse_function_decl(): No match found\n");
    return -1;
}

// Look for the binding definitions in the shader code and parse them
// into the ComputeInfo struct
int parse_binding_defs(char *shader_code, ComputeInfo *info) {

    if (!shader_code) {
        printf("Error -> parse_binding_defs(): Shader code is NULL\n");
        return -1;
    }

    if (!info) {
        printf("Error -> parse_binding_defs(): ComputeInfo is NULL\n");
        return -1;
    }

    // Save the start of the shader code for resetting the pointer
    char *shader_code_start = shader_code;
    int match_length;
    int num_found = 0;
    const char *patterns[4] = {
        "@group\\((\\d+)\\)\\s*@binding\\((\\d+)\\)\\s*texture<([^>]+)>",
        "@group\\((\\d+)\\)\\s*@binding\\((\\d+)\\)\\s*var<([^>]+)>",
        "\\[\\[group\\((\\d+)\\), binding\\((\\d+)\\)\\]]\\s*var<([^>]+)>",
        "\\[\\[group\\((\\d+)\\), binding\\((\\d+)\\)\\]]\\s*texture<([^>]+)>"};

    // Iterate until all patterns are tested.
    // If no pattern is found, return -1
    for (int i = 0; i < ((int)(sizeof(patterns) / sizeof(patterns[0]))); i++) {
        // (group) (binding) (usage)
        struct slre_cap caps[3];
        // Iterate through the shader code and find all matches of the pattern
        while (
            (match_length = slre_match(patterns[i], shader_code,
                                       (int)strlen(shader_code), caps, 3, 0))) {
            if (match_length > 0 &&
                num_found < MAX_GROUPS) { // Check bounds for array
                uint32_t group = (uint32_t)atoi(caps[0].ptr);
                uint32_t binding = (uint32_t)atoi(caps[1].ptr);
                strncpy(info->groups[group].bindings[binding].usage,
                        caps[2].ptr, (size_t)caps[2].len);
                info->groups[group].bindings[binding].usage[caps[2].len] =
                    '\0'; // Null-terminate the string
                num_found++;
                info->groups[group].num_bindings++;
                info->groups[group].bindings[binding].binding = binding;
                // Move the start point past the last match
                shader_code += caps[0].ptr - shader_code + caps[0].len;
            } else {
                // No more matches found, go to the next pattern
                break;
            }
        }
        // Reset the shader code pointer to the start for the next pattern
        shader_code = shader_code_start;
    }

    return 1;
}

// determine the compute shader entrypoint, workgroup size, and binding layout
int parse_wgsl_compute(char *shader_code, ComputeInfo *info) {

    if (!shader_code) {
        printf("Error -> parse_wgsl(): Shader code is NULL\n");
        return -1;
    }

    if (!info) {
        printf("Error -> parse_wgsl(): ComputeInfo is not initialized\n");
        return -1;
    }

    // Extract matching results
    char entry[256];

    // Get information about the compute function
    int status = parse_function_decl(shader_code, info->workgroup_size, entry);
    if (status < 0) {
        printf("Error -> parse_wgsl(): Could not parse function declaration\n");
        return -1;
    }
    strncpy(info->entry, entry, strlen(entry));

    // Initialize the group layout data
    for (int i = 0; i < MAX_GROUPS; i++) {
        info->groups[i].group = (uint32_t)i;
        info->groups[i].num_bindings = 0;
    }

    // Determine the number of groups for our binding layouts
    status = parse_binding_defs(shader_code, info);
    if (status < 0) {
        printf("Error -> parse_wgsl(): Could not parse binding definitions\n");
        return -1;
    }

    return status;
}

// Validate the compute info struct to ensure that the binding layout is correct
int validate_compute(ComputeInfo *info) {
    // Validate the bindgroup layout. If a group has no bindings, we can skip it
    // and move on to the next group

    // 1. Check if a group has no bindings
    // 2. Check each binding of the group for the following:
    //   a. Binding index is out of bounds
    //   b. Binding index does not match the current index
    //   c. Group index does not match the current group index
    //   d. Binding usage is empty
    //   e. Binding usage(s) are not one of:
    //      storage, uniform, read_write, read
    for (uint32_t i = 0; i < MAX_GROUPS; i++) {
        if (info->groups[i].num_bindings == 0) {
            continue;
        }

        int num_bindings = info->groups[i].num_bindings;

        /*
        Error bitmask
        0000 0001 -> Binding index out of bounds
        0000 0010 -> Binding index mismatch
        0000 0100 -> Group index mismatch
        0000 1000 -> Binding usage is empty
        0001 0000 -> Invalid binding usage
        0000 0000 -> No error
        */

        uint8_t error_mask = 0;
        for (uint32_t j = 0; j < (uint32_t)num_bindings; j++) {
            BindingInfo binding = info->groups[i].bindings[j];
            if (binding.binding >= MAX_BINDINGS) {
                error_mask |= 0x01;
            }
            if (binding.binding != j) {
                error_mask |= 0x02;
            }
            if (binding.group != i) {
                error_mask |= 0x04;
            }
            if (strlen(binding.usage) == 0 || strlen(binding.usage) > 50) {
                error_mask |= 0x08;
                break;
            }

            // Make a copy of the usage string to tokenize it without ruining
            // the pointer
            char usage_copy[51];
            strncpy(usage_copy, binding.usage, strlen(binding.usage));
            usage_copy[strlen(binding.usage)] = '\0';

            // Tokenize the usage string and check if all usages are valid
            char *token = strtok(usage_copy, ", ");
            while (token != NULL) {
                if (strcmp(token, "storage") != 0 &&
                    strcmp(token, "uniform") != 0 &&
                    strcmp(token, "read_write") != 0 &&
                    strcmp(token, "read") != 0) {
                    error_mask |= 0x10;
                    break;
                }
                token = strtok(NULL, ", ");
            }

            if (error_mask != 0) {
                printf("Error -> validate_compute(): Group %d has invalid "
                       "binding %d\n",
                       i, binding.binding);
                if (error_mask & 0x01) {
                    printf("\tBinding index out of bounds\n");
                }
                if (error_mask & 0x02) {
                    printf("\tBinding index mismatch\n");
                }
                if (error_mask & 0x04) {
                    printf("\tGroup index mismatch\n");
                }
                if (error_mask & 0x08) {
                    printf("\tBinding usage is empty OR too long\n");
                }
                if (error_mask & 0x10) {
                    printf(
                        "\tInvalid binding usage\n\t\tValid usages: storage, "
                        "uniform, read_write, read\n\t\tMulti-usage "
                        "traits must be separated by commas\n");
                }
                return -1;
            }
        }
    }
    return 1;
}

// Print the compute info struct
void print_compute_info(ComputeInfo *info) {
    if (!info) {
        printf("Error -> print_compute_info(): ComputeInfo is NULL\n");
        return;
    }

    printf("Entry: %s\n", info->entry);
    printf("Workgroup size: (%d, %d, %d)\n", info->workgroup_size[0],
           info->workgroup_size[1], info->workgroup_size[2]);

    // Iterate through each group and print bindings if active
    for (int i = 0; i < MAX_GROUPS; i++) {
        const GroupInfo group = info->groups[i];
        if (group.num_bindings > 0) {
            printf("Group %d:\n", group.group);
            for (int j = 0; j < group.num_bindings; j++) {
                const BindingInfo binding = group.bindings[j];
                printf("  Binding %d: Usage = %s\n", binding.binding,
                       binding.usage);
            }
        }
    }
}
