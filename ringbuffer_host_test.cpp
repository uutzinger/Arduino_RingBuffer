#include "RingBuffer.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

struct Sample {
    uint16_t id;
    float reading;
};

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

bool testUint8Basic() {
    RingBuffer<uint8_t, 8> buffer;
    const uint8_t values[] = {1, 2, 3, 4};
    uint8_t out[4] = {};

    return check(buffer.push(values, 4) == 4, "uint8_t push count") &&
           check(buffer.available() == 4, "uint8_t available after push") &&
           check(buffer.peek(out, 4) == 4, "uint8_t peek count") &&
           check(out[0] == 1 && out[1] == 2 && out[2] == 3 && out[3] == 4, "uint8_t peek contents") &&
           check(buffer.pop(out, 4) == 4, "uint8_t pop count") &&
           check(out[0] == 1 && out[1] == 2 && out[2] == 3 && out[3] == 4, "uint8_t pop contents") &&
           check(buffer.available() == 0, "uint8_t available after pop");
}

bool testOverwriteKeepsNewest() {
    RingBuffer<uint8_t, 8> buffer;
    const uint8_t values[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    uint8_t out[8] = {};

    return check(buffer.push(values, 12, true) == 8, "overwrite push count") &&
           check(buffer.available() == 8, "overwrite available") &&
           check(buffer.pop(out, 8) == 8, "overwrite pop count") &&
           check(out[0] == 4 && out[1] == 5 && out[2] == 6 && out[3] == 7 &&
                     out[4] == 8 && out[5] == 9 && out[6] == 10 && out[7] == 11,
                 "overwrite keeps newest values");
}

bool testInt16Wrap() {
    RingBuffer<int16_t, 8> buffer;
    const int16_t first[] = {10, 20, 30, 40, 50, 60};
    int16_t out[4] = {};

    if (!check(buffer.push(first, 6) == 6, "int16_t initial push")) return false;
    if (!check(buffer.consume(4) == 4, "int16_t consume")) return false;

    const int16_t second[] = {70, 80, 90, 100};
    return check(buffer.push(second, 4) == 4, "int16_t wrapped push") &&
           check(buffer.pop(out, 4) == 4, "int16_t wrapped pop") &&
           check(out[0] == 50 && out[1] == 60 && out[2] == 70 && out[3] == 80, "int16_t wrapped contents");
}

bool testFloatPeekConsume() {
    RingBuffer<float, 8> buffer;
    const float values[] = {1.25f, 2.5f, 3.75f};
    float out[3] = {};

    return check(buffer.push(values, 3) == 3, "float push") &&
           check(buffer.peek(out, 2) == 2, "float peek count") &&
           check(out[0] == 1.25f && out[1] == 2.5f, "float peek contents") &&
           check(buffer.consume() == 1, "float consume one") &&
           check(buffer.pop(out, 2) == 2, "float pop after consume") &&
           check(out[0] == 2.5f && out[1] == 3.75f, "float pop contents");
}

bool testStructType() {
    RingBuffer<Sample, 4> buffer;
    const Sample values[] = {
        {1, 0.5f},
        {2, 1.5f},
        {3, 2.5f},
    };
    Sample out[3] = {};

    return check(buffer.push(values, 3) == 3, "struct push") &&
           check(buffer.pop(out, 3) == 3, "struct pop") &&
           check(out[0].id == 1 && out[0].reading == 0.5f, "struct first value") &&
           check(out[1].id == 2 && out[1].reading == 1.5f, "struct second value") &&
           check(out[2].id == 3 && out[2].reading == 2.5f, "struct third value");
}

bool testSingleStructPushPop() {
    RingBuffer<Sample, 4> buffer;
    const Sample input = {42, 12.5f};
    Sample output = {};

    return check(buffer.push(input) == 1, "single struct push") &&
           check(buffer.available() == 1, "single struct available") &&
           check(buffer.pop(output) == 1, "single struct pop") &&
           check(output.id == 42 && output.reading == 12.5f, "single struct contents") &&
           check(buffer.available() == 0, "single struct empty after pop");
}

bool testRetainedStorage() {
    using RetainedBuffer = RingBuffer<uint8_t, 8>;
    RetainedBuffer::Storage storage = {};

    {
        RetainedBuffer writer(storage, false);
        const uint8_t values[] = {21, 22, 23};
        if (!check(writer.push(values, 3) == 3, "retained push")) return false;
        if (!check(writer.available() == 3, "retained available after write")) return false;
    }

    {
        RetainedBuffer reader(storage, true);
        uint8_t out[3] = {};
        return check(reader.available() == 3, "retained available after reconstruct") &&
               check(reader.pop(out, 3) == 3, "retained pop") &&
               check(out[0] == 21 && out[1] == 22 && out[2] == 23, "retained contents");
    }
}

}  // namespace

int main() {
    bool ok = true;

    ok = testUint8Basic() && ok;
    ok = testOverwriteKeepsNewest() && ok;
    ok = testInt16Wrap() && ok;
    ok = testFloatPeekConsume() && ok;
    ok = testStructType() && ok;
    ok = testSingleStructPushPop() && ok;
    ok = testRetainedStorage() && ok;

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "PASS: ring buffer host tests" << '\n';
    return EXIT_SUCCESS;
}
