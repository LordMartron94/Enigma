#include <stdio.h>
#include <stdlib.h>
#include "enigma/enigma.h"

int main(void) {
    int ok = 1;

    if (enigma_version_string() == NULL) {
        fprintf(stderr, "version_string() returned NULL\n");
        ok = 0;
    }

    if (!ok) {
        return EXIT_FAILURE;
    }

    puts("basic test passed");
    return EXIT_SUCCESS;
}
