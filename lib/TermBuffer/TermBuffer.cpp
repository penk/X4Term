#include "TermBuffer.h"
#include <cstring>

TermBuffer::TermBuffer() {
  for (int r = 0; r < TERM_ROWS; r++)
    clearRow(r);
  markAllDirty();
}

void TermBuffer::clampCursor() {
  if (_curRow < 0) _curRow = 0;
  if (_curRow >= TERM_ROWS) _curRow = TERM_ROWS - 1;
  if (_curCol < 0) _curCol = 0;
  if (_curCol >= TERM_COLS) _curCol = TERM_COLS - 1;
}

void TermBuffer::clearRow(int row) {
  for (int c = 0; c < TERM_COLS; c++)
    _cells[row][c].clear();
  markRowDirty(row);
}

void TermBuffer::clearCell(int row, int col) {
  _cells[row][col].clear();
}

void TermBuffer::putChar(uint16_t cp) {
  // Deferred wrap: if previous char was written at last column,
  // wrap now before placing this character
  if (_wrapPending) {
    _wrapPending = false;
    _curCol = 0;
    lineFeed();
  }
  _cells[_curRow][_curCol].codepoint = cp;
  _cells[_curRow][_curCol].attrs = _attrs;
  _cells[_curRow][_curCol].bgBright = _bgBright;
  markRowDirty(_curRow);
  _curCol++;
  // If we just wrote the last column, defer the wrap
  if (_curCol >= TERM_COLS) {
    _curCol = TERM_COLS - 1;
    _wrapPending = true;
  }
}

void TermBuffer::setCursor(int row, int col) {
  _curRow = row;
  _curCol = col;
  _wrapPending = false;
  clampCursor();
}

void TermBuffer::moveCursorUp(int n) {
  _curRow -= n;
  if (_curRow < 0) _curRow = 0;
  _wrapPending = false;
}

void TermBuffer::moveCursorDown(int n) {
  _curRow += n;
  if (_curRow >= TERM_ROWS) _curRow = TERM_ROWS - 1;
  _wrapPending = false;
}

void TermBuffer::moveCursorForward(int n) {
  _curCol += n;
  if (_curCol >= TERM_COLS) _curCol = TERM_COLS - 1;
  _wrapPending = false;
}

void TermBuffer::moveCursorBack(int n) {
  _curCol -= n;
  if (_curCol < 0) _curCol = 0;
  _wrapPending = false;
}

void TermBuffer::carriageReturn() {
  _curCol = 0;
  _wrapPending = false;
}

void TermBuffer::lineFeed() {
  if (_curRow == _scrollBottom) {
    scrollUp(1);
  } else if (_curRow < TERM_ROWS - 1) {
    _curRow++;
  }
}

void TermBuffer::reverseIndex() {
  if (_curRow == _scrollTop) {
    scrollDown(1);
  } else if (_curRow > 0) {
    _curRow--;
  }
}

void TermBuffer::tab() {
  int nextStop = ((_curCol / TAB_WIDTH) + 1) * TAB_WIDTH;
  if (nextStop >= TERM_COLS) nextStop = TERM_COLS - 1;
  _curCol = nextStop;
  _wrapPending = false;
}

void TermBuffer::backspace() {
  if (_curCol > 0) _curCol--;
  _wrapPending = false;
}

void TermBuffer::eraseLine(int mode) {
  markRowDirty(_curRow);
  switch (mode) {
    case 0:  // cursor to end
      for (int c = _curCol; c < TERM_COLS; c++) clearCell(_curRow, c);
      break;
    case 1:  // start to cursor
      for (int c = 0; c <= _curCol; c++) clearCell(_curRow, c);
      break;
    case 2:  // entire line
      clearRow(_curRow);
      break;
  }
}

void TermBuffer::eraseDisplay(int mode) {
  switch (mode) {
    case 0:  // cursor to end
      eraseLine(0);
      for (int r = _curRow + 1; r < TERM_ROWS; r++) clearRow(r);
      break;
    case 1:  // start to cursor
      for (int r = 0; r < _curRow; r++) clearRow(r);
      eraseLine(1);
      break;
    case 2:  // entire display
      for (int r = 0; r < TERM_ROWS; r++) clearRow(r);
      break;
  }
}

void TermBuffer::setScrollRegion(int top, int bottom) {
  if (top < 0) top = 0;
  if (bottom >= TERM_ROWS) bottom = TERM_ROWS - 1;
  if (top >= bottom) return;
  _scrollTop = top;
  _scrollBottom = bottom;
  _curRow = 0;
  _curCol = 0;
}

void TermBuffer::scrollUp(int n) {
  scrollRegionUp(_scrollTop, _scrollBottom, n);
}

void TermBuffer::scrollDown(int n) {
  scrollRegionDown(_scrollTop, _scrollBottom, n);
}

void TermBuffer::scrollRegionUp(int top, int bottom, int n) {
  if (n <= 0) return;
  if (n > bottom - top + 1) n = bottom - top + 1;
  for (int r = top; r <= bottom - n; r++) {
    memcpy(_cells[r], _cells[r + n], sizeof(TermCell) * TERM_COLS);
    markRowDirty(r);
  }
  for (int r = bottom - n + 1; r <= bottom; r++) {
    clearRow(r);
  }
}

void TermBuffer::scrollRegionDown(int top, int bottom, int n) {
  if (n <= 0) return;
  if (n > bottom - top + 1) n = bottom - top + 1;
  for (int r = bottom; r >= top + n; r--) {
    memcpy(_cells[r], _cells[r - n], sizeof(TermCell) * TERM_COLS);
    markRowDirty(r);
  }
  for (int r = top; r < top + n; r++) {
    clearRow(r);
  }
}

void TermBuffer::insertLines(int n) {
  if (_curRow < _scrollTop || _curRow > _scrollBottom) return;
  scrollRegionDown(_curRow, _scrollBottom, n);
}

void TermBuffer::deleteLines(int n) {
  if (_curRow < _scrollTop || _curRow > _scrollBottom) return;
  scrollRegionUp(_curRow, _scrollBottom, n);
}

void TermBuffer::insertChars(int n) {
  markRowDirty(_curRow);
  for (int c = TERM_COLS - 1; c >= _curCol + n; c--) {
    _cells[_curRow][c] = _cells[_curRow][c - n];
  }
  for (int c = _curCol; c < _curCol + n && c < TERM_COLS; c++) {
    clearCell(_curRow, c);
  }
}

void TermBuffer::deleteChars(int n) {
  markRowDirty(_curRow);
  for (int c = _curCol; c < TERM_COLS - n; c++) {
    _cells[_curRow][c] = _cells[_curRow][c + n];
  }
  for (int c = TERM_COLS - n; c < TERM_COLS; c++) {
    clearCell(_curRow, c);
  }
}

void TermBuffer::saveCursor() {
  _savedRow = _curRow;
  _savedCol = _curCol;
}

void TermBuffer::restoreCursor() {
  _curRow = _savedRow;
  _curCol = _savedCol;
  clampCursor();
}

void TermBuffer::eraseChars(int n) {
  markRowDirty(_curRow);
  for (int c = _curCol; c < _curCol + n && c < TERM_COLS; c++) {
    clearCell(_curRow, c);
  }
}

void TermBuffer::switchScreen(bool alt) {
  if (alt == _altActive) return;

  if (alt) {
    // Save main screen cursor, switch to alt, clear it
    _altSavedRow = _curRow;
    _altSavedCol = _curCol;
    memcpy(_altCells, _cells, sizeof(_cells));
    for (int r = 0; r < TERM_ROWS; r++) clearRow(r);
    _curRow = 0;
    _curCol = 0;
  } else {
    // Restore main screen content and cursor
    memcpy(_cells, _altCells, sizeof(_cells));
    _curRow = _altSavedRow;
    _curCol = _altSavedCol;
    markAllDirty();
  }

  _scrollTop = 0;
  _scrollBottom = TERM_ROWS - 1;
  _wrapPending = false;
  _altActive = alt;
}

void TermBuffer::setAttr(uint8_t attr) { _attrs |= attr; }
void TermBuffer::clearAttr(uint8_t attr) { _attrs &= ~attr; }
void TermBuffer::resetAttrs() { _attrs = 0; _bgBright = 255; }

const TermCell& TermBuffer::cellAt(int row, int col) const {
  return _cells[row][col];
}
