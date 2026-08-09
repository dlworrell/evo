#include "value.h"

#include <string.h>

int main(int argc, char **argv)
{
    volatile int value = 0;
    int index;

    if (argc != 3 || strcmp(argv[1], "--fixture") != 0 ||
        strcmp(argv[2], "small") != 0) {
        return 2;
    }
    for (index = 0; index < 1000; index += 1) {
        value += fixture_value(index);
    }
    return value == 1499500 ? 0 : 1;
}
