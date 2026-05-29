/*
 * Picocalc_Clock - PicoCalc clock firmware.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#include "diagnostics/screenshot_capture.h"

#include "config/build_config.h"
#include "platform/picocalc_display.h"
#include "platform/picocalc_uart_log.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if PICOMENT_ENABLE_SD_WRITE
#include "ff.h"
#endif

namespace picoment::diagnostics {
namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 320;
constexpr int kChunkRows = 16;
constexpr uint32_t kBmpHeaderSize = 54;
constexpr uint32_t kRowBytes = static_cast<uint32_t>(kWidth * 3);
constexpr uint32_t kImageBytes = static_cast<uint32_t>(kHeight) * kRowBytes;
constexpr uint32_t kFileBytes = kBmpHeaderSize + kImageBytes;
constexpr const char* kScreenshotDir = "0:/screenshots";
constexpr const char* kFallbackPath = "0:/screenshots/clk.BMP";

bool g_busy = false;

void notify_progress(ScreenshotProgressCallback progress_callback, void* progress_context) {
    if (progress_callback != nullptr) {
        progress_callback(progress_context);
    }
}

void log_readback_probe() {
    uint8_t raw[12] = {};
    if (!picoment::display::readback_probe_raw(0, 0, raw, sizeof(raw))) {
        log_printf("SCREENSHOT", "screenshot error stage=readback fr=0");
        return;
    }
    log_printf("SCREENSHOT",
               "screenshot raw y=0 x=0 bytes=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               raw[0],
               raw[1],
               raw[2],
               raw[3],
               raw[4],
               raw[5],
               raw[6],
               raw[7],
               raw[8],
               raw[9],
               raw[10],
               raw[11]);
    log_printf("SCREENSHOT", "screenshot mode dummy=1 bpp_bytes=2 swap=0 edge=fall shift=0");
}

#if PICOMENT_ENABLE_SD_WRITE
FATFS g_screenshot_fs{};
uint16_t g_pixels[kWidth * kChunkRows] = {};
uint8_t g_row[kRowBytes] = {};

void put_u16(uint8_t* dst, uint16_t value) {
    dst[0] = static_cast<uint8_t>(value);
    dst[1] = static_cast<uint8_t>(value >> 8);
}

void put_u32(uint8_t* dst, uint32_t value) {
    dst[0] = static_cast<uint8_t>(value);
    dst[1] = static_cast<uint8_t>(value >> 8);
    dst[2] = static_cast<uint8_t>(value >> 16);
    dst[3] = static_cast<uint8_t>(value >> 24);
}

bool file_exists(const char* path) {
    FILINFO info{};
    return f_stat(path, &info) == FR_OK;
}

bool choose_path(char* out_path, size_t out_size) {
    for (int index = 1; index <= 9999; ++index) {
        snprintf(out_path, out_size, "0:/screenshots/clk_%04d.BMP", index);
        if (!file_exists(out_path)) {
            return true;
        }
    }
    if (file_exists(kFallbackPath)) {
        return false;
    }
    snprintf(out_path, out_size, "%s", kFallbackPath);
    return true;
}

bool write_exact(FIL* file, const void* data, UINT size) {
    UINT written = 0;
    const FRESULT result = f_write(file, data, size, &written);
    return result == FR_OK && written == size;
}

void make_bmp_header(uint8_t header[kBmpHeaderSize]) {
    memset(header, 0, kBmpHeaderSize);
    header[0] = 'B';
    header[1] = 'M';
    put_u32(&header[2], kFileBytes);
    put_u32(&header[10], kBmpHeaderSize);
    put_u32(&header[14], 40);
    put_u32(&header[18], kWidth);
    put_u32(&header[22], static_cast<uint32_t>(-kHeight));
    put_u16(&header[26], 1);
    put_u16(&header[28], 24);
    put_u32(&header[34], kImageBytes);
}

void convert_row_rgb565_to_bgr24(const uint16_t* pixels) {
    for (int x = 0; x < kWidth; ++x) {
        const uint16_t rgb = pixels[x];
        const uint8_t red = static_cast<uint8_t>(((rgb >> 11) & 0x1f) << 3);
        const uint8_t green = static_cast<uint8_t>(((rgb >> 5) & 0x3f) << 2);
        const uint8_t blue = static_cast<uint8_t>((rgb & 0x1f) << 3);
        const int offset = x * 3;
        g_row[offset + 0] = blue;
        g_row[offset + 1] = green;
        g_row[offset + 2] = red;
    }
}
#endif

}  // namespace

bool capture_screenshot(ScreenshotProgressCallback progress_callback, void* progress_context) {
    if (g_busy) {
        log_printf("SCREENSHOT", "screenshot error stage=busy fr=0");
        return false;
    }
    g_busy = true;
    log_readback_probe();
    notify_progress(progress_callback, progress_context);

#if !PICOMENT_ENABLE_SD_WRITE
    log_printf("SCREENSHOT", "screenshot error stage=sd_write_disabled fr=0");
    g_busy = false;
    return false;
#else
    FRESULT result = f_mount(&g_screenshot_fs, "0:", 1);
    if (result != FR_OK) {
        log_printf("SCREENSHOT", "screenshot error stage=mount fr=%u", static_cast<unsigned>(result));
        g_busy = false;
        return false;
    }

    result = f_mkdir(kScreenshotDir);
    if (result != FR_OK && result != FR_EXIST) {
        log_printf("SCREENSHOT", "screenshot error stage=mkdir fr=%u", static_cast<unsigned>(result));
        g_busy = false;
        return false;
    }

    char path[40] = {};
    if (!choose_path(path, sizeof(path))) {
        log_printf("SCREENSHOT", "screenshot error stage=path fr=0");
        g_busy = false;
        return false;
    }

    FIL file{};
    result = f_open(&file, path, FA_CREATE_NEW | FA_WRITE);
    if (result != FR_OK) {
        log_printf("SCREENSHOT", "screenshot error stage=open fr=%u", static_cast<unsigned>(result));
        g_busy = false;
        return false;
    }

    log_printf("SCREENSHOT", "screenshot begin path=%s", path);
    notify_progress(progress_callback, progress_context);

    uint8_t header[kBmpHeaderSize] = {};
    make_bmp_header(header);
    bool ok = write_exact(&file, header, sizeof(header));
    notify_progress(progress_callback, progress_context);
    for (int y = 0; ok && y < kHeight; y += kChunkRows) {
        const int rows = (kHeight - y) < kChunkRows ? (kHeight - y) : kChunkRows;
        ok = picoment::display::readback_rect_rgb565(0, y, kWidth, rows, g_pixels);
        notify_progress(progress_callback, progress_context);
        if (!ok) {
            log_printf("SCREENSHOT", "screenshot error stage=readback fr=0");
            break;
        }
        log_printf("SCREENSHOT", "screenshot read y=%d rows=%d", y, rows);
        for (int row = 0; ok && row < rows; ++row) {
            convert_row_rgb565_to_bgr24(&g_pixels[row * kWidth]);
            ok = write_exact(&file, g_row, sizeof(g_row));
            notify_progress(progress_callback, progress_context);
        }
    }

    result = f_sync(&file);
    notify_progress(progress_callback, progress_context);
    if (ok && result != FR_OK) {
        ok = false;
        log_printf("SCREENSHOT", "screenshot error stage=write fr=%u", static_cast<unsigned>(result));
    }

    result = f_close(&file);
    notify_progress(progress_callback, progress_context);
    if (ok && result != FR_OK) {
        ok = false;
        log_printf("SCREENSHOT", "screenshot error stage=close fr=%u", static_cast<unsigned>(result));
    }

    if (!ok) {
        f_unlink(path);
        g_busy = false;
        return false;
    }

    log_printf("SCREENSHOT", "screenshot done status=ok path=%s bytes=%lu",
               path,
               static_cast<unsigned long>(kFileBytes));
    g_busy = false;
    return true;
#endif
}

}  // namespace picoment::diagnostics
