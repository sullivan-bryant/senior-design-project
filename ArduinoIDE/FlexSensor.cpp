#include "FlexSensor.h"

/* Static initializers */
portMUX_TYPE FlexSensor::mux = portMUX_INITIALIZER_UNLOCKED;    // mutex guard
unsigned int FlexSensor::sensorCount = 0;                       // static counter incremented via constructor
uint64_t FlexSensor::samplingInterval_ = 100000;                // sampling rate attribute of class—not particular sensors
/* Constructor for a flex sensor. Must provide a name for esp_timer. */
FlexSensor::FlexSensor(
    const char *name,                                           //  Name of the sensor (FLEX_2/FLEX_3/FLEX_4/FLEX_5)
    std::optional<uint8_t> pin,                                 //  Pin sensor's connected to
    std::function<void(uint16_t, const char*)> notifier,        //  Callback function for notifying samples
    Finger finger_) :                                           //  Finger representation of sensor
    name(name),                                                 //  Set the input name to the sensor's
    pin_(pin), notifier_(std::move(notifier)),                  //  Set the pin and callback
    samplingTimer(nullptr), samplingArgs({                   //  Assume no notifiers in constructor, instantiate sampling arguments for esp_timer_handle_t
        .callback = onTimer,                                    //      set callback to be input callback of nullptr
        .arg = this,                                            //      set arguments to pointer representing 'this'
        .dispatch_method = ESP_TIMER_TASK,                      //      default timer task priority
        .name = name,                                           //      name of the timer
        .skip_unhandled_events = false                          //      flag all sampling events
    }),
    ready_(false),                                              //  set volatile flag false (no samples yet)
    wasRunning(false),                                          //  flag storing last state
    reading(0),                                                 //  0 ADC reading
    failed(false),                                              //  haven't failed yet...
    finger(finger_)                                             //  set the finger to input (index)

{                                                           //  --- end initializer-list syntax
    sensorCount++;                                              // increment static value counting calls to constructor (devices attached)
} // end constructor

/* */
void FlexSensor::onTimer(void *arg) {
    portENTER_CRITICAL(&mux);
    auto cast = static_cast<FlexSensor*>(arg);
    cast->ready_ = true;
    portEXIT_CRITICAL(&mux);
}
/* */
void FlexSensor::setup() {
    failed = false;
    if (samplingTimer != nullptr) {
        sr::out << "Flex sensor already initialized. Deleting old timer." << sr::endl;
        if (esp_timer_is_active(samplingTimer)) {
            esp_timer_stop(samplingTimer);
            esp_rom_delay_us(500000);
        }
        esp_timer_delete(samplingTimer);
        samplingTimer = nullptr;
        esp_rom_delay_us(500000);
    }
    esp_err_t err = esp_timer_create(&samplingArgs, &samplingTimer);
    if (err != ESP_OK) {
        sr::out << "Failed to create timer: " << esp_err_to_name(err) << sr::endl;
        failed = true;
    } else {
        ready_ = false;
    }
}
void FlexSensor::loop() {
    if (failed) return;
    if (!ready_) return;
    ready_ = false;
    if (pin_.has_value()) {
        reading = analogRead(pin_.value());
        //sr::out << "Sensor count #" << sensorCount << " reading: " << reading << sr::endl;
        notifier_(reading, this->name);
    }
}
bool FlexSensor::setPin(std::optional<uint16_t> pin) {
    // 1) Log entry and inputs
    sr::out << "[setPin] entry: "
           << "timer=" << (samplingTimer ? "valid" : "null")
           << ", pin_in=" << (pin.has_value() ? pin.value() : UINT16_MAX)
           << sr::endl;

    // 2) Did we already create the timer and is it running?
    bool wasActive = (samplingTimer != nullptr) && esp_timer_is_active(samplingTimer);
    sr::out << "[setPin] wasActive=" << wasActive << sr::endl;

    // 3) Disable path
    if (!pin.has_value()) {
        sr::out << "[setPin] disabling sensor" << sr::endl;
        setActive(false);
        pin_ = std::nullopt;
        sr::out << "[setPin] exit OK (disabled)" << sr::endl;
        return true;
    }

    // 4) Pin‐range check
    if (pin.value() < A0 || pin.value() > A7) {
        sr::out << "[setPin] invalid pin: " << pin.value() << " (must be A0–A7)" << sr::endl;
        return false;
    }

    // 5) Must have a timer before we can stop/restart it
    if (!samplingTimer) {
        sr::out << "[setPin] ERROR: timer uninitialized. Call setup() first." << sr::endl;
        return false;
    }

    // 6) If it was running, stop it cleanly
    if (wasActive) {
        sr::out << "[setPin] stopping timer..." << sr::endl;
        esp_timer_stop(samplingTimer);
        delayMicroseconds(5000);
        sr::out << "[setPin] timer stopped" << sr::endl;
    }

    // 7) Actually assign the new pin
    pin_ = pin.value();
    sr::out << "[setPin] new pin set to A" << (pin.value() - A0)
           << " (raw " << pin.value() << ")" << sr::endl;

    // 8) If it was running before, restart it
    if (wasActive) {
        portENTER_CRITICAL(&mux);
        ready_ = true;
        portEXIT_CRITICAL(&mux);

        esp_timer_start_periodic(samplingTimer, samplingInterval_);
        sr::out << "[setPin] timer restarted" << sr::endl;
    }

    // 9) All done
    sr::out << "[setPin] exit OK" << sr::endl;
    return true;
}


void FlexSensor::setSamplingInterval(uint64_t interval) {
    samplingInterval_ = interval;
}

void FlexSensor::setNotifier(std::function<void(uint16_t, const char *)> notifier) {
    notifier_ = std::move(notifier);
    if (notifier == nullptr) {
        sr::out << "Notifier set to nullptr. " << sr::endl;
    }
}
void FlexSensor::setFinger(Finger finger) {
    this->finger = finger;
}

void FlexSensor::setActive(bool active) {
    if (failed) {
        sr::out << "Cannot activate sensor as it failed. Call setup() again to reinitialize." << sr::endl;
        return;
    }
    if (esp_timer_is_active(samplingTimer)) {
        if (!active) {
            esp_timer_stop(samplingTimer);
            sr::out << "Flex sensor stopped." << sr::endl;
            ready_ = false;
        }
    } else {
        if (active) {
            ready_ = true;
            esp_timer_start_periodic(samplingTimer, samplingInterval_);
            sr::out << "Flex sensor started." << sr::endl;
        }
    }
}
FlexSensor::~FlexSensor() {
    if (esp_timer_is_active(samplingTimer)) {
        esp_timer_stop(samplingTimer);
        esp_timer_delete(samplingTimer);
    }
}
void FlexSensor::setName(const char *name) {
    this->name = name;
}

