#pragma once
#include "TermCell.h"
#include "term_config.h"

class TermBuffer {
 public:
  TermBuffer();

  // Write character at cursor and advance
  void putChar(uint16_t cp);

  // Cursor movement
  void setCursor(int row, int col);
  void moveCursorUp(int n = 1);
  void moveCursorDown(int n = 1);
  void moveCursorForward(int n = 1);
  void moveCursorBack(int n = 1);
  void carriageReturn();
  void lineFeed();
  void reverseIndex();
  void tab();
  void backspace();

  // Erase
  void eraseLine(int mode);      // 0=to-end, 1=to-start, 2=entire
  void eraseDisplay(int mode);   // 0=to-end, 1=to-start, 2=entire

  // Scroll
  void setScrollRegion(int top, int bottom);
  void scrollUp(int n = 1);
  void scrollDown(int n = 1);

  // Insert/delete
  void insertLines(int n);
  void deleteLines(int n);
  void insertChars(int n);
  void deleteChars(int n);

  // Cursor save/restore
  void saveCursor();
  void restoreCursor();

  // Alternate screen buffer
  void switchScreen(bool alt);
  bool isAltScreen() const { return _altActive; }

  // Erase characters (ECH)
  void eraseChars(int n);

  // Attributes
  void setAttr(uint8_t attr);
  void clearAttr(uint8_t attr);
  void resetAttrs();
  uint8_t currentAttrs() const { return _attrs; }
  void setBgBright(uint8_t b) { _bgBright = b; }

  // Access
  const TermCell& cellAt(int row, int col) const;
  int cursorRow() const { return _curRow; }
  int cursorCol() const { return _curCol; }

  // Dirty tracking
  uint32_t dirtyRows() const { return _dirtyRows; }
  void clearDirty() { _dirtyRows = 0; }
  void markRowDirty(int row) { _dirtyRows |= (1u << row); }
  void markAllDirty() { _dirtyRows = (1u << TERM_ROWS) - 1; }

 private:
  TermCell _cells[TERM_ROWS][TERM_COLS];
  TermCell _altCells[TERM_ROWS][TERM_COLS];  // alternate screen buffer
  int _curRow = 0, _curCol = 0;
  int _savedRow = 0, _savedCol = 0;
  int _altSavedRow = 0, _altSavedCol = 0;   // cursor saved when entering alt screen
  int _scrollTop = 0, _scrollBottom = TERM_ROWS - 1;
  uint8_t _attrs = 0;
  uint8_t _bgBright = 255;    // current background brightness for new chars
  uint32_t _dirtyRows = 0;
  bool _wrapPending = false;  // deferred wrap: cursor at last col, wrap on next char
  bool _altActive = false;    // currently using alternate screen

  void clampCursor();
  void clearRow(int row);
  void clearCell(int row, int col);
  void scrollRegionUp(int top, int bottom, int n);
  void scrollRegionDown(int top, int bottom, int n);
};
