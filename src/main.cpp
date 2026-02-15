#include <Arduino.h>
#include "term_config.h"
#include "HalGPIO.h"
#include "TermBuffer.h"
#include "VtParser.h"
#include "TermRenderer.h"
#include <EInkDisplay.h>

// Hardware
static EInkDisplay display(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
static HalGPIO gpio;

// Terminal
static TermBuffer termBuf;
static VtParser parser(termBuf);
static TermRenderer renderer(display, termBuf);

// Refresh rate limiting
static unsigned long lastRefreshMs = 0;

// Send escape sequence for button press
static void sendKey(const char* seq) {
  Serial.print(seq);
}

static void handleButtons() {
  if (gpio.wasPressed(HalGPIO::BTN_UP))      sendKey("\033[A");
  if (gpio.wasPressed(HalGPIO::BTN_DOWN))    sendKey("\033[B");
  if (gpio.wasPressed(HalGPIO::BTN_RIGHT))   sendKey("\033[C");
  if (gpio.wasPressed(HalGPIO::BTN_LEFT))    sendKey("\033[D");
  if (gpio.wasPressed(HalGPIO::BTN_CONFIRM)) sendKey("\r");
  if (gpio.wasPressed(HalGPIO::BTN_BACK))    sendKey("\033");

  // Confirm + Back combo = force full refresh
  if (gpio.isPressed(HalGPIO::BTN_CONFIRM) && gpio.isPressed(HalGPIO::BTN_BACK)) {
    renderer.renderFull();
  }

  // Long press power = deep sleep
  if (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.getHeldTime() > 1500) {
    display.clearScreen(0xFF);
    display.displayBuffer(EInkDisplay::FULL_REFRESH, true);
    display.deepSleep();
    gpio.startDeepSleep();
  }
}

void setup() {
  Serial.setRxBufferSize(4096);  // Prevent overflow during display refresh
  Serial.begin(TERM_BAUD);       // USB CDC - baud rate ignored, always 12Mbps

  gpio.begin();
  display.begin();

  // Clear display to white
  display.clearScreen(0xFF);
  display.displayBuffer(EInkDisplay::FULL_REFRESH, false);

  // Draw initial terminal screen (blank with cursor)
  renderer.renderFull();

  // Print banner to terminal buffer
  const char* lines[] = {
    "Welcome to RobCo Industries (TM) Termlink",
    "Initializing...",
  };
  for (int i = 0; i < 2; i++) {
    for (const char* p = lines[i]; *p; p++) {
      parser.feed(*p);
    }
    parser.feed('\r');
    parser.feed('\n');
  }

  renderer.renderDirty();
}

void loop() {
  // 1. Drain serial input
  while (Serial.available()) {
    uint8_t byte = Serial.read();
    parser.feed(byte);
  }

  // 2. Handle button input
  gpio.update();
  handleButtons();

  // 3. Render if dirty and enough time has passed
  if (termBuf.dirtyRows() != 0) {
    unsigned long now = millis();
    if (now - lastRefreshMs >= MIN_REFRESH_INTERVAL_MS) {
      renderer.setCursorVisible(parser.cursorVisible());
      renderer.renderDirty();
      lastRefreshMs = now;
    }
  }

  // Small delay to batch input and reduce CPU
  delay(5);
}
