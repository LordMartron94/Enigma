#include <stdio.h>
#include "enigma/enigma.h"

int main(void) {
    printf("[enigma] Hello from demo app!\n");
    printf("[enigma] version = %s\n", enigma_version_string());
    return 0;
}
