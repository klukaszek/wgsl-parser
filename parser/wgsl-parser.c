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
            memcpy(function_name, caps[fn_index].ptr, (size_t)caps[fn_index].len);
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
        // (group) (binding) (type)
        struct slre_cap caps[3];
        // Iterate through the shader code and find all matches of the pattern
        while (
            (match_length = slre_match(patterns[i], shader_code,
                                       (int)strlen(shader_code), caps, 3, 0))) {
            if (match_length > 0 &&
                num_found < MAX_GROUPS) { // Check bounds for array
                int group = atoi(caps[0].ptr);
                int binding = atoi(caps[1].ptr);
                strncpy(info->groups[group].bindings[binding].type, caps[2].ptr,
                        (size_t)caps[2].len);
                info->groups[group].bindings[binding].type[caps[2].len] =
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
        info->groups[i].group = i;
        info->groups[i].num_bindings = 0;
    }

    // Determine the number of groups for our binding layouts
    status = parse_binding_defs(shader_code, info);

    return status;
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
                printf("  Binding %d: Type = %s\n", binding.binding,
                       binding.type);
            }
        }
    }
}
