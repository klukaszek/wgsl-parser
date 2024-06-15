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

    pcre *re;
    const char *error;
    int erroffset;
    int ovector[OVECCOUNT];
    int rc;

    char *pattern = "@compute\\s*@workgroup_size\\((\\d+)(?:,\\s*(\\d+))?(?:,"
                    "\\s*(\\d+))?\\)\\s*fn\\s+(\\w+)";

    // Compile the regex
    re = pcre_compile(pattern, 0, &error, &erroffset, NULL);
    if (re == NULL) {
        printf("Error -> parse_function_decl(): PCRE compilation failed at "
               "offset %d: %s\n",
               erroffset, error);
        return -1;
    }

    // Execute the regex
    rc = pcre_exec(re, NULL, shader_code, strlen(shader_code), 0, 0, ovector,
                   OVECCOUNT);
    if (rc < 0) { // Check for errors
        if (rc != PCRE_ERROR_NOMATCH)
            printf("Error -> parse_function_decl(): Matching error %d\n", rc);
        pcre_free(re);
        return -1;
    }

    // Iterate over the matching results and assign them to the info struct
    // This is a very simple parser, it assumes that the shader code is
    // correctly formatted and that the regex will always match.
    for (int i = 0; i < rc; i++) {
        const char *substring_start;
        int substring_length;
        // Since we know the order of the capture groups we can extract the data
        // in sequence
        pcre_get_substring(shader_code, ovector, rc, i, &substring_start);
        substring_length = ovector[2 * i + 1] - ovector[2 * i];
        if (i > 0 && i < 4 && substring_start) {
            workgroup_sizes[i - 1] = atoi(substring_start);
        }
        if (i == 4) {
            strncpy(function_name, substring_start, substring_length);
            function_name[substring_length] = '\0';
        }
        pcre_free_substring(substring_start);
    }

    pcre_free(re);
    return 1;
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

    const char *pattern =
        "@group\\((\\d+)\\)\\s*@binding\\((\\d+)\\)\\s*var<([^>]+)>\\s*[^;]+;";
    const char *error;
    int erroffset;
    int ovector[OVECCOUNT]; // Assuming OVECCOUNT is defined properly
    int rc;

    pcre *re = pcre_compile(pattern, PCRE_MULTILINE, &error, &erroffset, NULL);
    if (!re) {
        printf("Regex compilation failed: %s\n", error);
        return -1;
    }

    int offset = 0;
    int len = strlen(shader_code);
    // Iterate over the matches and get the binding information
    while ((rc = pcre_exec(re, NULL, shader_code, len, offset, 0, ovector,
                           OVECCOUNT)) >= 0) {
        // Extract group number, and binding number
        int group_num = atoi(shader_code + ovector[2 * 1]);
        int binding_num = atoi(shader_code + ovector[2 * 2]);

        // Extract binding type specifier (storage, uniform, etc.)
        char type[256];
        strncpy(type, shader_code + ovector[2 * 3],
                ovector[2 * 3 + 1] - ovector[2 * 3]);
        type[ovector[2 * 3 + 1] - ovector[2 * 3]] = '\0';

        // Update the binding information for the respective group
        if (group_num < MAX_GROUPS &&
            info->groups[group_num].num_bindings < MAX_BINDINGS) {
            BindingInfo *binding =
                &info->groups[group_num]
                     .bindings[info->groups[group_num].num_bindings++];
            binding->binding = binding_num;
            strncpy(binding->type, type, strlen(type));
        }
        offset = ovector[1]; // Update offset to search for the next match
    }

    pcre_free(re);
    return 1;
}

// determine the compute shader entrypoint, workgroup size, and binding layout
int parse_wgsl(char *shader_code, ComputeInfo *info) {

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

    return 1;
}

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
