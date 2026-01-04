#include "dam/dam_config.h"

void dam_small_init(void) {
    int list[32];
    int count = 0;
    int value = DAM_SMALL_MIN;

    while (value <= DAM_SMALL_MAX) {
        list[count++] = value;
        value *= SIZE_CLASS_MULTIPLIER;
    }
}