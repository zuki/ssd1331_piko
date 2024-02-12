#include <stdio.h>
#include <stdint.h>
#include <string.h>

/*
 * 24bit BMPから16bit RGB565に変換してheaderファイルに書き出す
 *  cc -o bmp2hex bmp_to_head.c
 *  ./bmp2head file.bmp > ../image.h
 */
int main(int argc, char *argv[]) {
    int32_t offset, img_size, width, height;
    uint8_t data[3];
    uint16_t rgb;

    FILE *fd = fopen(argv[1], "rb");
    if (!fd) return 1;
    // 画像データのファイル先頭からのオフセット
    fseek(fd, 0x0a, SEEK_SET);
    fread(&offset, 4, 1, fd);
    // 画像の幅と高さ
    fseek(fd, 0x12, SEEK_SET);
    fread(&width, 4, 1, fd);
    fread(&height, 4, 1, fd);
    // 画像のサイズ(3バイト/1ピクセル)
    fseek(fd, 0x22, SEEK_SET);
    fread(&img_size, 4, 1, fd);

    fseek(fd, offset, SEEK_SET);
    printf("#define IMG_WIDTH %d\n", width);
    printf("#define IMG_HEIGHT %d\n\n", height);
    printf("const uint8_t img[] = {\n\t");

    for (int i=0; i < img_size/3; i++) {
        fread(data, 3, 1, fd);
        // 画像データは BGR の順で格納されている
        rgb = (uint16_t)(((data[2] >> 3) << 11) | ((data[1] >> 2) << 5) | (data[0] >> 3));
        // RGB565はリトルエンディアンで出力
        printf("%s 0x%02X, 0x%02X", i == 0 ? " " : ",", (rgb >> 8) & 0xff, rgb & 0xff);
        if (((i + 1) % 8) == 0) printf("\n\t");
    }

    printf("};\n");

    fclose(fd);
    return 0;
}
