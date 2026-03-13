#include <Arduino.h>
#include "RingBuffer.h"

#if !defined(ARDUINO_ARCH_ESP32)
#error "This example targets ESP32 boards with RTC memory support."
#endif

#include "esp_sleep.h"

constexpr size_t kBufferCapacity = 16;
using RTCBuffer = RingBuffer<uint8_t, kBufferCapacity>;

// Store raw ring buffer state in RTC memory so it survives deep sleep resets.
RTC_DATA_ATTR RTCBuffer::Storage rtcBufferStorage;
RTC_DATA_ATTR uint8_t nextValue = 1;

RTCBuffer myBuffer(rtcBufferStorage, true);

void printBufferContents() {
    uint8_t snapshot[kBufferCapacity];
    size_t count = myBuffer.peek(snapshot, myBuffer.available());

    Serial.print("Buffer contains ");
    Serial.print(count);
    Serial.println(" value(s):");

    if (count == 0) {
        Serial.println("(empty)");
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        Serial.print(snapshot[i]);
        Serial.print((i + 1) < count ? ' ' : '\n');
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("RingBuffer Example - Survives Deep Sleep");

    bool wokeFromDeepSleep = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
    if (!wokeFromDeepSleep) {
        myBuffer.clear();
        nextValue = 1;
        Serial.println("Fresh start: cleared RTC ring buffer");
    } else {
        Serial.println("Woke from deep sleep: restored RTC ring buffer");
    }

    printBufferContents();
}

void loop() {
    myBuffer.push(nextValue, true);
    Serial.print("Pushed ");
    Serial.println(nextValue);
    ++nextValue;

    printBufferContents();

    // Go to deep sleep
    Serial.println("Going to deep sleep...");
    esp_sleep_enable_timer_wakeup(5000000); // Wake up in 5 sec
    esp_deep_sleep_start();
}
