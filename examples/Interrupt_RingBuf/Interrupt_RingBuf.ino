// Example Arduino Sketch using RingBuffer with Interrupts
#include <Arduino.h>
#include "RingBuffer.h"

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// Define a RingBuffer for uint8_t with a size of 16
RingBuffer<uint8_t, 16> myBuffer;

volatile bool dataReady = false;

void IRAM_ATTR ISR_function() {
    myBuffer.push(42); // Example data
    dataReady = true;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("RingBuffer Example with Interrupts");

    pinMode(2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(2), ISR_function, RISING);
}

void loop() {
    if (dataReady) {
        while (true) {
            uint8_t value;
            size_t popped = 0;

            noInterrupts(); // Disable interrupts while accessing the shared buffer
            if (myBuffer.available() > 0) {
                popped = myBuffer.pop(value);
            } else {
                dataReady = false;
            }
            interrupts(); // Re-enable interrupts before any serial output

            if (!popped) {
                break;
            }

            Serial.print("Popped from ISR: ");
            Serial.println(value);
        }
    }
}
