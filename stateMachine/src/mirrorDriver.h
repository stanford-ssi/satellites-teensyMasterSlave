#ifndef MIRROR_H
#define MIRROR_H

#include <main.h>

typedef struct mirrorOutput {
    uint32_t x_high;
    uint32_t x_low;
    uint32_t y_high;
    uint32_t y_low;
    mirrorOutput() {
        x_high = x_low = y_high = y_low = 0;
    }

    void copy(const volatile mirrorOutput& s) {
        this->x_high = s.x_high;
        this->x_low = s.x_low;
        this->y_high = s.y_high;
        this->y_low = s.y_low;
    }
    mirrorOutput(const volatile mirrorOutput& s) {
        copy(s);
    }
    mirrorOutput& operator =(const volatile mirrorOutput& s) {
        copy(s);
        return *this;
    }
    void toString(char* buf, int len) {
        snprintf(buf, len - 1, "x_high %u, x_low %u, y_high %u, y_low %u\n", (unsigned int) x_high, (unsigned int) x_low, (unsigned int) y_high, (unsigned int) y_low);
    }
} mirrorOutput;

#endif
