#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "ssd1306_font.h"

int main(void) {
    uint8_t B[8] = {0};
    uint8_t p;

    printf("static uint8_t font[] = {\n    ");
    for (int i = 0; i < sizeof(font) / 8; i++) {
        memset(B, 0, 8);
        for (int j = 0; j < 8; j++) {
            p = 0x80;
            for (int k = 0; k < 8; k++) {
                B[k] <<= 1;
                B[k] |= ((font[i*8+j] & p) ? 1 : 0);
                p >>= 1;
            }
        }
        for (int j = 0; j < 8; j++) {
            printf("0x%02x, ", B[j]);
        }
        printf("\n    ");
    }
    printf("};\n");
    return 0;
}
