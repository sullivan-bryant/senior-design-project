#include <Arduino.h>
#include "WebSocketBridge.h"
#ifndef PRINT_DEBUG
    #define PRINT_DEBUG // Comment this line out to hide debugging.
#endif

WebSocketBridge ws;
void setup() {
    try {
        ws.setup();
    } catch (...) {
        sr::out << "Failed to start WebSocket server. " << sr::endl;
        esp_deep_sleep_start();
    }
}

void loop() {
    ws.loop();
}