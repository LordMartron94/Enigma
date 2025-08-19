#include <stdio.h>
#include "enigma/enigma.h"

int main(void) {
    printf("[enigma] Hello from demo app!\n");
    printf("[enigma] add(2, 3) = %d\n", enigma_add(2, 3));
    printf("[enigma] version = %s\n", enigma_version_string());
    return 0;
}
