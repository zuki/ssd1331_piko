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

/* SPI経由で96x64 16bit-Color OLEDディスプレイを駆動するSSD1331を
   操作するサンプルコード

   * GPIO 17 (pin 22) Chip select -> SSD1331ボードのCS (Chip Select)
   * GPIO 18 (pin 24) SCK/spi0_sclk -> SSD1331ボードのSCL
   * GPIO 19 (pin 25) MOSI/spi0_tx -> SSD1331ボードのSDA (MOSI)
   * GPIO 21 (pin 27) -> SSD1331ボードのRES (Reset)
   * GPIO 22 (pin 26) -> SSD1331ボードのDC (Data/Command)
   * 3.3V OUT (pin 36) -> SSD1331ボードのVCC
   * GND (pin 38)  -> SSD1331ボードのGND

   spi_default (SPI0) インスタンスを使用する。
*/

void calc_render_area_buflen(struct render_area *area) {
    // レンダーエリアのバッファの長さを計算する: データは16bit
    area->buflen = 2 * (area->end_col - area->start_col + 1) * (area->end_page - area->start_page + 1);
}

void send_cmd(spi_inst_t *spi, uint8_t cmd) {
    CS_SELECT;
    DC_COMMAND;
    spi_write_blocking(spi, &cmd, 1);
    DC_DATA;
    CS_DESELECT;
}

void send_cmd_list(spi_inst_t *spi, uint8_t *buf, size_t len) {
    for (int i = 0; i < len; i++)
        send_cmd(spi, buf[i]);
}

void send_data(spi_inst_t *spi, uint8_t *buf, size_t len) {
    CS_SELECT;
    spi_write_blocking(spi, buf, len);
    CS_DESELECT;
}

void reset(spi_inst_t *spi) {
    RESET_OFF;
    CS_DESELECT;
    busy_wait_ms(5);
    RESET_ON;
    busy_wait_ms(80);
    RESET_OFF;
    busy_wait_ms(20);
}

void rect(spi_inst_t *spi, uint32_t x, uint32_t width, uint32_t y, uint32_t height)
{
    uint8_t cmds[] = {
        SSD1331_SET_COL_ADDR, x, width,
        SSD1331_SET_ROW_ADDR, y, height
    };
    send_cmd_list(spi, cmds, count_of(cmds));
}

void clear(spi_inst_t *spi, uint16_t color) {
    uint8_t buf[] = {
        (uint8_t)(color >> 8 & 0xFF),
        (uint8_t)(color & 0xFF)
    };

    rect(spi, 0, SSD1331_WIDTH - 1, 0, SSD1331_HEIGHT - 1);

    for (int i = 0; i < (SSD1331_WIDTH * SSD1331_HEIGHT); i++)
        send_data(spi, buf, 2);
}

void write_pixel(spi_inst_t *spi, int x, int y, uint16_t color) {
    uint8_t buf[] = {
        (uint8_t)(color >> 8 & 0xFF),
        (uint8_t)(color & 0xFF)
    };
    rect(spi, x, x, y, y);
    send_data(spi, buf, 2);
}

void draw_line(spi_inst_t *spi, int x0, int y0, int x1, int y1, uint16_t color) {
    assert(x0 >= 0 && x1 <= SSD1331_WIDTH - 1
        && y0 >= 0 && y1 <= SSD1331_HEIGHT - 1);

    int dx =  abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1-y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int e2;

    while (true) {
        write_pixel(spi, x0, y0, color);
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

 void ssd1331_init(spi_inst_t *spi, uint freq) {
    // SPI0 を 10MHz で使用.
    spi_init(spi, freq);

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
    reset(spi);

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

    send_cmd_list(spi, cmds, count_of(cmds));
};

int main() {
    // stdioの初期化
    stdio_init_all();
    // ssd1331の初期化
    ssd1331_init(spi_default, 10 * 1000 * 1000);
    printf("Hello, SSD1331\n");

    clear(spi_default, COL_WHITE);
    draw_line(spi_default, 10, 5, 80, 60, COL_BLACK);
    busy_wait_ms(500);
    draw_line(spi_default, 10, 5, 80, 60, COL_WHITE);
    busy_wait_ms(500);
    draw_line(spi_default, 20, 10, 50, 40, COL_BLUE);
    busy_wait_ms(500);
    draw_line(spi_default, 20, 10, 50, 40, COL_WHITE);
    busy_wait_ms(500);
    draw_line(spi_default, 0, 0, 95, 63, COL_RED);
    busy_wait_ms(500);
    draw_line(spi_default, 0, 0, 95, 63, COL_WHITE);
    busy_wait_ms(500);
    draw_line(spi_default, 0, 0, 95, 63, COL_GREEN);
    busy_wait_ms(500);
    clear(spi_default, COL_BLACK);
    send_cmd(spi_default, SSD1331_SET_DISP_OFF);

/*
        busy_wait_ms(500);
        clear(spi_default, COL_BLACK);
        send_cmd(spi_default, SSD1331_SET_DISP_OFF);

    while(1) {
        clear(spi_default, COL_WHITE);
        busy_wait_ms(500);
        clear(spi_default, COL_YELLOW);
        busy_wait_ms(500);
        clear(spi_default, COL_RED);
        busy_wait_ms(500);
        clear(spi_default, COL_MAGENTA);
        busy_wait_ms(500);
        clear(spi_default, COL_BLUE);
        busy_wait_ms(500);
        clear(spi_default, COL_AQUA);
        busy_wait_ms(500);
        clear(spi_default, COL_GREEN);
        busy_wait_ms(500);
    }
*/
    return 0;
}
