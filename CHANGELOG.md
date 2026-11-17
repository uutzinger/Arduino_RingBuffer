# Changelog

## 2.1.0
- `push()` accepts an optional `size_t* overwritten` that reports how many elements the push lost (overwritten older data plus input that exceeded the capacity). The value is determined inside the push's critical section, so callers such as MPRBuffer can count overruns exactly instead of checking `available()` before pushing. Existing calls are unchanged.
- Added host test for the lost-element count.

## 2026 Mar 22
- Fixed template member definitions to remain valid when another library injects a conflicting `RingBuffer` symbol into scope.
- Documented and shipped the global-qualification fix needed for builds that also pull in `audio_tools::RingBuffer`.

## 2026 Mar 13
- Fixed overwrite handling for writes larger than the buffer capacity.
- Fixed index sizing so full buffers work correctly at capacities such as 256.
- Restricted supported element types to trivially copyable types and documented that requirement.
- Added caller-provided `Storage` support for retained-memory use cases such as ESP32 RTC memory.
- Updated the ISR examples to drain all buffered values and print outside interrupt-disabled sections.
- Reworked the ESP32 RTC deep-sleep example to preserve ring buffer contents across wakeups.
- Corrected Arduino library metadata to reference `RingBuffer.h`.
- Standardized ESP32 feature detection on `ARDUINO_ARCH_ESP32`.

## 2025 Dec
- Added ESP32 task locks.

## 2025 Feb 24
- First release.
