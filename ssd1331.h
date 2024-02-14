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

/* SPI経由で96x64 16bit-Color OLEDディスプレイを駆動するSSD1331を
   操作するサンプルコード

   * GPIO 17 (pin 22) Chip select -> SSD1331ボードのCS (Chip Select)
   * GPIO 18 (pin 24) SCK/spi0_sclk -> SSD1331ボードのSCL
   * GPIO 19 (pin 25) MOSI/spi0_tx -> SSD1331ボードのSDA (MOSI)
   * GPIO 21 (pin 27) -> SSD1331ボードのRES (Reset)
   * GPIO 20 (pin 26) -> SSD1331ボードのDC (Data/Command)
   * 3.3V OUT (pin 36) -> SSD1331ボードのVCC
   * GND (pin 38)  -> SSD1331ボードのGND

   spi_default (SPI0) インスタンスを使用する。
*/
#define SPI_SCK_PIN         PICO_DEFAULT_SPI_SCK_PIN
#define SPI_MOSI_PIN        PICO_DEFAULT_SPI_TX_PIN
#define SPI_CSN_PIN         PICO_DEFAULT_SPI_CSN_PIN
#define SPI_DCN_PIN         20
#define SPI_RESN_PIN        21

#define SSD1331_HEIGHT      64
#define SSD1331_WIDTH       96
#define SSD1331_BUF_LEN     (2 * SSD1331_HEIGHT * SSD1331_WIDTH)

#define RGB(r,g,b)	(uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))

#define COL_BLACK	RGB(0,0,0)
#define COL_WHITE	RGB(255,255,255)
#define COL_RED		RGB(255,0,0)
#define COL_GREEN	RGB(0,255,0)
#define COL_BLUE	RGB(0,0,255)


#define COL_YELLOW	RGB(255,255,0)
#define COL_MAGENTA	RGB(255,0,255)
#define COL_AQUA	RGB(0,255,255)

#define COL_PURPLE	RGB(160,32,240)
#define COL_REDPINK RGB(255,50,50)
#define COL_ORANGE  RGB(255,165,0)
#define	COL_LGRAY	RGB(160,160,160)
#define	COL_GRAY	RGB(128,128,128)

#define COL_FRONT   COL_WHITE
#define COL_BACK   	COL_BLACK

// 基本コマンド
#define SSD1331_SET_COL_ADDR        _u(0x15)    // 桁アドレスの設定: 0x00/0x5F
#define SSD1331_SET_ROW_ADDR        _u(0x75)    // 行アドレスの設定: 0x00/0x3F
#define SSD1331_SET_CONTRAST_A      _u(0x81)    // カラー"A"のコントラストの設定: 0x80
#define SSD1331_SET_CONTRAST_B      _u(0x82)    // カラー"B"のコントラストの設定: 0x80
#define SSD1331_SET_CONTRAST_C      _u(0x83)    // カラー"C"のコントラストの設定: 0x80
#define SSD1331_MASTER_CURRENT_CNTL _u(0x87)    // マスターカレント制御: 0x0F
#define SSD1331_SET_PRECHARGE_A     _u(0x8A)    // カラー"A"の二次プリチャージ速度の設定: 0x81
#define SSD1331_SET_PRECHARGE_B     _u(0x8B)    // カラー"A"の二次プリチャージ速度の設定: 0x82
#define SSD1331_SET_PRECHARGE_C     _u(0x8C)    // カラー"A"の二次プリチャージ速度の設定: 0x83
#define SSD1331_REMAP_COLOR_DEPTH   _u(0xA0)    // Remapとカラー震度の設定: 0x40
#define SSD1331_SET_DISP_START_LINE _u(0xA1)    // ディスプレイ開始行の設定: 0x00
#define SSD1331_SET_DISP_OFFSET     _u(0xA2)    // ディスプレイオフセットの設定: 0x00
#define SSD1331_SET_NORM_DISP       _u(0xA4)
#define SSD1331_SET_ALL_ON          _u(0xA5)
#define SSD1331_SET_ALL_OFF         _u(0xA6)
#define SSD1331_SET_INV_DISP        _u(0xA7)
#define SSD1331_SET_MUX_RATIO       _u(0xA8)    // 多重度比を設定: 0x3F
#define SSD1331_SET_DIM             _u(0xAB)    // Dimモードの設定: 0x00/A/B/C/D/プリチャージ
#define SSD1331_SET_MASTER_CONFIG   _u(0xAD)    // マスター構成の設定: 0x8F
#define SSD1331_SET_DISP_ON_DIM     _u(0xAC)    // DIMモードでディスプレイオン
#define SSD1331_SET_DISP_OFF        _u(0xAE)    // ディスプレイオフ
#define SSD1331_SET_DISP_ON_NORM    _u(0xAF)    // ノーマルモードでディスプレイオン
#define SSD1331_POWER_SAVE          _u(0xB0)    // パワーセーブモード: 0x1A: 有効、0x0B: 無効
#define SSD1331_ADJUST              _u(0xB1)    // フェーズ1,2クロック数の調整: 0x74
#define SSD1331_DISP_CLOck          _u(0xB3)    // ディスプレイクロック分周比/発振周波数: 0xD0
#define SSD1331_SET_GRAY_SCALE      _u(0xB8)    // グレイスケールテーブルの設定: 32バイト
#define SSD1331_EN_LINEAR_SCALE     _u(0xB9)    // 線形グレイスケールテーブルの有効化
#define SSD1331_SET_PRECHARGE_LEVEL _u(0xBB)    // プリチャージレベルの設定: 0x3E
#define SSD1331_NOP                 _u(0xBC)    // NOP, 0xBD, 0xE3もNOP
#define SSD1331_SET_VCOMH           _u(0xBE)    // Vcomhの設定: 0x3E
#define SSD1331_SET_COMND_LOCK      _u(0xFD)    // コマンドロックの設定: 0x12

// 描画コマンド
#define SSD1331_DRAW_LINE          _u(0x21)     // ラインの描画: 7バイト
#define SSD1331_DRAW_RECT          _u(0x22)     // 矩形の描画: 10バイト
#define SSD1331_COPY               _u(0x23)     // コピー: 6バイト
#define SSD1331_DIM_WIN            _u(0x24)     // ウィンドウをぼやかす: 4バイト
#define SSD1331_CLEAR_WIN          _u(0x25)     // ウィンドウをクリア: 4バイト
#define SSD1331_FILL               _u(0x26)     // 塗りつぶしの有効/無効: 0x00
#define SSD1331_SETUP_SCROL        _u(0x27)     // 水平/垂直スクロールの設定: 5バイト
#define SSD1331_DEACT_SCROL        _u(0x2E)     // スクロールを停止
#define SSD1331_ACT_SCROL          _u(0x2F)     // スクロールを開始

struct render_area {
    uint8_t start_col;
    uint8_t end_col;
    uint8_t start_row;
    uint8_t end_row;

    int buflen;
};

typedef enum scroll_interval {
    SCROLL_ULTRA_HIGH, SCROLL_HIGH, SCROLL_MEDIUM, SCROLL_LOW
} scroll_interval_t;

#define CS_SELECT       gpio_put(SPI_CSN_PIN, 0)
#define CS_DESELECT     gpio_put(SPI_CSN_PIN, 1)
#define DC_COMMAND      gpio_put(SPI_DCN_PIN, 0)
#define DC_DATA         gpio_put(SPI_DCN_PIN, 1)
#define RESET_ON        gpio_put(SPI_RESN_PIN, 0)
#define RESET_OFF       gpio_put(SPI_RESN_PIN, 1)

void calc_render_area_buflen(struct render_area *area);
void send_cmd(uint8_t cmd);
void send_cmd_list(uint8_t *buf, size_t len);
void send_data(uint8_t *buf, size_t len);
void scroll(uint8_t h, uint8_t v, scroll_interval_t speed, bool on);
void render(uint8_t *buf, struct render_area *area);
void reset();
//void clear(uint16_t color);

void ssd1331_init(uint freq);
