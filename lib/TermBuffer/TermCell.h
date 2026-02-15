#pragma once
#include <cstdint>

struct TermCell {
  uint16_t codepoint = ' ';
  uint8_t  attrs     = 0;
  uint8_t  bgBright  = 255;  // Background brightness: 0=black, 255=white

  static constexpr uint8_t ATTR_BOLD    = 0x01;
  static constexpr uint8_t ATTR_INVERSE = 0x02;
  static constexpr uint8_t ATTR_UNDERLINE = 0x04;

  void clear() {
    codepoint = ' ';
    attrs = 0;
    bgBright = 255;
  }

  bool operator==(const TermCell& o) const {
    return codepoint == o.codepoint && attrs == o.attrs && bgBright == o.bgBright;
  }
  bool operator!=(const TermCell& o) const { return !(*this == o); }
};
