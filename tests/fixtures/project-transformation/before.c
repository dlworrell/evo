#include <stdint.h>

static unsigned int transform_fixture(
    unsigned int value,
    int delta,
    int ready)
{
    unsigned int scaled = value * 8U;
    int total = delta;
    total = total + ready;
    if (!!ready) {
        total += 1;
    }
    return scaled + (unsigned int)total;
}

int main(void)
{
    return transform_fixture(2U, 1, 1) == 19U ? 0 : 1;
}
