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
        memcpy(this, (const void *) &s, sizeof(mirrorOutput));
    }
    mirrorOutput(const volatile mirrorOutput& s) {
        copy(s);
    }
    mirrorOutput& operator =(const volatile mirrorOutput& s) {
        copy(s);
        return *this;
    }
    void toString(char* buf, int len) {
        snprintf(buf, len - 1, "x_high %u, x_low %u, y_high %u, y_low %u", (unsigned int) x_high, (unsigned int) x_low, (unsigned int) y_high, (unsigned int) y_low);
    }

    // Sum of memory, by uint16s
    uint16_t getChecksum() {
        int size = sizeof(mirrorOutput) * 8 / 16; // uint16_t units
        uint16_t checksum = 0;
        for (int i = 0; i < size; i++) {
            checksum += ((uint16_t *) this)[i];
        }
        return checksum;
    }
} mirrorOutput;

/*
typedef struct expandedMirrorOutput {
    uint32_t x_high_msb;
    uint32_t x_high_lsb;
    uint32_t x_low_msb;
    uint32_t x_low_lsb;
    uint32_t y_high_msb;
    uint32_t y_high_lsb;
    uint32_t y_low;
    uint32_t y_low;

    expandedMirrorOutput() {
        x_high = x_low = y_high = y_low = 0;
    }

    void copy(const volatile mirrorOutput& s) {
        memcpy(this, (const void *) &s, sizeof(mirrorOutput));
    }
    expandedMirrorOutput(const volatile mirrorOutput& s) {
        copy(s);
    }
    expandedMirrorOutput& operator =(const volatile mirrorOutput& s) {
        copy(s);
        return *this;
    }
    void toString(char* buf, int len) {
        snprintf(buf, len - 1, "x_high %u, x_low %u, y_high %u, y_low %u", (unsigned int) x_high, (unsigned int) x_low, (unsigned int) y_high, (unsigned int) y_low);
    }

    // Sum of memory, by uint16s
    uint16_t getChecksum() {
        int size = sizeof(mirrorOutput) * 8 / 16; // uint16_t units
        uint16_t checksum = 0;
        for (int i = 0; i < size; i++) {
            checksum += ((uint16_t *) this)[i];
        }
        return checksum;
    }
} expandedMirrorOutput;*/

#endif
