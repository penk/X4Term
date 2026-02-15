#pragma once
#include "TermBuffer.h"
#include <cstdint>

class VtParser {
 public:
  VtParser(TermBuffer& buf) : _buf(buf) {}

  // Feed one byte from serial input
  void feed(uint8_t byte);

  // Cursor visibility (controlled by DECTCEM ?25h/l)
  bool cursorVisible() const { return _cursorVisible; }

 private:
  TermBuffer& _buf;

  enum class State {
    Ground,
    Escape,
    CsiEntry,
    CsiParam,
    OscString,
    EscSwallow,  // consume one byte after ESC( ESC) ESC#
  };

  State _state = State::Ground;

  static constexpr int MAX_PARAMS = 16;
  int _params[MAX_PARAMS] = {};
  int _paramCount = 0;
  bool _questionMark = false;   // for DEC private modes (ESC [ ?)
  bool _hasIntermediate = false; // CSI sequence has intermediate bytes (0x20-0x2F)
  char _csiPrefix = 0;          // CSI parameter prefix: '>', '=', '<', etc.

  void handleGround(uint8_t byte);
  void handleEscape(uint8_t byte);
  void handleCsiEntry(uint8_t byte);
  void handleCsiParam(uint8_t byte);
  void dispatchCsi(uint8_t cmd);
  void handleSgr();

  int param(int idx, int def = 0) const;
  void resetParams();

  bool _cursorVisible = true;

  // UTF-8 decoder state
  uint32_t _utf8Cp = 0;
  int _utf8Remaining = 0;
};
