#pragma once

// Display dimensions (native landscape)
#define DISPLAY_W 800
#define DISPLAY_H 480

// Font cell size
#define TERM_FONT_W 10
#define TERM_FONT_H 20

// Horizontal margin to avoid bezel clipping
#define TERM_OFFSET_X 10

// Terminal grid (with margin: 10 + 78*10 = 790, leaving 10px right margin)
#define TERM_COLS ((DISPLAY_W - TERM_OFFSET_X * 2) / TERM_FONT_W)  // 78
#define TERM_ROWS (DISPLAY_H / TERM_FONT_H)  // 24

// Display refresh thresholds
#define DIRTY_ROWS_PARTIAL_MAX  5       // Use partial update for <= this many dirty rows
#define FULL_REFRESH_INTERVAL   20      // Full refresh every N fast refreshes
#define MIN_REFRESH_INTERVAL_MS 300     // Minimum ms between display refreshes

// Serial
#define TERM_BAUD 115200

// Tab width
#define TAB_WIDTH 8
