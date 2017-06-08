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
    mirrorOutput(volatile mirrorOutput& s) {
        copy(s);
    }
    mirrorOutput& operator =(const volatile mirrorOutput& s) {
        copy(s);
        return *this;
    }
} mirrorOutput;

#endif
