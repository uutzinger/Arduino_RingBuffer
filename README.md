# RingBuffer Library for Embedded Systems such as Arduino

A lightweight, header-only circular buffer for trivially copyable element types that mirrors the API in `RingBuffer.h`.

## Features

- Template-based buffer supporting trivially copyable `T` with a power-of-two capacity `N`.
- ESP32-friendly: per-instance spinlocks guard critical sections when `ARDUINO_ARCH_ESP32` is defined.
- Overload-aware helpers for single items or contiguous blocks when calling `push`, `pop`, `peek`, and `consume`.
- Runtime helpers: `available()`, `capacity()`, and `clear()` provide buffer insights and maintenance.
- Optional overwriting behaviour during `push` to discard oldest data when the buffer is full.
- Optional caller-provided `Storage` lets the buffer wrap retained memory such as ESP32 `RTC_DATA_ATTR`.

Supported element types are POD-like values such as `uint8_t`, `int16_t`, `float`, or simple structs that are trivially copyable. Types with non-trivial constructors, destructors, or internal ownership such as `String` or `std::string` are intentionally not supported.

The class exposes the following methods:

- `RingBuffer()`
- `RingBuffer(Storage& storage, bool preserve = false)`
- `push(const T& data, bool overwrite = false, size_t* overwritten = nullptr)`
- `push(const T* data, size_t data_len, bool overwrite = false, size_t* overwritten = nullptr)`
- `pop(T& output)`
- `pop(T* output, size_t len)`
- `peek(T& output) const`
- `peek(T* output, size_t len) const`
- `consume()`
- `consume(size_t len)`
- `available() const`
- `capacity() const`
- `clear()`

`overwritten` is optional. If given, it receives the number of elements lost by this push: older elements that were overwritten plus input elements dropped because a single push was larger than the capacity. It is computed inside the same critical section as the push, so it is exact even when another task pops concurrently. It is 0 if the push was rejected.

For retained-memory use cases, declare `RingBuffer<T, N>::Storage` in the retained region and reconstruct `RingBuffer<T, N>` around it after reset.

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
