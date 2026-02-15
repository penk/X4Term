#include "TermRenderer.h"
#include "TermCell.h"
#include "term_font_10x20.h"

// 4x4 Bayer dithering matrix (threshold values 0-15)
static const uint8_t kBayer4x4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
};

void TermRenderer::blitGlyph(int px, int py, const uint8_t* glyph,
                              uint8_t bgBright, bool invertGlyph) {
  uint8_t* fb = _display.getFrameBuffer();
  constexpr int fbStride = DISPLAY_W / 8;  // 100 bytes per row

  for (int gy = 0; gy < TERM_FONT_H; gy++) {
    int fbY = py + gy;
    if (fbY >= DISPLAY_H) break;

    for (int gx = 0; gx < TERM_FONT_W; gx++) {
      int fbX = px + gx;
      if (fbX >= DISPLAY_W) break;

      // Read glyph bit (MSB-first)
      int glyphByteIdx = gy * TermFont::BYTES_PER_ROW + (gx / 8);
      int glyphBitIdx = 7 - (gx % 8);
      bool isGlyphPixel = (pgm_read_byte(&glyph[glyphByteIdx]) >> glyphBitIdx) & 1;

      bool drawBlack;
      if (isGlyphPixel) {
        // Foreground: black normally, white on dark backgrounds
        drawBlack = !invertGlyph;
      } else {
        // Background: Bayer-dithered based on brightness
        // bgBright=255 → all white, bgBright=0 → all black
        int threshold = kBayer4x4[gy & 3][gx & 3];  // 0-15
        int level = (bgBright * 17) >> 8;            // 0-16
        drawBlack = (level <= threshold);
      }

      // Write to framebuffer (bit=1 → white, bit=0 → black)
      int fbByteIdx = fbY * fbStride + (fbX / 8);
      int fbBitIdx = 7 - (fbX % 8);
      if (drawBlack) {
        fb[fbByteIdx] &= ~(1 << fbBitIdx);
      } else {
        fb[fbByteIdx] |= (1 << fbBitIdx);
      }
    }
  }
}

void TermRenderer::renderRow(int row) {
  for (int col = 0; col < TERM_COLS; col++) {
    const TermCell& cell = _buf.cellAt(row, col);
    const uint8_t* glyph = TermFont::getGlyph(cell.codepoint);

    // Determine effective background brightness
    uint8_t bgBright = cell.bgBright;
    bool isInverse = (cell.attrs & TermCell::ATTR_INVERSE) != 0;
    if (isInverse) bgBright = 255 - bgBright;

    // Invert glyph when background is dark (for readability)
    bool invertGlyph = bgBright < 128;

    blitGlyph(TERM_OFFSET_X + col * TERM_FONT_W, row * TERM_FONT_H,
              glyph, bgBright, invertGlyph);
  }
}

void TermRenderer::renderDirty() {
  uint32_t dirty = _buf.dirtyRows();

  // Always include the previous cursor row so the old cursor gets erased
  if (_lastCursorRow >= 0) {
    dirty |= (1u << _lastCursorRow);
  }

  if (dirty == 0) return;

  int dirtyCount = __builtin_popcount(dirty);

  // Render all dirty rows into framebuffer (this erases old cursor too)
  for (int row = 0; row < TERM_ROWS; row++) {
    if (dirty & (1u << row)) {
      renderRow(row);
    }
  }

  // Draw cursor at new position
  renderCursor();

  if (dirtyCount > DIRTY_ROWS_PARTIAL_MAX) {
    // Many rows changed: full-screen fast refresh
    _display.displayBuffer(EInkDisplay::FAST_REFRESH);
    _fastRefreshCount++;
  } else {
    // Few rows changed: windowed partial update
    int minRow = __builtin_ctz(dirty);
    int maxRow = 31 - __builtin_clz(dirty);

    // Also include cursor row in the window
    int curRow = _buf.cursorRow();
    if (curRow < minRow) minRow = curRow;
    if (curRow > maxRow) maxRow = curRow;
    // Include previous cursor position too
    if (_lastCursorRow >= 0) {
      if (_lastCursorRow < minRow) minRow = _lastCursorRow;
      if (_lastCursorRow > maxRow) maxRow = _lastCursorRow;
    }

    int y = minRow * TERM_FONT_H;
    int h = (maxRow - minRow + 1) * TERM_FONT_H;
    _display.displayWindow(0, y, DISPLAY_W, h);
    _fastRefreshCount++;
  }

  // Periodic full refresh to clear ghosting
  if (_fastRefreshCount >= FULL_REFRESH_INTERVAL) {
    _display.displayBuffer(EInkDisplay::FULL_REFRESH);
    _fastRefreshCount = 0;
  }

  _lastCursorRow = _buf.cursorRow();
  _lastCursorCol = _buf.cursorCol();
  _buf.clearDirty();
}

void TermRenderer::renderFull() {
  _buf.markAllDirty();
  for (int row = 0; row < TERM_ROWS; row++) {
    renderRow(row);
  }
  renderCursor();
  _display.displayBuffer(EInkDisplay::FULL_REFRESH);
  _fastRefreshCount = 0;
  _lastCursorRow = _buf.cursorRow();
  _lastCursorCol = _buf.cursorCol();
  _buf.clearDirty();
}

void TermRenderer::renderCursor() {
  if (!_cursorVisible) return;

  // Draw cursor as inverted block at current position
  int row = _buf.cursorRow();
  int col = _buf.cursorCol();

  if (col >= TERM_COLS) col = TERM_COLS - 1;

  const TermCell& cell = _buf.cellAt(row, col);
  const uint8_t* glyph = TermFont::getGlyph(cell.codepoint);

  // Cursor: invert the cell's effective background
  uint8_t bgBright = cell.bgBright;
  bool isInverse = (cell.attrs & TermCell::ATTR_INVERSE) != 0;
  if (isInverse) bgBright = 255 - bgBright;
  // Flip for cursor
  bgBright = 255 - bgBright;
  bool invertGlyph = bgBright < 128;

  blitGlyph(TERM_OFFSET_X + col * TERM_FONT_W, row * TERM_FONT_H,
            glyph, bgBright, invertGlyph);
}
