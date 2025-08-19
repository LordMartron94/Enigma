#include <stdio.h>
#include "enigma/enigma.h"

/* Simple example API impl */
int enigma_add(int a, int b) {
    return a + b;
}

const char* enigma_version_string(void) {
    /* Keep this tiny and compile-time only */
    return "0.0.0";
}
