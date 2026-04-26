
#include "plume/cli/entrypoint.h"

int main (int argc, char** argv) {
    return entrypoint(argc - 1, argv + 1);
}
