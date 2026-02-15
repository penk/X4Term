#pragma once
#include "TermBuffer.h"
#include "term_config.h"
#include <EInkDisplay.h>

class TermRenderer {
 public:
  TermRenderer(EInkDisplay& display, TermBuffer& buf)
      : _display(display), _buf(buf) {}

  // Render all dirty rows and refresh display
  void renderDirty();

  // Force full-screen render + full refresh (clear ghosting)
  void renderFull();

  // Render cursor at current position (XOR block)
  void renderCursor();

  // Cursor visibility (set from VtParser's DECTCEM state)
  void setCursorVisible(bool v) { _cursorVisible = v; }

 private:
  EInkDisplay& _display;
  TermBuffer& _buf;
  int _fastRefreshCount = 0;
  int _lastCursorRow = -1;
  int _lastCursorCol = -1;
  bool _cursorVisible = true;

  void renderRow(int row);
  void blitGlyph(int px, int py, const uint8_t* glyph, uint8_t bgBright, bool invertGlyph);
};
