/*----------------------------------------------------------------------------------------------------------------------
 *  BME:4920 - Biomedical Engineering Senior Design II
 *  Team 13 | Remote Hand Exoskeleton
 *  Sullivan Bryant, Charley Dunham, Jared Gilliam
 *
 *  Written by Sullivan Bryant
 *
 *  This class encapsulates functionality associated with flex sensors, providing flexibility to change various
 *  aspects, i.e.,
 *      the non-blocking sampling rate acquired via esp_timers (interruptible events simply setting a flag guarded by a mutex),
 *          >> The default sampling rate set to 100,000 µs (10 Hz), plenty fast, designed with a lowpass filter
 *             attenuating frequencies @ 1.59 Hz (safely under Nyquist @ 5 Hz). This provides the assumption that the
 *             patient won't be performing fast-paced flexion.
 *      the pin the sensor's connected to (any ADC pin—however, be aware you cannot use pins A4 – A7 w/ Wi-Fi).
 *----------------------------------------------------------------------------------------------------------------------*/

#pragma once
#include <Arduino.h>
#include <optional>
#include <functional>
#include "SerialStream.h"
#include <esp_timer.h>
class FlexSensor {                          //  Class for managing flex sensor devices
public:
    //------------- Custom types
    enum Finger {                                                   //  Enumerated finger types
        Index = 2,                                                  //  Index finger
        Middle = 3,                                                 //  Middle finger
        Ring = 4,                                                   //  Ring finger
        Pinky = 5                                                   //  Pinky finger
    };
    //------------- Constructor
    explicit FlexSensor(                                            //  Explicit constructor with a required <const char*> name argument
        const char *name,                                               //  (required) name of flex sensor
        std::optional<uint8_t> pin = std::nullopt,                      //  Optional pin field (std::nullopt/uint8_t)
        std::function                                                   //  Callback function with signature:
                <void(uint16_t,                                             //  No return, 16 unsigned value for sensor reading
                const char*)>                                               //  Name of flex sensor
        notifier = nullptr,                                             //  Default to nullptr
        Finger finger = Index);                                         //  Default finger is index finger
    //------------- Destructor
    ~FlexSensor();                                                  //  Explicit destructor
    //------------- Arduino methods
    void setup();                                                   //  Setup method called once in setup() block
    void loop();                                                    //  Loop method called once per iteration in loop() block
    //------------- Static methods
    static void setSamplingInterval(                                //  Set the sampling interval
    uint64_t interval);                                                 //  Positive new value of sampling rate
    static uint64_t getSamplingInterval()                           //  Get the sampling interval
        { return samplingInterval_; }                                   //  Returns uint64_t representing sampling interval
    //------------- Instance methods
    bool setPin(                                                    //  Method to set the pin of the flex sensor.
        std::optional<uint16_t> pin);                                   //  Optional argument to signify not connected status.

    [[nodiscard]] std::optional<uint8_t>getPin() const              //  Method to get pin (can be std::nullopt or uint8_t)
        { return pin_; }                                                //  Return private field pin_

    void setActive(                                                 //  Method to enable/disable sampling
        bool enable = true);                                        //  Flag representing sampling status
    [[nodiscard]] bool setupFailed() const                          //  Method to get whether setup failed
        { return failed; }                                              //  Returns a bool indicating failure status
    [[nodiscard]] bool getActive() const                            //  Method to get whether the sensor is being sampled
        { return esp_timer_is_active(                                   //  Returns a bool indicating if instance is collecting samples
            samplingTimer); }
    [[nodiscard]] uint16_t getLastReading() const                   //  Method to obtain last reading of the flex sensor
        { return reading; }                                             //  Returns an uint16_t value of the last reading
    void setFinger(                                                 //  Method to set the sensor's finger
        Finger finger);                                                 //  The new Finger to set to
    [[nodiscard]] Finger getFinger() const                          //  Method to get the sensor's finger
        { return finger; }                                              //  Returns an enumerated Finger representing sensor's finger
    void setName(                                                   //  Method to set the name of the sensor. This is used as the .name attribute for esp_timers
        const char *name);                                              //  New name to set timer to
    [[nodiscard]] const char *getName() const                       //  Method to get the timer's name.
        {return name;}                                                  //  Returns a const char* array representing name
    // --- WebSocketBridge notifier
    void setNotifier(                                               //  Method to pass a callback notifier to for collected samples
        std::function<                                                  //  Callback has no-return, two argument signature,
            void(uint16_t,                                                  //  with the first being the placeholder for the reading value,
                const char* )>                                              //  the second being the name of the timer.
            notifier);
private:
    //------------- Private static methods
    static void onTimer(                                            //  Callback for sampling timer.
        void *arg);                                                     //  Generic pointer cast back to 'this' (pointer to instance)
    //------------- Private static fields
    static portMUX_TYPE mux;                                        //  Mutex guarding flag determining whether a new reading must be sampled in the loop.
    static uint64_t samplingInterval_;                              //  Sampling interval—assumed to be a property of the class itself rather than an instance (all should
                                                                    //      have the same sampling interval).
    static unsigned int sensorCount;                                //  Static counter incremented each call to the constructor
    //------------- Private instance fields
    std::function<void(                                             //  Placeholder for sampling callback
        uint16_t,                                                       //  Has same signature as setter: placeholder for reading,
        const char *)>                                                  //  placeholder for the timer's name.
    notifier_;
    std::optional<                                                  //  Optional field for the pin.
        uint8_t>                                                        //  When stored, it is an uint8_t value.
    pin_;
    esp_timer_handle_t samplingTimer;                               //  Sampling esp_timer handle
    esp_timer_create_args_t samplingArgs;                           //  Sampling timer's arguments
    bool failed;                                                    //  Flag indicating failure status
    volatile bool ready_;                                           //  Volatile flag (guarded by mutex) indicating whether a new reading should be collected
    bool wasRunning;                                                //  Flag indicating last running state (lots of local declarations, so it was scoped within the instance)
    uint16_t reading;                                               //  Placeholder for the last reading
    Finger finger;                                                  //  Placeholder for timer's finger
    const char *name;                                               //  Name of the timer
};

