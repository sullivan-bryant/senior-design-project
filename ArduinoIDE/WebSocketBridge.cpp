/*  BME:4920 - Biomedical Engineering Senior Design II
 *  Team 13 - Remote Hand Exoskeleton | Final Prototype
 *  Sullivan Bryant, Charley Dunham, Jared Gilliam
 *  -------------------- -------------------- --------------------
 *                        WebSocketBridge.cpp
 *  Class for bridging connection between arduino and client.
 *  This is the .cpp file which parses JSON messages from the
 *  client, executes the commands associated with them, and
 *  responds to the client.
 */

#include "WebSocketBridge.h"

/**
 * No-argument constructor, initializing the
 *  AsyncWebServer  \code server_ \endcode
 *  @see AsyncWebServer
 */
WebSocketBridge::WebSocketBridge() : server_(80),
                                     ws_{"/ws"},
                                     servo_{D4},
                                     sensors {
                                         FlexSensor("FLEX_2"),
                                         FlexSensor("FLEX_3"),
                                         FlexSensor("FLEX_4"),
                                         FlexSensor("FLEX_5")
                                     }
{}

/*
 * Setup method for the websocket bridge. Initializes the serial port, the SPIFFS file system,
 * the Wi-Fi access-point, the server, and the web socket.
 */
void WebSocketBridge::setup() {
    Serial.begin(115200);                                                               // Start serial monitor for debugging
    delay(1000);                                                                        // Wait for the serial port
    sr::debug << "Last reset reason: " << esp_reset_reason() << sr::endl;               // debug the last reset reason
    if (!SPIFFS.begin(true)) throw std::runtime_error("Failed to mount SPIFFS");        // throw a runtime error if SPIFFS fails
    WiFiClass::mode(WIFI_AP);                                                           // set the wifi mode to access point
    WiFi.softAP("RemoteExoskeleton", "remoteExoskeleton");                              // set the ssid/pass of access point
    server_.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");                 // serve default index.html page
    server_.serveStatic("/script.js", SPIFFS, "/script.js");                            // serve the javascript page
    server_.serveStatic("/style.css", SPIFFS, "/style.css");                            // serve the styling page
    server_.serveStatic("/smoothie.js", SPIFFS, "/smoothie.js");                        // serve the external library for a javascript graph
    server_.onNotFound([](auto *req) {
        req->send(404);                                                                 // send a 404 to domains not found
    });

    server_.addHandler(&ws_);                                                           // bind socket to server port
    
    ws_.onEvent([this](                                                           // Signature must match AsyncWebSocket
        AsyncWebSocket *s,                                                              // Pointer to AsyncWebSocket object invoking event
        AsyncWebSocketClient *c,                                                        // Pointer to client from which event occurred
        AwsEventType t,                                                                 // 
        void *a,                                                                        //
        uint8_t *d,     
        size_t l) { 
        this->onWsEvent(s, c, t, a, d, l); // invoke callback via lambda for websocket events   
    });
    servo_.setup(); // setup the servo motor    
    server_.begin(); // setup the server    
    ws_.enable(true); // enable the web socket  
    servo_.addAngleNotify([this](int pos) -> void { // register servo angle listener    
        this->emitServoAngle(pos); // call to servo angle emitter   
    });
    int i = 17; // starting at pin A0, setup all sensors
    for (auto &sensor : sensors) { // for every sensor in the array...
        sensor.setup(); // setup the sensor
        sensor.setPin(i); // set the pin to the local 'i'
        sensor.setFinger(static_cast<FlexSensor::Finger>(i - 15)); // set the sensor's finger to be enumerated value from difference of pin and 15 (starting at 2)
        if (sensor.setupFailed()) {
            sr::out << "Failed to setup sensor " << i - 16 << " on pin " << i << sr::endl; // notify user of sensor failing to setup
        }
        sensor.setNotifier([this](uint16_t val, const char *name) -> void { // register instance-method of sensor readings
            emitSensorReading(val, name);
        });
    }
}

void WebSocketBridge::loop() {
    while (!received.empty()) {
        auto request = received.front(); // process all queued requests
        handleReceived(request.c_str()); // call to parser
        received.pop(); // pop from queue
    }
    ws_.cleanupClients(); // clean up all clients
    servo_.loop(); // allow servo to actuate if enabled
    for (auto &sensor : sensors) {
        sensor.loop(); // allow sensors to notify / sample
    }
    delay(1); // prevent explosions
}
/*
 * Private helper to send a client an invalid request from the last received
 * values contained in the inputBuffer.
 */
void WebSocketBridge::sendInvalidRequest(AsyncWebSocketClient *client) {
    char buf[200]; // 200-size fixed array allocation
    outBuffer.clear(); // clear output doc
    outBuffer["error"] = "Invalid request"; // add/set error field to invalid request
    outBuffer["details"] = inBuffer["req"].as<const char *>() != nullptr ? inBuffer["req"].as<const char *>() : "null"; // details as request (null if omitted)
    const size_t n = serializeJson(outBuffer, buf); // grab size of serialized buffer
    client->text(buf, n); // send buffer to client

    sr::out << "Sent invalid request: " <<
        F(outBuffer["details"].as<const char *>() != nullptr ? outBuffer["details"].as<const char *>() : "null")
    << sr::endl; // print output statement
}
/*
 * Method for sending an invalid attribute specification to the client. The JSON
 * message is as follows:
 * {
 *      dev: "[last device received in inBuffer]",
 *      req: "[last request received in inBuffer],
 *      attr: "[last attribute received in inBuffer]",
 *      stat: "ERROR",
 *      details: "[attr]"
 * }
 */
void WebSocketBridge::sendInvalidAttr(AsyncWebSocketClient *client) {
    outBuffer.clear(); // clear output buffer
    char buf[200]; // fixed array allocation
    outBuffer["dev"] = inBuffer["dev"]; // set device, req, attr fields
    outBuffer["req"] = inBuffer["req"];
    outBuffer["attr"] = inBuffer["attr"];
    outBuffer["stat"] = "ERROR"; // set error status
    outBuffer["details"] = inBuffer["attr"].as<const char *>() != nullptr ? inBuffer["attr"].as<const char *>() : "null";
    const size_t n = serializeJson(outBuffer, buf); // serialize and send
    client->text(buf, n);
    sr::debug << "Sent invalid attribute: " << inBuffer["attr"].as<const char *>() << sr::endl; // notify user
}
/*
 * Method for sending a set response status to the client. The JSON is as follows:
 * {
 *      dev: "[last device - inBuffer]",
 *      req: "SET",
 *      attr: "[last attr - inBuffer]",
 *      val: "[last val - inBuffer]",
 *      stat: "[status code: OK or ERROR]"
 * }
 */
void WebSocketBridge::sendSetResponse(AsyncWebSocketClient *client, const Status &status) {
    outBuffer.clear(); // clear output
    outBuffer["dev"] = inBuffer["dev"]; // set device, req, attr, val, and stat fields
    outBuffer["req"] = "SET";
    outBuffer["attr"] = inBuffer["attr"];
    outBuffer["val"] = inBuffer["val"];
    outBuffer["stat"] = status == OK ? "OK" : "ERROR";
    char buf[200]; // allocate char buffer
    size_t n = serializeJson(outBuffer, buf); // grab size and serialize
    client->text(buf, n); // send to client
    sr::debug << "Sent set response: \n >> " << buf << sr::endl; // notify user
}

/* ------ Method for sending a response to a get request ------
 *  const char* device - Device string
 *  const char* attr - Device's attribute string
 *  const T& val - Attribute's value (any type compatible w/ ArduinoJson)
 */
template <typename T>
void WebSocketBridge::sendGetResponse(const char *device, const char *attr, const T &val) {
    outBuffer.clear(); // clear output
    outBuffer["dev"] = device; // set device, attr, val fields
    outBuffer["attr"] = attr;
    outBuffer["val"] = val;
    char buf[200]; // allocate fixed char array buffer
    const size_t n = serializeJson(outBuffer, buf); // grab size and serialize, sending to client
    sr::debug << "Sent to client: " << buf << sr::endl; // print debug
    ws_.textAll(buf, n);
    sr::debug << "Sent get response: " << val << sr::endl; // print debug
}
/* ------ Callback for servo angle notifier ------
 *  This method sends all the clients connected the angle reading of the servo.
 */
void WebSocketBridge::emitServoAngle(int angle) {
    char buf[200]; // allocate fixed-size char array
    outBuffer.clear();  // clear output buffer
    outBuffer["dev"] = "SERVO"; // device name is SERVO
    outBuffer["attr"] = "POSITION"; // attribute is position reading
    outBuffer["val"] = angle; // value is new reading
    const size_t n = serializeJson(outBuffer, buf); // serialize, grabbing size of doc
    ws_.textAll(buf, n); // notify all clients
}

/* ------ Callback method emitting a sensor reading to the client ------
 * The JSON message is as follows:
 * {
 *      dev: "[SERVO or FLEX_2/FLEX_3/FLEX_4/FLEX_5]",
 *      attr: "READ",
 *      val: [reading as uint16_t]
 * }
 */
void WebSocketBridge::emitSensorReading(uint16_t val, const char *name) {
    char buff[200];
    outBuffer.clear();
    outBuffer["dev"] = name;
    outBuffer["attr"] = "READ";
    outBuffer["val"] = val;
    size_t n = serializeJson(outBuffer, buff);
    sr::out << "Sensor reading: " << val << sr::endl;
    ws_.textAll(buff, n);
}
/* ------ Method for parsing a FlexAttr from the inBuffer ------
 *  This method does c-style string operations on the received
 *  attr field of the inBuffer, retrieving the enumerated
 *  field.
 *
 *  I've mapped enumerations to string operations to enable switch statements,
 *  much faster on an embedded system vs. if-else chains on c-string operations.
 *
 */
WebSocketBridge::FlexAttr WebSocketBridge::parseFlexAttr() {
    if (inBuffer["attr"].isNull()) return FlexAttr::INVALID_FLEX_ATTR; // invalid if nullptr
    const auto retrieved = inBuffer["attr"].as<const char *>(); // cast to c-style string (char array)
    if (strcmp(retrieved, "SAMPLE_RATE") == 0) { // string comparison yields 0 difference
        return FlexAttr::SampleRate; // sampling rate attribute
    }
    if (strcmp(retrieved, "START") == 0) { // string comp. yields 0 diff.
        return FlexAttr::Start; // start attr
    }
    if (strcmp(retrieved, "STOP") == 0) { // 0 diff.
        return FlexAttr::Stop; // stop
    }
    return FlexAttr::INVALID_FLEX_ATTR; // invalid otherwise
}
/* ------ Method for parsing flex sensor attributes ------
 * The only attribute for instances of flex sensors are the pins.
 */
WebSocketBridge::FlexNAttr WebSocketBridge::parseFlexNAttr() {
    if (inBuffer["attr"].isNull()) return FlexNAttr::INVALID_FLEX_N_ATTR; // nullptr == invalid
    if (strcmp(inBuffer["attr"].as<const char *>(), "PIN") == 0) { // 0 difference in received vs PIN str
        return FlexNAttr::Pin; // pin attribute
    }
    return FlexNAttr::INVALID_FLEX_N_ATTR; // invalid otherwise
}
/* ------ Method for parsing device field ------
 * Valid devices:
 *  "SERVO" <-> Device::Servo,
 *  "FLEX_n" <-> Device::Flex_n instance-based attributes
 *  "FLEX" <-> Device::Flex (static attributes—that is, attributes of the class, not actual objects themselves).
 */
WebSocketBridge::Device WebSocketBridge::parseDevice() {
    const char *dev = inBuffer["dev"] | "INVALID";

    if (strcmp(dev, "INVALID") == 0) {
        return Device::INVALID_DEV;
    }
    if (strncmp(dev, "FLEX", 4) == 0) { // null terminator validating length since using strncmp vs strcmp
        if (dev[4] == '\0') { // null terminator found where underscore is, so it's a static flex-sensor attribute
            return Device::Flex;
        }
        switch (dev[5]) {   // switch statement on last char of received inBuffer["dev"] field
            case '2': return Device::Flex_2; // char[5] = 'n' <-> "FLEX_n" where 2 <= n && n <= 5
            case '3': return Device::Flex_3;
            case '4': return Device::Flex_4;
            case '5': return Device::Flex_5;
            default: return Device::INVALID_DEV; // otherwise invalid
        }
    }
    if (strcmp(dev, "SERVO") == 0) { // servo device
        return Device::Servo;
    }
    return Device::INVALID_DEV; // invalid otherwise
}
/* ------ Method to parse the request field of the inBuffer ------
 *
 */
WebSocketBridge::Method WebSocketBridge::parseMethod() {
    const char *method = inBuffer["req"];
    if (method == nullptr) {
        return Method::INVALID_METHOD;
    }
    if (strcmp(method, "GET") == 0) { // get method
        return Method::GET;
    }
    if (strcmp(method, "SET") == 0) { // set method
        return Method::SET;
    }
    return Method::INVALID_METHOD; // invalid otherwise
}
/* ------ Method to parse servo attributes ------
 * This method uses a search-tree pattern, an efficient computational search protocol
 */
WebSocketBridge::ServoAttr WebSocketBridge::parseServoAttr() {
    const char *attr = inBuffer["attr"];
    if (attr == nullptr) return ServoAttr::INVALID_SERVO_ATTR;
    // what letter does the command start with?
    switch (attr[0]) {
        case 'A': { // if A, try ACTUATE or ANGLE_STEP, invalid otherwise
            if (strcmp(attr, "ACTUATE") == 0) return ServoAttr::Actuate;
            if (strcmp(attr, "ANGLE_STEP") == 0) return ServoAttr::AngleStep;
            return ServoAttr::INVALID_SERVO_ATTR;
        }
            // try M
        case 'M': {
            switch (attr[3]) { // what about the fourth character? either an '_' or 'I'
                case '_': {
                    if (strcmp(attr, "MAX_PWM") == 0) {
                        return ServoAttr::MaxPWM;
                    }
                    if (strcmp(attr, "MIN_PWM") == 0) {
                        return ServoAttr::MinPWM;
                    }
                    if (strcmp(attr, "MAX_ANGLE") == 0) {
                        return ServoAttr::MaxAngle;
                    }
                    return ServoAttr::INVALID_SERVO_ATTR;
                }
                default: { // try entire string comparison
                    if (strcmp(attr, "MOTION") == 0) {
                        return ServoAttr::Motion;
                    }
                    return ServoAttr::INVALID_SERVO_ATTR; // invalid otherwise
                }
            }
        }
            //starts w/ 'P'
        case 'P':
            // try position and pin
            if (strcmp(attr, "PIN") == 0) return ServoAttr::Pin;
            if (strcmp(attr, "POSITION") == 0) return ServoAttr::Position;
            return ServoAttr::INVALID_SERVO_ATTR; // invalid otherwise
        // try S
        case 'S':
            if (strcmp(attr, "START_ANGLE") == 0) return ServoAttr::StartAngle;
            if (strcmp(attr, "STOP_ANGLE") == 0) return ServoAttr::StopAngle;
            return ServoAttr::INVALID_SERVO_ATTR; // invalid
        case 'T':
            if (strcmp(attr, "TIME_DELAY") == 0) return ServoAttr::TimeDelayUS;
            return ServoAttr::INVALID_SERVO_ATTR;
        default:
            return ServoAttr::INVALID_SERVO_ATTR; // invalid otherwise
    }
}

/*
 * Callback for websocket events. unused server and arg parameters.
 */
void WebSocketBridge::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT: {
            handleConnect(client);
        } break;
        case WS_EVT_DISCONNECT: {
            if (ws_.count() == 0) {
                sr::out << "Client disconnected." << sr::endl;
                for (auto &sensor : sensors) {
                    sensor.setActive(false);
                }
                servo_.disableMotion();
            }
        } break;
        case WS_EVT_DATA: {
            std::string msg(reinterpret_cast<const char *>(data), len);
            sr::out << msg.c_str() << sr::endl;
            received.push(msg);
        } break;
        case WS_EVT_PONG:
            ws_.pingAll();
        default: break;
    }
}
// initial config of servo/flex sensors
void WebSocketBridge::handleConnect(AsyncWebSocketClient *client) {
    sr::out << "Client 1/1 connected. Sending current information." << sr::endl;
    sendGetResponse( "SERVO", "ANGLE_STEP", servo_.getAngleStep());
    sendGetResponse( "SERVO", "MAX_PWM", servo_.getPwmMax());
    sendGetResponse( "SERVO", "MAX_ANGLE", servo_.getMaxAngle());
    sendGetResponse( "SERVO", "MIN_PWM", servo_.getPwmMin());
    sendGetResponse("SERVO", "MOTION", ServoController::motionString(servo_.getMotion()));
    sendGetResponse("SERVO", "PIN", servo_.getPin());
    sendGetResponse("SERVO", "POSITION", servo_.getPosition());
    sendGetResponse( "SERVO", "START_ANGLE", servo_.getStartAngle());
    sendGetResponse( "SERVO", "STOP_ANGLE", servo_.getStopAngle());
    sendGetResponse( "SERVO", "TIME_DELAY", servo_.getTimeDelay());
    sendGetResponse( "FLEX", "SAMPLE_RATE", FlexSensor::getSamplingInterval());
    sendGetResponse("FLEX_2", "PIN", sensors[0].getPin().value_or(false));
    sendGetResponse("FLEX_3", "PIN", sensors[1].getPin().value_or(false));
    sendGetResponse("FLEX_4", "PIN", sensors[2].getPin().value_or(false));
    sendGetResponse("FLEX_5", "PIN", sensors[3].getPin().value_or(false));

}
// set related fields of the inBuffer JSON from received fields
void WebSocketBridge::handleReceived(const char *request) {
    inBuffer.clear();
    DeserializationError error = deserializeJson(inBuffer, request);
    if (error) {
        sr::out << "Failed to parse request: " << error.c_str() << sr::endl;
        return;
    }
    auto dev = parseDevice(); // get device, request
    auto req = parseMethod();
    if (dev != Device::INVALID_DEV && req != Method::INVALID_METHOD) { // continue if not invalid
        switch (dev) { // switch between devices
            case Device::Servo: { // attempt servo attribute get/set
                const auto attr = parseServoAttr();
                if (req == Method::SET && !inBuffer["val"].isNull()) { // must be a setter
                    switch (attr) {
                        case ServoAttr::AngleStep:
                            servo_.setAngleStep(inBuffer["val"].as<int>());
                            break;
                        case ServoAttr::TimeDelayUS:
                            servo_.setTimeDelay(inBuffer["val"].as<int>());
                            break;
                        case ServoAttr::MinPWM:
                            servo_.setMaxPWM(inBuffer["val"].as<int>());
                            break;
                        case ServoAttr::MaxPWM:
                            servo_.setMinPWM(inBuffer["val"].as<int>());
                            break;
                        case ServoAttr::Position:
                            servo_.setPosition(inBuffer["val"].as<int>());
                            break;
                        case ServoAttr::Pin:
                            servo_.setPin(inBuffer["val"].as<uint8_t>());
                            break;
                        case ServoAttr::Actuate: {
                            if (const auto enabled = inBuffer["val"].as<bool>(); enabled == true) {
                                servo_.enableMotion();
                            } else {
                                servo_.disableMotion();
                            }
                        } break;
                        case ServoAttr::StartAngle:
                            servo_.setStartAngle(inBuffer["val"].as<int>());
                            break;
                        case ServoAttr::StopAngle:
                            servo_.setStopAngle(inBuffer["val"].as<int>());
                            break;
                        case ServoAttr::Motion:
                            servo_.setMotion(ServoController::fromString( inBuffer["val"].as<const char *>()));
                            break;
                        case ServoAttr::MaxAngle:
                            servo_.setMaxAngle(inBuffer["val"].as<unsigned int>());
                            break;
                        default:
                            sr::out << "Invalid servo attribute: " << inBuffer["val"].as<const char *>() << sr::endl;
                            sendInvalidAttr(&ws_.getClients().front());
                            break;
                    }
                }
                else if (req == Method::GET) { // try a getter
                    // redundant parsing, just send the associated attribute from the attribute.
                    switch (attr) {
                        case ServoAttr::AngleStep:
                            sendGetResponse("SERVO", "ANGLE_STEP", servo_.getAngleStep());
                            break;
                        case ServoAttr::TimeDelayUS:
                            sendGetResponse("SERVO", "TIME_DELAY", servo_.getTimeDelay());
                            break;
                        case ServoAttr::MinPWM:
                            sendGetResponse("SERVO", "MIN_PWM", servo_.getPwmMin());
                            break;
                        case ServoAttr::MaxPWM:
                            sendGetResponse( "SERVO", "MAX_PWM", servo_.getPwmMax());
                            break;
                        case ServoAttr::Position:
                            sendGetResponse("SERVO", "POSITION", servo_.getPosition());
                            break;
                        case ServoAttr::Pin:
                            sendGetResponse("SERVO", "PIN", servo_.getPin());
                            break;
                        case ServoAttr::Actuate:
                            sendGetResponse( "SERVO", "ACTUATE", servo_.isActive());
                            break;
                        case ServoAttr::StartAngle:
                            sendGetResponse( "SERVO", "START_ANGLE", servo_.getStartAngle());
                            break;
                        case ServoAttr::StopAngle:
                            sendGetResponse( "SERVO", "STOP_ANGLE", servo_.getStopAngle());
                            break;
                        case ServoAttr::Motion:
                            sendGetResponse( "SERVO", "MOTION", ServoController::motionString(servo_.getMotion()));
                            break;
                        case ServoAttr::MaxAngle:
                            sendGetResponse("SERVO", "MAX_ANGLE", servo_.getMaxAngle());
                            break;
                        default:
                            sendInvalidAttr(&ws_.getClients().front());
                    }
                }
                else { // invalid method otherwise
                    sendInvalidRequest(&ws_.getClients().front());
                }
            } break; // end Device::Servo case
            case Device::Flex: {
                if (auto attr = parseFlexAttr(); attr != FlexAttr::INVALID_FLEX_ATTR) {
                    if (attr == FlexAttr::SampleRate) {
                        if (req == Method::SET) {
                            if (inBuffer["val"].isNull()) {
                                sendInvalidAttr(&ws_.getClients().front());
                            } else {
                                bool wasActive = sensors[0].getActive();
                                for (auto &sensor : sensors) {
                                    sensor.setActive(false);
                                }
                                FlexSensor::setSamplingInterval(inBuffer["val"].as<unsigned int>());
                                if (wasActive) {
                                    for (auto &sensor : sensors) {
                                        sensor.setActive(true);
                                    }
                                }
                            }
                        } else {
                            sendGetResponse("FLEX", "SAMPLE_RATE", FlexSensor::getSamplingInterval());
                        }
                    } else if (attr == FlexAttr::Start) {
                        for (auto &sensor : sensors) {
                            sensor.setActive(true);
                        }
                        sendSetResponse(&ws_.getClients().front(), OK);
                    } else {
                        for (auto &sensor : sensors) {
                            sensor.setActive(false);
                        }
                        sendSetResponse(&ws_.getClients().front(), OK);
                    }
                } else {
                    sendInvalidAttr(&ws_.getClients().front());
                }
            } break; // end Device::Flex case (static)
            default: {
                // search through all sensors, attempting to compare in buffer's device field to the sensors name.
                bool found = false;
                auto attr = parseFlexNAttr();
                int i = 0; // store index where found sensor occurred
                for (auto &sensor : sensors) {
                    if (strcmp(sensor.getName(), inBuffer["dev"].as<const char *>()) == 0) {
                        found = true;
                        break;
                    }
                    i++;
                }
                if (found) {
                    if (attr == FlexNAttr::Pin) { // pin attr
                        if (req == Method::GET) { // request to get the pin number
                            sendGetResponse(sensors[i].getName(), "PIN", sensors[i].getPin().value_or(false));
                        } else {
                            // attempt reinterpreting false pin value to std::nullopt
                            auto v = inBuffer["val"];
                            // 1) bool `false` -> detach
                            if (v.is<bool>() && v.as<bool>() == false) {
                                sensors[i].setPin(std::nullopt);
                                sendSetResponse(&ws_.getClients().front(), OK);
                            }
                            // 2) numeric -> pin number
                            else if (v.is<long>()) {
                                const auto pinNum = v.as<long>();
                                if (sensors[i].setPin(static_cast<uint16_t>(pinNum))) {
                                    sendSetResponse(&ws_.getClients().front(), OK);
                                } else {
                                    sendSetResponse(&ws_.getClients().front(), ERROR);
                                }
                            }
                            // 3) string “false” -> detach
                            else if (v.is<const char*>()) {
                                const auto s = v.as<const char*>();
                                if (strcmp(s, "false") == 0) {
                                    sensors[i].setPin(std::nullopt);
                                    sendSetResponse(&ws_.getClients().front(), OK);
                                }
                                else {
                                    sendInvalidAttr(&ws_.getClients().front());
                                }
                            }
                            else {
                                sendInvalidAttr(&ws_.getClients().front());
                            }
                        }
                    }
                } else {
                    sendSetResponse(&ws_.getClients().front(), ERROR);
                }
            } break;
        }
    } else {
        sendInvalidRequest(&ws_.getClients().front());
    }
}

