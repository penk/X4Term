#include "VtParser.h"
#include <Arduino.h>
#include <cstdio>

void VtParser::resetParams() {
  for (int i = 0; i < MAX_PARAMS; i++) _params[i] = 0;
  _paramCount = 0;
  _questionMark = false;
  _hasIntermediate = false;
  _csiPrefix = 0;
}

int VtParser::param(int idx, int def) const {
  if (idx >= _paramCount) return def;
  return _params[idx] == 0 ? def : _params[idx];
}

void VtParser::feed(uint8_t byte) {
  switch (_state) {
    case State::Ground:    handleGround(byte);  break;
    case State::Escape:    handleEscape(byte);   break;
    case State::CsiEntry:  handleCsiEntry(byte); break;
    case State::CsiParam:  handleCsiParam(byte); break;
    case State::OscString:
      // Consume until ST (ESC \) or BEL
      if (byte == 0x07 || byte == 0x1B) _state = State::Ground;
      break;
    case State::EscSwallow:
      // Swallow one byte (charset designation parameter) and return to ground
      _state = State::Ground;
      break;
  }
}

void VtParser::handleGround(uint8_t byte) {
  // UTF-8 continuation byte
  if (_utf8Remaining > 0) {
    if ((byte & 0xC0) == 0x80) {
      _utf8Cp = (_utf8Cp << 6) | (byte & 0x3F);
      _utf8Remaining--;
      if (_utf8Remaining == 0) {
        _buf.putChar((uint16_t)_utf8Cp);
      }
      return;
    }
    // Invalid continuation - reset and fall through
    _utf8Remaining = 0;
  }

  if (byte == 0x1B) {
    _state = State::Escape;
    return;
  }

  switch (byte) {
    case 0x07: break;                 // BEL - ignore
    case 0x08: _buf.backspace(); break;  // BS
    case 0x09: _buf.tab(); break;        // HT
    case 0x0A: // LF
    case 0x0B: // VT
    case 0x0C: // FF
      _buf.lineFeed();
      break;
    case 0x0D: _buf.carriageReturn(); break;  // CR
    default:
      if (byte >= 0x20 && byte < 0x7F) {
        _buf.putChar(byte);
      } else if (byte >= 0xC0 && byte < 0xE0) {
        // UTF-8 2-byte sequence start
        _utf8Cp = byte & 0x1F;
        _utf8Remaining = 1;
      } else if (byte >= 0xE0 && byte < 0xF0) {
        // UTF-8 3-byte sequence start (BMP: U+0800-U+FFFF)
        _utf8Cp = byte & 0x0F;
        _utf8Remaining = 2;
      } else if (byte >= 0xF0 && byte < 0xF8) {
        // UTF-8 4-byte sequence start (>BMP, will truncate to 16-bit)
        _utf8Cp = byte & 0x07;
        _utf8Remaining = 3;
      }
      break;
  }
}

void VtParser::handleEscape(uint8_t byte) {
  switch (byte) {
    case '[':
      _state = State::CsiEntry;
      resetParams();
      break;
    case ']':
      _state = State::OscString;
      break;
    case 'D':  // IND - index (move down, scroll if at bottom)
      _buf.lineFeed();
      _state = State::Ground;
      break;
    case 'M':  // RI - reverse index (move up, scroll if at top)
      _buf.reverseIndex();
      _state = State::Ground;
      break;
    case '7':  // DECSC - save cursor
      _buf.saveCursor();
      _state = State::Ground;
      break;
    case '8':  // DECRC - restore cursor
      _buf.restoreCursor();
      _state = State::Ground;
      break;
    case 'c':  // RIS - full reset
      _buf.eraseDisplay(2);
      _buf.setCursor(0, 0);
      _buf.resetAttrs();
      _buf.setScrollRegion(0, TERM_ROWS - 1);
      _state = State::Ground;
      break;
    case '=':  // DECKPAM - keypad application mode (ignore)
    case '>':  // DECKPNM - keypad numeric mode (ignore)
      _state = State::Ground;
      break;
    case '(':  // G0 charset designation - consume next byte
    case ')':  // G1 charset designation - consume next byte
    case '#':  // DEC line drawing - consume next byte
    case '*':  // G2 charset designation - consume next byte
    case '+':  // G3 charset designation - consume next byte
      _state = State::EscSwallow;
      break;
    default:
      _state = State::Ground;
      break;
  }
}

void VtParser::handleCsiEntry(uint8_t byte) {
  if (byte == '?') {
    _questionMark = true;
    _state = State::CsiParam;
    return;
  }
  // Handle other parameter prefix bytes (>, =, <)
  if (byte == '>' || byte == '=' || byte == '<') {
    _csiPrefix = byte;
    _state = State::CsiParam;
    return;
  }
  // Fall through to param handling
  _state = State::CsiParam;
  handleCsiParam(byte);
}

void VtParser::handleCsiParam(uint8_t byte) {
  if (byte >= '0' && byte <= '9') {
    if (_paramCount == 0) _paramCount = 1;
    _params[_paramCount - 1] = _params[_paramCount - 1] * 10 + (byte - '0');
    return;
  }
  if (byte == ';') {
    if (_paramCount < MAX_PARAMS) _paramCount++;
    return;
  }
  // Intermediate bytes (0x20-0x2F: space ! " # $ % & ' ( ) * + , - . /)
  // Collect but mark as having intermediates so we can ignore unsupported sequences
  if (byte >= 0x20 && byte <= 0x2F) {
    _hasIntermediate = true;
    return;
  }
  // Dispatch final byte (0x40-0x7E)
  if (byte >= 0x40 && byte <= 0x7E) {
    // Dispatch if no intermediates and no unrecognized prefix
    // _questionMark sequences go through dispatchCsi which handles them
    if (!_hasIntermediate && _csiPrefix == 0) {
      dispatchCsi(byte);
    }
    // Sequences with intermediates or prefixes (like ESC[>c) are
    // silently consumed — the final byte ends the sequence cleanly
    _state = State::Ground;
    return;
  }
  // Unknown byte - abort sequence cleanly
  _state = State::Ground;
}

void VtParser::dispatchCsi(uint8_t cmd) {
  int n = param(0, 1);

  if (_questionMark) {
    int mode = param(0, 0);
    switch (cmd) {
      case 'h':  // DECSET
        switch (mode) {
          case 25: _cursorVisible = true; break;    // DECTCEM show cursor
          case 47:    // alt screen (simple)
          case 1047:  // alt screen
          case 1049:  // alt screen + save cursor
            if (mode == 1049) _buf.saveCursor();
            _buf.switchScreen(true);
            break;
        }
        break;
      case 'l':  // DECRST
        switch (mode) {
          case 25: _cursorVisible = false; break;   // DECTCEM hide cursor
          case 47:
          case 1047:
          case 1049:
            _buf.switchScreen(false);
            if (mode == 1049) _buf.restoreCursor();
            break;
        }
        break;
    }
    return;
  }

  switch (cmd) {
    case 'A': _buf.moveCursorUp(n); break;       // CUU
    case 'B': _buf.moveCursorDown(n); break;      // CUD
    case 'C': _buf.moveCursorForward(n); break;   // CUF
    case 'D': _buf.moveCursorBack(n); break;      // CUB
    case 'E':  // CNL - cursor next line
      _buf.moveCursorDown(n);
      _buf.carriageReturn();
      break;
    case 'F':  // CPL - cursor previous line
      _buf.moveCursorUp(n);
      _buf.carriageReturn();
      break;
    case 'G':  // CHA - cursor horizontal absolute
      _buf.setCursor(_buf.cursorRow(), param(0, 1) - 1);
      break;
    case 'H':  // CUP - cursor position
    case 'f':  // HVP - same as CUP
      _buf.setCursor(param(0, 1) - 1, param(1, 1) - 1);
      break;
    case 'J': _buf.eraseDisplay(param(0, 0)); break;  // ED
    case 'K': _buf.eraseLine(param(0, 0)); break;      // EL
    case 'L': _buf.insertLines(n); break;   // IL
    case 'M': _buf.deleteLines(n); break;   // DL
    case 'P': _buf.deleteChars(n); break;   // DCH
    case '@': _buf.insertChars(n); break;   // ICH
    case 'S': _buf.scrollUp(n); break;      // SU
    case 'T': _buf.scrollDown(n); break;    // SD
    case 'd':  // VPA - vertical position absolute
      _buf.setCursor(param(0, 1) - 1, _buf.cursorCol());
      break;
    case 'm': handleSgr(); break;            // SGR
    case 'r':  // DECSTBM - set scroll region
      _buf.setScrollRegion(param(0, 1) - 1, param(1, TERM_ROWS) - 1);
      break;
    case 'n':  // DSR - device status report
      if (param(0, 0) == 6) {
        // Cursor position report - send response over serial
        // ESC [ row ; col R (1-based)
        char resp[20];
        snprintf(resp, sizeof(resp), "\033[%d;%dR",
                 _buf.cursorRow() + 1, _buf.cursorCol() + 1);
        Serial.print(resp);
      }
      break;
    case 's': _buf.saveCursor(); break;     // ANSI save cursor
    case 'u': _buf.restoreCursor(); break;  // ANSI restore cursor
    case 'X': _buf.eraseChars(n); break;    // ECH - erase characters
    case 'c':  // DA - device attributes
      Serial.print("\033[?1;0c");  // VT100 with no options
      break;
  }
}

// Standard ANSI color palette: approximate luminance (0-255) for colors 0-15
static const uint8_t kAnsiLum[16] = {
    0,  // 0: black
   76,  // 1: red
  149,  // 2: green
  226,  // 3: yellow
   29,  // 4: blue
  105,  // 5: magenta
  178,  // 6: cyan
  200,  // 7: white (light gray)
  128,  // 8: bright black (dark gray)
  128,  // 9: bright red
  192,  // 10: bright green
  255,  // 11: bright yellow
   80,  // 12: bright blue
  160,  // 13: bright magenta
  224,  // 14: bright cyan
  255,  // 15: bright white
};

// Compute luminance from 256-color palette index
static uint8_t lum256(int n) {
  if (n < 16) return kAnsiLum[n];
  if (n >= 232) {
    // Grayscale ramp: 232=dark(8), 255=light(238)
    return 8 + (n - 232) * 10;
  }
  // 6x6x6 color cube (indices 16-231)
  int idx = n - 16;
  int b5 = idx % 6;
  int g5 = (idx / 6) % 6;
  int r5 = idx / 36;
  // Map 0-5 to 0,95,135,175,215,255
  int r = r5 ? r5 * 40 + 55 : 0;
  int g = g5 ? g5 * 40 + 55 : 0;
  int b = b5 ? b5 * 40 + 55 : 0;
  return (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
}

// Compute luminance from RGB
static uint8_t lumRGB(int r, int g, int b) {
  return (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
}

void VtParser::handleSgr() {
  if (_paramCount == 0) {
    _buf.resetAttrs();
    return;
  }

  for (int i = 0; i < _paramCount; i++) {
    int p = _params[i];
    switch (p) {
      case 0:  _buf.resetAttrs(); break;
      case 1:  _buf.setAttr(TermCell::ATTR_BOLD); break;
      case 2:  _buf.clearAttr(TermCell::ATTR_BOLD); break;  // dim
      case 4:  _buf.setAttr(TermCell::ATTR_UNDERLINE); break;
      case 7:  _buf.setAttr(TermCell::ATTR_INVERSE); break;
      case 22: _buf.clearAttr(TermCell::ATTR_BOLD); break;
      case 24: _buf.clearAttr(TermCell::ATTR_UNDERLINE); break;
      case 27: _buf.clearAttr(TermCell::ATTR_INVERSE); break;

      // Basic foreground colors (dark) - no attribute change
      case 30: case 31: case 32: case 33:
      case 34: case 35: case 36: case 37:
        break;
      case 39: break;  // Default foreground

      // Basic background colors - map to brightness
      case 40: _buf.setBgBright(kAnsiLum[0]); break;   // black bg
      case 41: _buf.setBgBright(kAnsiLum[1]); break;   // red bg
      case 42: _buf.setBgBright(kAnsiLum[2]); break;   // green bg
      case 43: _buf.setBgBright(kAnsiLum[3]); break;   // yellow bg
      case 44: _buf.setBgBright(kAnsiLum[4]); break;   // blue bg
      case 45: _buf.setBgBright(kAnsiLum[5]); break;   // magenta bg
      case 46: _buf.setBgBright(kAnsiLum[6]); break;   // cyan bg
      case 47: _buf.setBgBright(kAnsiLum[7]); break;   // white bg
      case 49: _buf.setBgBright(255); break;            // default bg (white)

      // Bright foreground - map to bold
      case 90: case 91: case 92: case 93:
      case 94: case 95: case 96: case 97:
        _buf.setAttr(TermCell::ATTR_BOLD);
        break;

      // Bright background - map to brightness
      case 100: _buf.setBgBright(kAnsiLum[8]); break;
      case 101: _buf.setBgBright(kAnsiLum[9]); break;
      case 102: _buf.setBgBright(kAnsiLum[10]); break;
      case 103: _buf.setBgBright(kAnsiLum[11]); break;
      case 104: _buf.setBgBright(kAnsiLum[12]); break;
      case 105: _buf.setBgBright(kAnsiLum[13]); break;
      case 106: _buf.setBgBright(kAnsiLum[14]); break;
      case 107: _buf.setBgBright(kAnsiLum[15]); break;

      // Extended foreground color: 38;5;N or 38;2;R;G;B
      case 38:
        if (i + 1 < _paramCount && _params[i + 1] == 5) {
          // 256-color: 38;5;N
          if (i + 2 < _paramCount) {
            int n = _params[i + 2];
            if (n >= 8 && n < 16) _buf.setAttr(TermCell::ATTR_BOLD);
          }
          i += 2;
        } else if (i + 1 < _paramCount && _params[i + 1] == 2) {
          // RGB: 38;2;R;G;B
          if (i + 4 < _paramCount) {
            uint8_t lum = lumRGB(_params[i + 2], _params[i + 3], _params[i + 4]);
            if (lum > 150) _buf.setAttr(TermCell::ATTR_BOLD);
          }
          i += 4;
        }
        break;

      // Extended background color: 48;5;N or 48;2;R;G;B
      case 48:
        if (i + 1 < _paramCount && _params[i + 1] == 5) {
          // 256-color: 48;5;N
          if (i + 2 < _paramCount) {
            _buf.setBgBright(lum256(_params[i + 2]));
          }
          i += 2;
        } else if (i + 1 < _paramCount && _params[i + 1] == 2) {
          // RGB: 48;2;R;G;B
          if (i + 4 < _paramCount) {
            _buf.setBgBright(lumRGB(_params[i + 2], _params[i + 3], _params[i + 4]));
          }
          i += 4;
        }
        break;

      default: break;
    }
  }
}
