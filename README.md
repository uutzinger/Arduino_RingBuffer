# RingBuffer Library for Embedded Systems such as Arduino

A lightweight, header-only circular buffer that mirrors the API in `RingBuffer.h`.

## Features

- Template-based buffer supporting any `T` with a power-of-two capacity `N`.
- ESP32-friendly: per-instance spinlocks guard critical sections when `ARDUINO_ARCH_ESP32` is defined.
- Overload-aware helpers for single items or contiguous blocks when calling `push`, `pop`, `peek`, and `consume`.
- Runtime helpers: `available()`, `capacity()`, and `clear()` provide buffer insights and maintenance.
- Optional overwriting behaviour during `push` to discard oldest data when the buffer is full.

The class exposes the following methods:

- `push(const T& data, bool overwrite = false)`
- `push(const T* data, size_t data_len, bool overwrite = false)`
- `pop(T& output)`
- `pop(T* output, size_t len)`
- `peek(T& output) const`
- `peek(T* output, size_t len) const`
- `consume()`
- `consume(size_t len)`
- `available() const`
- `capacity() const`
- `clear()`

The buffer automatically resets `head` and `tail` indexes when emptied and maintains a bounded element count to prevent overflow.

## Installation

Clone or download [uutzinger/RingBuffer](https://github.com/uutzinger/RingBuffer) into your Arduino `libraries` folder.

## Dependencies

None

## Contributing

Urs Utzinger, 2025
ChatGPT, OpenAI

## License

See [LICENSE](LICENSE.txt).
