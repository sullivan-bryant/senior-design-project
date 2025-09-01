
/*
	This program uses gnu++17. This must be changed in the Arduino ESP32 core folder:
	1. Locate ESP32 core folder:
		MacOS: ~/Library/Arduino15/packages/esp32/hardware/esp32/<version>/
		Windows: %USERPROFILE%\AppData\Local\Arduino15\packages\esp32\hardware\esp32\<version>\
	2. Create a file named 'platform.local.txt' (same directory as to platform.txt) and add the two lines:
		compiler.cppstd=gnu++17
		compiler.cpp.flags=-std={compiler.cppstd} {compiler.warning_flags}
	3. Restart the IDE, and errors w/ std::optional shouldn't occur.

	The board uses SPIFFS to upload the filesystem located in '/data'. For more info, visit
		https://docs.arduino.cc/tutorials/nano-esp32/spiff/ 
	
*/

#include "WebSocketBridge.h"                                                     // Websocket bridge
#ifndef PRINT_DEBUG                                                 // Macro enabling debugging statements
                                                                    // To disable, just delete these three lines
	#define PRINT_DEBUG                                                          // Define if not already defined
#endif
                                                                                 //
WebSocketBridge ws;                                                              // Websocket bridge object
void setup() {                                                                   // Attempt to setup
	try {
		ws.setup();
	} catch (...) {
		sr::out << "Failed to start WebSocket server." << sr::endl;
		esp_deep_sleep_start();                                                  // Deep sleep if failure
	}
}

void loop() {

}
