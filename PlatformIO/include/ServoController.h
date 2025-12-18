/*----------------------------------------------------------------------------------------------------------------------
 *  BME:4920 - Biomedical Engineering Senior Design II
 *  Team 13 | Remote Hand Exoskeleton
 *  Sullivan Bryant, Charley Dunham, Jared Gilliam
 *
 *
 *  This header file outlines the ServoController class, providing flexibility to accommodate various servo motors.
 *
 *----------------------------------------------------------------------------------------------------------------------*/

#pragma once
#include <Arduino.h>
#include <functional>
#include <esp_timer.h>
#include <esp_event.h>
#include "SerialStream.h"
/*
 * Class for controlling a servo motor with a PWM signal. This class provides flexibility, allowing the user to
 * change the PWM signal as it may differ among various servo motors. Standard servos typically provide a 180º range.
 * However, this class defaults to servos using a 270º range @ 500 – 2500 µs as these were the ones used in our
 * project.
 *
 */
class ServoController {
public:
    // =======================================================================================
    //                                  Public custom types
    /* ------ Different motion states of servo motor ------
     * Loop:
     * Sweep: w
     */
    enum Motion {
        LOOP,                                                   // startAngle  -> stopAngle controlled speed
        SWEEP,                                                  // startAngle <-> stopAngle controlled speed
        ONE_SHOT,                                               // startAngle  -> stopAngle controlled, only one iteration
        INVALID                                                 // invalid motion specifier
    };
    // ------ Static method to convert motion type to string ------
    static const char *motionString(const Motion motion) {
        switch (motion) {
            case LOOP: return "LOOP";
            case SWEEP: return "SWEEP";
            case ONE_SHOT: return "ONE_SHOT";
            default: return "INVALID";
        }
    }
    // ------ Static method to convert a string to a motion type ------
    static Motion fromString(const char *motion) {
        if (strcmp(motion, "LOOP") == 0) return LOOP;
        if (strcmp(motion, "SWEEP") == 0) return SWEEP;
        if (strcmp(motion, "ONE_SHOT") == 0) return ONE_SHOT;
        return INVALID;
    }
    // ------ Explicit constructor ------
    explicit ServoController(
        uint8_t pin = D4,                                       // Default pin @ D4
        unsigned int maxAngle = 270);                           // Default maximum angle @ 270º
    // ------ Destructor ------
    ~ServoController();
    /* ------ Setup method ------
     * This method creates the timers for the actuation and fallback periods
     */
    void setup();
    /* ------ Loop method ------
     * This method checks the volatile flag set by the actuation callback, determines whether
     * to run, and how to increment/decrement the position.
     *
     * The portion checking the flag is guarded by a critical section macro,
     * protecting the flag as the callback for the timer is an interruptible event.
     * Then, it checks the motion-mode, and continues actuating depending on the motion mode.
     * >> Loop: the intended direction is determined from the angle-step.
     *      - If decrementing (CW), the sum of the current position and angle step is compared to the
     *        stopAngle_. If the sum falls below the stopping angle, the motion disable is triggered
     *        as well as the fallback timer, otherwise continuing decrement of the position.
     *      - If incrementing (CCW), the sum is compared to the
     * >> Sweep:
     *
     */
    void loop();

    void setPin(uint8_t pin);
    uint8_t getPin() const { return pin_; }


    void setMaxAngle(unsigned int maxAngle);
    int getMaxAngle() const { return maxAngle_; }

    void setMotion(Motion motion);
    Motion getMotion() const { return motion_; }

    void setTimeDelay(unsigned long delayUs);
    unsigned long getTimeDelay() const { return delayUs_; }

    void setMinPWM(unsigned long pwmMin);
    unsigned long getPwmMin() const { return pwmMin_; }

    void setMaxPWM(unsigned long pwmMax);
    unsigned long getPwmMax() const { return pwmMax_; }

    void setStartAngle(int startAngle);
    int getStartAngle() const { return startAngle_; }

    void setStopAngle(int stopAngle);
    int getStopAngle() const { return stopAngle_; }

    void setAngleStep(int angleStep);
    int getAngleStep() const { return angleStep_; }

    void setPosition(int pos);
    int getPosition() const { return pos_; }

    bool isActive() const { return esp_timer_is_active(timer_); }


    void enableMotion();
    void disableMotion();
    using callback = std::function<void(int angle)>;

    void addAngleNotify(std::function<void(int)> cb) { angleNotify_ = std::move(cb); }
private:
    std::function<void(int angle)> angleNotify_;
    void updateDuty() const;
    volatile bool tick_;
    uint8_t pin_;
    int maxAngle_;
    Motion motion_;
    int pos_;
    unsigned long delayUs_;
    unsigned long pwmMin_;
    unsigned long pwmMax_;
    int startAngle_;
    int stopAngle_;
    int angleStep_;
    esp_timer_handle_t timer_;
    esp_timer_create_args_t timerArgs_;
    esp_timer_handle_t fallbackTimer_;
    esp_timer_create_args_t fallbackTimerArgs_;
    static void timerCB(void *arg);
    static void fallbackTimerCB(void *arg);
    static uint8_t channelCount;
    static portMUX_TYPE mux;
    uint64_t fallbackDelay;
};

