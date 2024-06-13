#include "wgsl-parser.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define ASSET_PATH "/home/kyle/Projects/fips-projects/parse-wgsl/assets/%s"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    if (strlen(argv[1]) > 255) {
        fprintf(stderr, "Filename too long\n");
        return 1;
    }

    // Print current working directory in order to understand where the program
    // is looking for the file
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working dir: %s\n", cwd);
    } else {
        perror("getcwd() error");
        return 1;
    }

    char *filename = argv[1];
    char *shader;

    char path[256];
    snprintf(path, 256, ASSET_PATH, filename);
    printf("Path: %s\n", path);

    shader = read_file(path);

    if (!shader) {
        fprintf(stderr, "Could not read file %s\n", filename);
        return 1;
    }

    printf("Shader:\n%s\n", shader);

    free(shader);

    return 0;
}
