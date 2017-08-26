#ifndef MIRROR_H
#define MIRROR_H

#include <main.h>

typedef struct mirrorOutput {
    int32_t x;
    int32_t y;
    uint32_t useless1; // Temporary, will remove later
    uint32_t useless2;
    mirrorOutput() {
        x = y = 0;
        useless1 = useless2 = 0xbeefdbca;
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
        snprintf(buf, len - 1, "x %u, y %u", (unsigned int) x, (unsigned int) y);
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

void mirrorDriverSetup();
void sendMirrorOutput(const mirrorOutput& out);

#endif
