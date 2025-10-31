#include <ctype.h>   // for isspace()
#include <string.h>  // for strlen()

void trimm(char *str) {
    char *end;
    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0)  // All spaces?
        return;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end + 1) = 0;
}

