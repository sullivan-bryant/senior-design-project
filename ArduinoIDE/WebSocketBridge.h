/*----------------------------------------------------------------------------------------------------------------------
 *  BME:4920 - Biomedical Engineering Senior Design II
 *  Team 13 | Remote Hand Exoskeleton
 *  Sullivan Bryant, Charley Dunham, Jared Gilliam
 *
 *
 *
 *----------------------------------------------------------------------------------------------------------------------*/

#pragma once
#include <queue>                // Standard C++ queue library–queueing requests (FIFO)
#include <ArduinoJson.h>        // JSON parsing library
#include <ESPAsyncWebServer.h>  // Web server library
#include <SPIFFS.h>             // File system library
// Custom classes
#include "SerialStream.h"       // Serial stream header—easier Serial monitoring/debugging
#include "ServoController.h"    // Servo controller class
#include "FlexSensor.h"         // Flex sensor class
// Use arduino board mapping
#ifndef BOARD_HAS_PIN_REMAP
    #define BOARD_HAS_PIN_REMAP
#endif
/*
 *  Class for bridging the JSON commands to a device, serving web controller, and
 *  managing the Wi-Fi station. Encapsulates all device functionality. All the main.cpp
 *  file needs to do is include the class, call the setup() method, and call the loop() method.
 *  -----!! WARNING !!-----
 *  The setup() method may throw an exception if SPIFFS fails. Make sure to nest the setup() in a
 *  try-catch. Ex.:
 *                   #include "WebSocketBridge.h"
 *                   WebSocketBridge ws;
 *                   void setup() {
 *                       try {
 *                           ws.setup();
 *                       } catch (...) {
 *                           sr::out << "Failed to start WebSocket server. " << sr::endl;
 *                           esp_deep_sleep_start();
 *                       }
 *                   }
 */
class WebSocketBridge {
public:
    // No-argument constructor.
    WebSocketBridge();
    // =======================================================================================
    //                                  Public methods
    void setup();       // Call once in setup()
    void loop();        // Place once in loop()
private:
    // =======================================================================================
    //                                  Private types
    /* ------ DEVICE TYPES ------
     * An enumeration defining types of devices.
     */
    enum class Device {
        Servo,              // Servo motor
        Flex,               // Represents static flex-sensor properties (applies to all)
        Flex_2,             // Index flex sensor
        Flex_3,             // Middle flex sensor
        Flex_4,             // Ring flex sensor
        Flex_5,             // Pinky flex sensor
        INVALID_DEV         // Not a valid device
    };
    // Method types
    /* ------ METHOD TYPES ------
     * Only get/set methods permitted for various attributes of sensors/servos.
     */
    enum class Method {
        GET,                // Get a device's attribute
        SET,                // Set a device's attribute
        INVALID_METHOD      // Not a valid method
    };

    /* ------ SERVO MOTOR ATTRIBUTES ------
     * These are used to modify properties of the servo motor, providing flexibility with
     * various motor types. Refer to the data sheet to specify PWM signal constrains. The
     * default values are for a 270º servo w/ 270º rotation @ 500 – 2500 µs.
     */
    enum class ServoAttr {
        AngleStep,   /* <int> */                        // Angle increment (º) per invocation of timer.
        TimeDelayUS, /* <unsigned long> */              // Time delay (µs) between angle increments.
        MinPWM,      /* <unsigned long> */              // Minimum pulse-width (µs) of PWM signal (can be found in servo's data sheet).
        MaxPWM,      /* <unsigned long> */              // Maximum pulse-width (µs) of PWM signal (also in data sheet).
        Position,    /* <unsigned long> */              // Angular position of motor (º)
        Pin,         /* <uint8_t> */                    // Set pin which motor is connected to.
        Actuate,     /* <bool> */                       // Enable/disable actuation of servo when in a controlled-speed state.
        StartAngle,  /* <unsigned int> */               // The angle which the motor starts actuation during a controlled-speed state.
        StopAngle,   /* <unsigned int> */               // The angle which the motor stops actuation during controlled motion.
        Motion,      /* <ServoController::Motion> */    // The motion mode of servo (LOOP/SWEEP/ONE_SHOT) (see 'ServoController.h').
        MaxAngle,    /* <unsigned int> */               // The maximum angle used for duty-cycle calculation.
        INVALID_SERVO_ATTR                              // Default, invalid value.
    };
    /* ------ STATIC FLEX SENSOR ATTRIBUTES ------
     * These values are used to modify attributes belonging to the flex-sensor class itself, as they're
     * a property of the class, not any particular flex sensor device. For example, the sampling rate for
     * a flex-sensor device should be the same for all devices (unless you'd like to change it).
     */
    enum class FlexAttr {
        SampleRate,  /* <uint64_t> */                   // The sampling interval for which the flag indicating to acquire a new reading is invoked.
        Start,       /* value not required */           // Enable sampling.
        Stop,        /* value not required */           // Disable sampling.
        INVALID_FLEX_ATTR                               // Invalid value for a static flex-sensor attribute.
    };
    /* ------ INSTANCE-BASED FLEX SENSOR ATTRIBUTES ------
     * For now, only the pin for which the sensor's attached is an instance-based attribute which can be modified.
     */
    enum class FlexNAttr {
        Pin,                                            // Pin which to connect the sensor to. Must be a valid ADC pin.
        INVALID_FLEX_N_ATTR                             // Invalid value for an instance of a flex sensor.
    };
    /* ------ STATUS CODES ------
     * These are the codes sent to the client upon set requests, indicating whether their set-attribute
     * call was successful or not. These aren't included in responses to get requests as they represent
     * responses to external modifications.
     */
    enum Status {
        OK,                                             // Successful set request.
        ERROR                                           // Failed set request.
    };
    // =======================================================================================
    //                                  Private fields
    /* ------ SERVER/CLIENT INTERACTION -------
     * Asynchronous web servers and web sockets allow for multiple client connections. These two fields are externally sourced.
     * - The asynchronous web server can serve webpages hosted locally, which, for my particular Nano, was http://192.168.4.1/
     * - An asynchronous web socket can bind to the server's port, but uses the '/ws' endpoint to distinguish it from regular
     *   HTTP requests. This allows for bidirectional communication and is fairly simple to implement on the client side as
     *   WebSocket is a native JavaScript library.
     *  >> repo (socket and server): https://github.com/ESP32Async/ESPAsyncWebServer.git
     */
    AsyncWebSocket       ws_;                           // Instance of an asynchronous web-socket.
    AsyncWebServer      server_;                        // Instance of an asynchronous web server.
    /* ------ DATA FORMATTING/MANAGING ------
     * Two JSON documents declared class-scoped to reduce heap fragmentation on an embedded system like the Nano ESP32. This is
     * due to the latest ArduinoJson library version dynamically allocating everything.
     */
    JsonDocument inBuffer;                              // Store input data.
    JsonDocument outBuffer;                             // Store output data.
    std::queue<std::string> received;                   // Queued std::strings representing received, unparsed data.
    /* ------ DEVICES ------
     * Five devices total are declared—a servo motor for flexion controlling and four flex sensors, one for each
     * finger, excluding the thumb.
     */
    ServoController servo_;                             // Instance of a servo motor.
    FlexSensor sensors[4];                              // Four flex sensors.
    // =======================================================================================
    //                                  Private methods
    /* ------ Callback for websocket-related events ------
     *  This method handles events where,
     *      - a client connects -> call to send initial data,
     *      - a client disconnects -> stop all streaming if no clients connected,
     *      - a client sends data -> data is reinterpreted and queued,
     *      - a client pongs -> server pings all clients.
     *  This method also matches the signature required for the onEvent method in the AsyncWebSocket class.
     */
    void onWsEvent(AsyncWebSocket *server,              // Pointer of the server which the socket is bound to.
                   AsyncWebSocketClient *client,        // Pointer of the client which triggered the event.
                   AwsEventType type,                   // Event-type specifier.
                   void *arg,                           // Callback arguments cast to a generic pointer.
                   uint8_t *data,                       // Pointer to the data array received.
                   size_t len) ;                        // Length of the data array.
    /* ------ Callback for emitting servo position readings ------
     * This method is called when the servo's angle changes but only angle-changes invoked by the esp_timer
     * governing the servo's speed.
     */
    void emitServoAngle(
        int angle);                                     // New angle reading

    /* ------ Callback for emitting a sensor's ADC reading ------
     * This method is invoked after the sampling esp_timer flags for a new reading, and the
     * reading is collected. Actual invoking happens in the sensor's loop method.
     */
    void emitSensorReading(
        uint16_t value,                                 // ADC 16-bit value (0 - 4095).
        const char* name);                              // Name of the sensor emitting reading.

    /* ------ Helper for sending an invalid request ------
     * This helper method is called throughout the parsing of the program to notify the client
     * that an invalid request was made. This response is only sent from errors due to changing
     * device attributes, not due to the connection.
     */
    void sendInvalidRequest(
        AsyncWebSocketClient* client);                  // Client which made the invalid request.
    /* ------ Helper for sending a response from a set request ------
     * This method is called when a request to change an attribute is made.
     */
    void sendSetResponse(
        AsyncWebSocketClient* client,                   // Client initially making the set request
        const Status &status);                          // Status of the request
    /* ------ Helper for sending a response indicating an invalid attribute ------
     * This method is sent when a client makes a request to change or get an attribute
     * not owned by any of the devices.
     */
    void sendInvalidAttr(
        AsyncWebSocketClient *client);                  // Client making the invalid set/get request
    /* ------ Helper for sending a response to a get request ------
     * The templated argument provides flexibility governed by ArduinoJson's library to
     * convert the type into a sendable field.
     */
    template <typename T>                               // Generic type compatible w/ ArduinoJson
    void sendGetResponse(
        const char* device,                             // Device name as a character array (string)
        const char* attr,                               // Attribute field as a character array (string)
        const T& val);                                  // Value to send
    /* ------ Helper for handling when a client connects ------
     * This method sends all the initial values of all the devices as a response to a getter request.
     */
    void handleConnect(
        AsyncWebSocketClient *client);                  // Pointer to the client that connected.

    Device parseDevice();                               // Method for parsing the device field in the inBuffer JSON doc.
    Method parseMethod();                               // Method for parsing the request field in the inBuffer JSON doc.
    ServoAttr parseServoAttr();                         // Method for parsing servo attributes from the inBuffer.
    FlexAttr parseFlexAttr();                           // Method for parsing static flex sensor attributes from the inBuffer.
    FlexNAttr parseFlexNAttr();                         // Helper method for parsing instance-based flex sensor attributes.
    /* ------ Helper method for parsing queued data ------
     * This is the monster method that parses all the fields in the inBuffer JsonDocument. It is a nasty method
     * but optimizes performance by performing c-string operations, tree search patterns, and switch statements.
     */
    void handleReceived(
        const char* message);
};
