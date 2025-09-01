#include "ServoController.h"


uint8_t ServoController::channelCount = 0;
portMUX_TYPE ServoController::mux = portMUX_INITIALIZER_UNLOCKED;
void ServoController::timerCB(void *arg) {
    const auto instance = static_cast<ServoController*>(arg);
    portENTER_CRITICAL(&mux);
    instance->tick_ = true;
    portEXIT_CRITICAL(&mux);
}
void ServoController::fallbackTimerCB(void *arg) {
    if (const auto self = static_cast<ServoController*>(arg); !esp_timer_is_active(self->timer_)) {
        if (self->motion_ != INVALID && self->motion_ != ONE_SHOT) {
            if (const esp_err_t err = esp_timer_start_periodic(self->timer_, self->delayUs_); err != ESP_OK) {
                sr::out << "Failed to restart servo timer: " << esp_err_to_name(err) << sr::endl;
            }
        }
    }
}
ServoController::ServoController(uint8_t pin, unsigned int maxAngle) :
    pin_(pin),
    maxAngle_(maxAngle),
    timer_(nullptr),
    fallbackTimer_(nullptr),
    pos_(0),
    delayUs_(100000),
    pwmMin_(500),
    pwmMax_(2500),
    startAngle_(0),
    stopAngle_(270),
    angleStep_(1),
    fallbackTimerArgs_({
        .callback = fallbackTimerCB,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "fallback",
        .skip_unhandled_events = false
    }),
    timerArgs_({
        .callback = timerCB,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "servo",
        .skip_unhandled_events = false
    }),
    tick_(false), angleNotify_(nullptr),
    motion_(LOOP),
    fallbackDelay(3000000)
{
    channelCount++;
}


ServoController::~ServoController() {
    esp_timer_stop(timer_);
    esp_timer_stop(fallbackTimer_);
    esp_timer_delete(timer_);
    esp_timer_delete(fallbackTimer_);
}
void ServoController::setup() {
    tick_ = false;
    const auto error = esp_timer_create(&timerArgs_, &timer_);
    if (error != ESP_OK) {
        sr::out << "Failed to create timer: " << error << sr::endl << "Throwing std::runtime_error." << sr::endl;
        throw std::runtime_error("Failed to create timer");
    }
    const auto error2 = esp_timer_create(&fallbackTimerArgs_, &fallbackTimer_);
    if (error2 != ESP_OK) {
        sr::out << "Failed to create fallback timer: " << error2 << sr::endl << "Throwing std::runtime_error." << sr::endl;
        throw std::runtime_error("Failed to create fallback timer");
    }
}
void ServoController::loop() {
    portENTER_CRITICAL(&mux);
    bool run = tick_;
    tick_ = false;
    portEXIT_CRITICAL(&mux);
    if (!run) return;
    if (angleStep_ == 0) {
        sr::debug << F("Angle-step was set to 0. Setting to 1 and disabling motion.");
        angleStep_ = 1;
        disableMotion();
        return;
    }
    switch (motion_) {
        case LOOP: {
                    /*
                     *  to move ccw:
                     *      start < stop AND angleStep_ > 0
                     *      stop < start AND angleStep_ < 0
                     *  to move cw:
                     *      start < stop AND angleStep_ < 0
                     *      stop < start AND angleStep_ > 0
                     */
            // Determine the intended direction by comparing start/stop angles
            if (startAngle_ < stopAngle_) {
                // Intended direction is ccw. Make sure the angle-step is positive.
                if (angleStep_ < 0) {
                    // Print a debugging message, if not, and disable motion.
                    sr::debug << F("The starting angle (") << startAngle_ << F(") is < the stopping angle (")
                    << stopAngle_ << F("), but the angle-step is < 0.") << sr::endl;
                    sr::debug << F("To move CCW: \n\t start < stop AND angle-step > 0 OR \n\t stop < start AND angle-step > 0") << sr::endl;
                    sr::debug << F("To move CW: \n\t start < stop AND angle-step < 0 OR \n\t stop < start AND angle-step > 0") << sr::endl;
                    sr::debug << F("Disabling servo...") << sr::endl;
                    disableMotion();
                } else {
                    // Check if sum is > stopAngle,
                    if (const int newPos = pos_ + angleStep_; newPos > stopAngle_) {
                        // If so, start the fallback timer to let the servo go back to startAngle.
                        pos_ = startAngle_;
                        disableMotion();
                        sr::debug << F("Starting fallback timer: \n\t\tcurrent pos = ") << pos_ << F(", \n\tpos after increment = ") << newPos
                        << F(", \n\t\t\t stopAngle_ = ") << stopAngle_ << sr::endl;
                        esp_timer_start_once(fallbackTimer_, fallbackDelay);
                    } else {
                        pos_ = newPos;
                    }
                }
            } else {
                // Intended direction is CW. Make sure angle-step is -.
                if (angleStep_ > 0) {
                    // Print a debugging message if positive
                    sr::debug << F("The starting angle (") << startAngle_ << F(") is > the stopping angle (")
                    << stopAngle_ << F("), but the angle-step is > 0.") << sr::endl;
                    sr::debug << F("To move CW: \n\t start < stop AND angle-step < 0 OR \n\t stop < start AND angle-step > 0") << sr::endl;
                    sr::debug << F("To move CCW: \n\t start < stop AND angle-step > 0 OR \n\t stop < start AND angle-step > 0") << sr::endl;
                    sr::debug << F("Disabling servo...") << sr::endl;
                    disableMotion();
                } else {
                    // Check if sum is < startAngle
                    if (const int newPos = pos_ + angleStep_; newPos < startAngle_) {
                        // If so, start the fallback timer to let the servo go back to startAngle.
                        pos_ = stopAngle_;
                        disableMotion();
                        sr::debug << F("Starting fallback timer: \n\t\tcurrent pos = ") << pos_ << F(", \n\tpos after decrement = ") << newPos
                        << F(", \n\t\t\t startAngle_ = ") << startAngle_ << sr::endl;
                        esp_timer_start_once(fallbackTimer_, fallbackDelay);
                    } else {
                        pos_ = newPos;
                    }
                }
            }
        } break;
        case SWEEP: {
            // Local variable for the new position as the sum of the current position and the angle-step.
            const int newPos = pos_ + angleStep_;
            // Again as before, check intended direction, except with the angle-step coefficient.
            if (angleStep_ < 0) {
                // Moving CW. Check if new position < start angle.
                if (newPos < startAngle_) {
                    // Angle-step is the opposite of current (*= –1).
                    angleStep_ *= -1;
                    // Set the current position as starting angle.
                    pos_ = startAngle_;
                    // Notify user of change
                    sr::debug << F("Switching direction from CW -> CCW") << sr::endl;
                } else {
                    // Otherwise, keep decrementing.
                    pos_ = newPos;
                }
            } else {
                // Moving CCW. Check if new position > stopAngle_.
                if (newPos > stopAngle_) {
                    // Angle-step *= -1
                    angleStep_ *= -1;
                    // Set current position as stopping angle
                    pos_ = stopAngle_;
                    // Notify user of change
                    sr::debug << F("Switching direction from CCW -> CW") << sr::endl;
                } else {
                    // Otherwise, keep incrementing.
                    pos_ = newPos;
                }
            }
        } break;
        case ONE_SHOT: {
            /* For this one, we'll replicate functionality to LOOP except, instead of initiating
             * fallback timer, we'll stop the servo.
             */
            // Determine the intended direction by comparing start/stop angles
            if (startAngle_ < stopAngle_) {
                // Intended direction is ccw. Make sure the angle-step is positive.
                if (angleStep_ < 0) {
                    // Print a debugging message, if not, and disable motion.
                    sr::debug << F("The starting angle (") << startAngle_ << F(") is < the stopping angle (")
                    << stopAngle_ << F("), but the angle-step is < 0.") << sr::endl;
                    sr::debug << F("To move CCW: \n\t start < stop AND angle-step > 0 OR \n\t stop < start AND angle-step > 0") << sr::endl;
                    sr::debug << F("To move CW: \n\t start < stop AND angle-step < 0 OR \n\t stop < start AND angle-step > 0") << sr::endl;
                    sr::debug << F("Disabling servo...") << sr::endl;
                    disableMotion();
                } else {
                    // Check if sum is > stopAngle,
                    if (const int newPos = pos_ + angleStep_; newPos > stopAngle_) {
                        // If so, start the fallback timer to let the servo go back to startAngle.
                        pos_ = startAngle_;
                        disableMotion();
                        sr::debug << "Stopping timer. Motion finished. \n\tCurrent position: " << pos_ << sr::endl
                        << "\tStop-angle: " << stopAngle_ << sr::endl;
                    } else {
                        pos_ = newPos;
                    }
                }
            } else {
                // Intended direction is CW. Make sure angle-step is -.
                if (angleStep_ > 0) {
                    // Print a debugging message if positive
                    sr::debug << F("The starting angle (") << startAngle_ << F(") is > the stopping angle (")
                    << stopAngle_ << F("), but the angle-step is > 0.") << sr::endl;
                    sr::debug << F("To move CW: \n\t start < stop AND angle-step < 0 OR \n\t stop < start AND angle-step > 0") << sr::endl;
                    sr::debug << F("To move CCW: \n\t start < stop AND angle-step > 0 OR \n\t stop < start AND angle-step > 0") << sr::endl;
                    sr::debug << F("Disabling servo...") << sr::endl;
                    disableMotion();
                } else {
                    // Check if sum is < startAngle
                    if (const int newPos = pos_ + angleStep_; newPos < startAngle_) {
                        // If so, start the fallback timer to let the servo go back to startAngle.
                        pos_ = stopAngle_;
                        disableMotion();
                        sr::debug << "Reached stopAngle. Current position: " << pos_ << sr::endl
                        << "\tStop-angle: " << stopAngle_ << sr::endl;
                    } else {
                        pos_ = newPos;
                    }
                }
            }
        } break;
        default: disableMotion(); break;
    }
    updateDuty();
    if (angleNotify_) angleNotify_(pos_);
}
void ServoController::setMaxPWM(const unsigned long m) {
    if (m <= pwmMin_) {
        sr::out << "new max PWM value cannot be <= existing PWM value." << sr::endl;
        return;
    }
    const bool wasRunning = esp_timer_is_active(timer_);
    if (wasRunning) disableMotion();
    pwmMax_ = m;
    sr::out << "new max PWM value: " << pwmMax_ << sr::endl;
    if (wasRunning) disableMotion();
}
void ServoController::setMinPWM(const unsigned long m) {
    if (m >= pwmMax_) {
        sr::out << "new min PWM value cannot be >= existing PWM value." << sr::endl;
        return;
    }
    const bool wasRunning = esp_timer_is_active(timer_);
    if (wasRunning) disableMotion();
    pwmMin_ = m;
    sr::out << "new min PWM value: " << pwmMin_ << sr::endl;
    if (wasRunning) disableMotion();
}

void ServoController::setAngleStep(const int angleStep) {
    if (abs(angleStep) > maxAngle_) {
        sr::out << "angle-step size cannot exceed the maximum range of servo." << sr::endl;
        return;
    }
    const bool wasRunning = esp_timer_is_active(timer_);
    if (wasRunning) disableMotion();
    angleStep_ = angleStep;
    sr::out << "new angle-step: " << angleStep_ << sr::endl;
    if (wasRunning) enableMotion();
}

void ServoController::setPosition(const int pos) {
    const bool running = esp_timer_is_active(timer_);
    if (running) disableMotion();
    if (pos > maxAngle_) {
        sr::out << "New position '" << pos << "' exceeds maximum range '" << maxAngle_ << "'. Setting to max angle." << sr::endl;
        pos_ = maxAngle_;
    } else if (pos < 0) {
        sr::out << F("New position cannot be < 0. Setting to 0.") << sr::endl;
        pos_ = 0;
    } else {
        sr::debug << "New position: " << pos << sr::endl;
        pos_ = pos;
    }
    updateDuty();
    if (running) enableMotion();
    if (angleNotify_) angleNotify_(pos_);
}
void ServoController::setMotion(const Motion motion) {
    if (motion == INVALID) {
        sr::out << "Disabling servo..." << sr::endl;
        disableMotion();
    } else {
        const bool wasRunning = esp_timer_is_active(timer_);
        if (wasRunning) disableMotion();
        motion_ = motion;
        sr::out << "New motion: " << motion_ << sr::endl;
        if (wasRunning) enableMotion();
    }
}
void ServoController::setTimeDelay(const unsigned long delayUs) {
    if (delayUs < pwmMin_) {
        sr::out << "new time delay (" << delayUs << "cannot be < minimum PWM value." << sr::endl;
        return;
    }
    const bool wasRunning = esp_timer_is_active(timer_);
    if (wasRunning) disableMotion();
    delayUs_ = delayUs;
    sr::out << "new time delay: " << delayUs_ << sr::endl;
    if (wasRunning) enableMotion();
}
void ServoController::setStartAngle(const int startAngle) {
    if (startAngle < 0) {
        sr::out << "new start angle cannot be < 0." << sr::endl;
        return;
    }
    if (startAngle > maxAngle_) {
        sr::out << "new start angle cannot be > maximum range." << sr::endl;
        return;
    }
    const bool wasRunning = esp_timer_is_active(timer_);
    if (wasRunning) disableMotion();
    startAngle_ = startAngle;
    sr::out << "new start angle: " << startAngle_ << sr::endl;
    if (wasRunning) enableMotion();
}
void ServoController::setStopAngle(const int stopAngle) {
    if (stopAngle < 0) {
        sr::out << "new stop angle cannot be < 0." << sr::endl;
        return;
    }
    if (stopAngle > maxAngle_) {
        sr::out << "new stop angle cannot be > maximum range." << sr::endl;
        return;
    }
    const bool wasRunning = esp_timer_is_active(timer_);
    if (wasRunning) disableMotion();
    stopAngle_ = stopAngle;
    sr::out << "new stop angle: " << stopAngle_ << sr::endl;
    if (wasRunning) enableMotion();
}
void ServoController::updateDuty() const {
    const auto pulse = map(static_cast<long>(pos_), 0,
        static_cast<long>(maxAngle_),
        static_cast<long>(pwmMin_),
        static_cast<long>(pwmMax_));
    constexpr uint32_t PERIOD_US = 20000;
    constexpr uint32_t MAX_TICKS = (1u << 10) - 1;
    const uint32_t duty = (pulse * MAX_TICKS) / PERIOD_US;
    analogWrite(pin_, duty);
}
void ServoController::setPin(uint8_t pin) {
    bool wasRunning = esp_timer_is_active(timer_);
    if (wasRunning) disableMotion();
    pin_ = pin;
    ledcAttachPin(pin_, channelCount);
    sr::out << "new pin: " << pin_ << sr::endl;
    if (wasRunning) enableMotion();
}
void ServoController::enableMotion() {
    if (esp_timer_is_active(timer_)) return;
    const auto error = esp_timer_start_periodic(timer_, delayUs_);
    if (error != ESP_OK) {
        sr::out << "Failed to start servo timer." << sr::endl;
        return;
    }
    if (motion_ == INVALID) motion_ = LOOP;
    sr::out << "Servo enabled." << sr::endl;
}

/**
 * Method setting the maximum angle for servo—used in PWM signal calculation.
 * @param a is the maximum angle to set
 * @link esp_timer_create_args_t
 */
void ServoController::setMaxAngle(unsigned int a) {
    bool wasRunning = esp_timer_is_active(timer_);
    if (wasRunning) disableMotion();
    maxAngle_ = a;
    sr::out << "new max angle: " << maxAngle_ << sr::endl;
    if (wasRunning) enableMotion();
}


void ServoController::disableMotion() {
    if (!esp_timer_is_active(timer_)) return;
    const auto error = esp_timer_stop(timer_);
    if (error != ESP_OK) {
        sr::out << "Failed to stop servo timer." << sr::endl;
        return;
    }
    sr::out << "Servo disabled." << sr::endl;
}



