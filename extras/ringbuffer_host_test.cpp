#include "../RingBuffer.h"

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

bool testOverwrittenCount() {
    RingBuffer<uint8_t, 4> buffer;
    size_t lost = 99;
    const uint8_t v[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    bool ok = check(buffer.push(v[0], true, &lost) == 1 && lost == 0, "empty buffer: nothing lost");
    ok = check(buffer.push(v, 3, true, &lost) == 3 && lost == 0, "fits exactly: nothing lost") && ok;
    ok = check(buffer.available() == 4, "buffer full") && ok;
    ok = check(buffer.push(v[3], true, &lost) == 1 && lost == 1, "single push on full buffer loses one") && ok;
    ok = check(buffer.push(v, 3, true, &lost) == 3 && lost == 3, "block push on full buffer loses three") && ok;
    ok = check(buffer.push(v, 10, true, &lost) == 4 && lost == 10, "oversized push: 4 old + 6 input lost") && ok;

    uint8_t out[4] = {};
    ok = check(buffer.pop(out, 4) == 4 && out[0] == 7 && out[3] == 10, "newest elements retained") && ok;

    buffer.push(v, 4, true);
    lost = 99;
    ok = check(buffer.push(v[0], false, &lost) == 0 && lost == 0, "rejected push (no overwrite) reports 0 lost") && ok;
    ok = check(buffer.push(v, 0, true, &lost) == 0 && lost == 0, "zero length push reports 0 lost") && ok;
    ok = check(buffer.push(v[0], true) == 1, "pointer argument stays optional") && ok;
    return ok;
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
    ok = testOverwrittenCount() && ok;

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "PASS: ring buffer host tests" << '\n';
    return EXIT_SUCCESS;
}
