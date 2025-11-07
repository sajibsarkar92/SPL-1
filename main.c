// main.c — minimal program entry point

#include "core.h"
#include <stdlib.h>

int main(void) {
    // Delegate program control to core controller. Keep main tiny.
    return raw_data_to_csv();
}