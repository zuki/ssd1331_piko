/**
 * Copyright (c) 2023 SUZUKI Keiji.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "ssd1331.h"
#include "font.h"
#include "image.h"

/* SPI経由で96x64 16bit-Color OLEDディスプレイを駆動するSSD1331を
   操作するサンプルコード

   * GPIO 17 (pin 22) Chip select -> SSD1331ボードのCS (Chip Select)
   * GPIO 18 (pin 24) SCK/spi_default0_sclk -> SSD1331ボードのSCL
   * GPIO 19 (pin 25) MOSI/spi_default0_tx -> SSD1331ボードのSDA (MOSI)
   * GPIO 21 (pin 27) -> SSD1331ボードのRES (Reset)
   * GPIO 22 (pin 26) -> SSD1331ボードのDC (Data/Command)
   * 3.3V OUT (pin 36) -> SSD1331ボードのVCC
   * GND (pin 38)  -> SSD1331ボードのGND

   spi_default_default (SPI0) インスタンスを使用する。
*/

void calc_render_area_buflen(struct render_area *area) {
    // レンダーエリアのバッファの長さを計算する: データは16bit
    area->buflen = 2 * (area->end_col - area->start_col + 1) * (area->end_row - area->start_row + 1);
}

#ifdef spi_default

void send_cmd(uint8_t cmd) {
    CS_SELECT;
    DC_COMMAND;
    spi_write_blocking(spi_default, &cmd, 1);
    DC_DATA;
    CS_DESELECT;
}

void send_cmd_list(uint8_t *buf, size_t len) {
    for (int i = 0; i < len; i++)
        send_cmd(buf[i]);
}

void send_data(uint8_t *buf, size_t len) {
    CS_SELECT;
    DC_DATA;
    spi_write_blocking(spi_default, buf, len);
    DC_COMMAND;
    CS_DESELECT;
}

void ssd1331_init(uint freq) {
    // SPI0 を 10MHz で使用.
    spi_init(spi_default, freq);

    gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);

    // CS# (Chip select)ピンはアクティブLOWなので、HIGH状態で初期化
    gpio_init(SPI_CSN_PIN);
    gpio_set_function(SPI_CSN_PIN, GPIO_FUNC_SPI);
    gpio_set_dir(SPI_CSN_PIN, GPIO_OUT);
    CS_DESELECT;

    // D/C# (Data/Command)ピンはアクティブLOWなので、HIGH状態で初期化
    gpio_init(SPI_DCN_PIN);
    //gpio_set_function(SPI_DCN_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(SPI_DCN_PIN, GPIO_OUT);
    DC_DATA;

    // RES# (Data/Command)ピンはアクティブLOWなので、HIGH状態で初期化
    gpio_init(SPI_RESN_PIN);
    //gpio_set_function(SPI_RESN_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(SPI_RESN_PIN, GPIO_OUT);
    gpio_put(SPI_RESN_PIN, 1);

    // ssd1331をリセット
    reset();

    // デフォルト値で初期化: Adafruit-SSD1331-OLED-Driver-Library-for-Arduinoから引用
    uint8_t cmds[] = {
        SSD1331_SET_DISP_OFF,
        SSD1331_SET_ROW_ADDR, 0x00, 0x3F,
        SSD1331_SET_COL_ADDR, 0x00, 0x5F,
        SSD1331_REMAP_COLOR_DEPTH, 0x72,
        SSD1331_SET_DISP_START_LINE, 0x00,
        SSD1331_SET_DISP_OFFSET, 0x00,
        SSD1331_SET_NORM_DISP,
        SSD1331_SET_MUX_RATIO, 0x3F,
        SSD1331_SET_MASTER_CONFIG, 0xBE,
        SSD1331_POWER_SAVE, 0x0B,
        SSD1331_ADJUST, 0x74,
        SSD1331_DISP_CLOck, 0xF0,
        SSD1331_SET_PRECHARGE_A, 0x64,
        SSD1331_SET_PRECHARGE_B, 0x78,
        SSD1331_SET_PRECHARGE_C, 0x64,
        SSD1331_SET_PRECHARGE_LEVEL, 0x3A,
        SSD1331_SET_VCOMH, 0x3E,
        SSD1331_MASTER_CURRENT_CNTL, 0x06,
        SSD1331_SET_CONTRAST_A, 0x91,
        SSD1331_SET_CONTRAST_B, 0x50,
        SSD1331_SET_CONTRAST_C, 0x7D,
        SSD1331_SET_DISP_ON_NORM
    };

    send_cmd_list(cmds, count_of(cmds));
};

void scroll(bool on) {
    // 水平スクロールを構成
    uint8_t cmds[] = {
        SSD1331_SETUP_SCROL,
        0x00, // 水平スクロールオフセット列数
        0x00, // 開始行アドレス
        0x40, // スクロールする行数
        0x00, // 垂直スクロールオフセットの行数
        0x00, // スクロールの時間間隔: 6 フレーム
        on ? SSD1331_ACT_SCROL : SSD1331_DEACT_SCROL
    };

    send_cmd_list(cmds, count_of(cmds));
}

void render(uint8_t *buf, struct render_area *area) {
    // render_areaでディスプレイの一部を更新
    uint8_t cmds[] = {
        SSD1331_SET_COL_ADDR,
        area->start_col,
        area->end_col,
        SSD1331_SET_ROW_ADDR,
        area->start_row,
        area->end_row
    };

    send_cmd_list(cmds, count_of(cmds));
    send_data(buf, area->buflen);
}

static void set_pixel(char *buf, int x, int y, uint16_t color) {
    assert(x >= 0 && x < SSD1331_WIDTH && y >=0 && y < SSD1331_HEIGHT);

    int idx = 2 * (y * 96 + x);
    buf[idx]   = (uint8_t)(color & 0xFF);
    buf[idx*1] = (uint8_t)(color >> 8 & 0xFF);
}

void reset(void) {
    RESET_OFF;
    CS_DESELECT;
    sleep_ms(5);
    RESET_ON;
    sleep_ms(80);
    RESET_OFF;
    sleep_ms(20);
}

static void draw_line(char *buf, int x0, int y0, int x1, int y1, uint16_t color) {
    assert(x0 >= 0 && x1 <= SSD1331_WIDTH - 1
        && y0 >= 0 && y1 <= SSD1331_HEIGHT - 1);

    int dx =  abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1-y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int e2;

    while (true) {
        set_pixel(buf, x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2 * err;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static inline int get_font_index(uint8_t ch) {
    if (ch >= 'A' && ch <='Z') {
        return  ch - 'A' + 1;
    }
    else if (ch >= '0' && ch <='9') {
        return  ch - '0' + 27;
    }
    else return  0; // Not got that char so space.
}

static void write_char(uint8_t *buf, int x, int y, uint8_t ch, uint16_t color) {
    if (x > SSD1331_WIDTH - 8 || y > SSD1331_HEIGHT - 8)
        return;

    uint8_t b, c, p;

    ch = toupper(ch);
    int idx = get_font_index(ch);    // 文字のfont[]内の行数
    for (int i = 0; i < 8; i++) {
        c = font[idx*8+i];
        p = 0x80;
        for (int k = 0; k < 8; k++) {
            b = (c & p);
            p >>= 1;
            set_pixel(buf, x + k, y + i, b ? color : 0);
        }
    }
}

static void write_string(uint8_t *buf, int x, int y, char *str, uint16_t color) {
    // Cull out any string off the screen
    if (x > SSD1331_WIDTH - 8 || y > SSD1331_HEIGHT - 8)
        return;

    while (*str) {
        write_char(buf, x, y, *str++, color);
        x += 8;
    }
}

#endif // ifdef spi_default

int main() {
    // stdioの初期化
    stdio_init_all();

#if !defined(spi_default)
#warning this example requires a board with spi pins
    puts("Default SPI pins were not defined");
#else
    // ssd1331の初期化
    ssd1331_init(10 * 1000 * 1000);
    //printf("Hello, SSD1331\n");

    // 描画領域を初期化
    struct render_area frame_area = {
        start_col: 0,
        end_col : SSD1331_WIDTH - 1,
        start_row : 0,
        end_row : SSD1331_HEIGHT - 1
    };

    calc_render_area_buflen(&frame_area);

    // 画面全体を黒で塗りつぶす
    uint8_t buf[SSD1331_BUF_LEN];
    memset(buf, 0, SSD1331_BUF_LEN);
    render(buf, &frame_area);
/*
    // 画面を3回フラッシュ
    for (int i = 0; i < 3; i++) {
        send_cmd(SSD1331_SET_ALL_ON);
        sleep_ms(500);
        send_cmd(SSD1331_SET_ALL_OFF);
        sleep_ms(500);
    }
*/

    // 画像を描画
    struct render_area area = {
        start_row : 0,
        end_row : IMG_HEIGHT - 1
    };

    area.start_col = 0;
    area.end_col = IMG_WIDTH - 1;
/*
    calc_render_area_buflen(&area);
    render(img, &area);

    scroll(true);
    sleep_ms(5000);
    scroll(false);
*/
    char *text[] = {
        "A long time ",
        "ago on an OL",
        "ED display f",
        "ar far away ",
        "Lived a smal",
        "l red raspbe",
        "rry by the ",
        "name of PICO"
    };


    int y = 0;
    for (int i = 0; i < count_of(text); i++) {
        write_string(buf, 0, y, text[i], COL_ORANGE);
        y += 8;
    }
    render(buf, &frame_area);

    printf("COL_WHITE: 0x%04x\n", COL_WHITE);
    printf("COL_RED: 0x%04x\n", COL_RED);
    printf("COL_GREEN: 0x%04x\n", COL_GREEN);
    printf("COL_BLUE: 0x%04x\n", COL_BLUE);
    printf("COL_YELLOW: 0x%04x\n", COL_YELLOW);
    printf("COL_MAGENTA: 0x%04x\n", COL_MAGENTA);
    printf("COL_AQUA: 0x%04x\n", COL_AQUA);
    printf("COL_PURPLE: 0x%04x\n", COL_PURPLE);
    printf("COL_REDPINK: 0x%04x\n", COL_REDPINK);
    printf("COL_ORANGE: 0x%04x\n", COL_ORANGE);
    printf("COL_LGRAY: 0x%04x\n", COL_LGRAY);
    printf("COL_GRAY: 0x%04x\n", COL_GRAY);

/*
    scroll(true);
    sleep_ms(5000);
    scroll(false);

    // Test the display invert function
    sleep_ms(3000);
    send_cmd(SSD1331_SET_INV_DISP);
    sleep_ms(3000);
    send_cmd(SSD1331_SET_NORM_DISP);

    bool pix = true;
    for (int i = 0; i < 2;i++) {
        for (int x = 0;x < SSD1306_WIDTH;x++) {
            DrawLine(buf, x, 0,  SSD1306_WIDTH - 1 - x, SSD1306_HEIGHT - 1, pix);
            render(buf, &frame_area);
        }

        for (int y = SSD1306_HEIGHT-1; y >= 0 ;y--) {
            DrawLine(buf, 0, y, SSD1306_WIDTH - 1, SSD1306_HEIGHT - 1 - y, pix);
            render(buf, &frame_area);
        }
        pix = false;
    }

    goto restart;


    clear(COL_WHITE);
    draw_line(10, 5, 80, 60, COL_BLACK);
    sleep_ms(500);
    draw_line(10, 5, 80, 60, COL_WHITE);
    sleep_ms(500);
    draw_line(20, 10, 50, 40, COL_BLUE);
    sleep_ms(500);
    draw_line(20, 10, 50, 40, COL_WHITE);
    sleep_ms(500);
    draw_line(0, 0, 95, 63, COL_RED);
    sleep_ms(500);
    draw_line(0, 0, 95, 63, COL_WHITE);
    sleep_ms(500);
    draw_line(0, 0, 95, 63, COL_GREEN);
    sleep_ms(500);
    clear(COL_BLACK);
    send_cmd(SSD1331_SET_DISP_OFF);


        sleep_ms(500);
        clear(COL_BLACK);
        send_cmd(SSD1331_SET_DISP_OFF);

    while(1) {
        clear(COL_WHITE);
        sleep_ms(500);
        clear(COL_YELLOW);
        sleep_ms(500);
        clear(COL_RED);
        sleep_ms(500);
        clear(COL_MAGENTA);
        sleep_ms(500);
        clear(COL_BLUE);
        sleep_ms(500);
        clear(COL_AQUA);
        sleep_ms(500);
        clear(COL_GREEN);
        sleep_ms(500);
    }
*/

#endif

    return 0;
}
