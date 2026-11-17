// ************************************************************************
// RingBuffer.h
//
// Ring buffer (circular buffer) implementation in C++
// Thread safe on ESP32 using per-instance spinlocks
// Template class supporting trivially copyable data types T
// Supports caller-provided storage for retained-memory use cases
// Buffer size N must be a power of 2 for efficiency
//
// push
//   Push data into the ring buffer
//   If overwrite is true, older data is overwritten when buffer is full
//   Optional 'overwritten' receives the number of elements lost by this push
//   (older elements overwritten plus input elements dropped because they exceed the capacity),
//   determined in the same critical section as the push
// pop
//   Pop data from the ring buffer
// peek
//   Peek at data in the ring buffer without removing it
// consume
//   Remove data from the ring buffer without copying it out
// available
//   Get number of elements currently in the buffer
// capacity
//   Get total capacity of the buffer
// clear
//   Clear the buffer
//
// ************************************************************************
#ifndef RB_MIN
    #define RB_MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stddef.h>     // for size_t
#include <string.h>     // for memcpy
#include <stdint.h>     // for uint8_t, uint16_t

#if defined(ARDUINO_ARCH_ESP32)
    # include "freertos/FreeRTOS.h"
    # include "freertos/portmacro.h"
#endif

// Define critical section helpers BEFORE class so they are visible where used.
// They reference 'this->mux_' which is a per-instance spinlock.
#ifdef ARDUINO_ARCH_ESP32
#  define RB_CRITICAL_ENTER() portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&this->mux_))
#  define RB_CRITICAL_EXIT()  portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&this->mux_))
#else
#  define RB_CRITICAL_ENTER()
#  define RB_CRITICAL_EXIT()
#endif

// Minimal replacement for std::conditional: avr-gcc does not ship <type_traits>
namespace rb_detail {
template <bool Cond, typename A, typename B> struct Conditional { typedef A type; };
template <typename A, typename B> struct Conditional<false, A, B> { typedef B type; };
}  // namespace rb_detail

template <typename T, size_t N>
class RingBuffer {
public:
    static_assert(__is_trivially_copyable(T),     // compiler builtin (GCC, Clang, MSVC), no <type_traits> needed
                  "RingBuffer requires trivially copyable element types");

    // Select optimal index type based on buffer size, including a full-buffer count.
    using IndexType = typename rb_detail::Conditional<
        (N <= UINT8_MAX), uint8_t,
        typename rb_detail::Conditional<(N <= UINT16_MAX), uint16_t, size_t>::type
    >::type;

    struct Storage {
        T buffer[N];
        IndexType head;
        IndexType tail;
        IndexType count;
        uint32_t magic;
    };

    RingBuffer() : storage_(&localStorage_) {
        initializeMux();
        initializeStorage(false);
    }

    explicit RingBuffer(Storage& storage, bool preserve = false) : storage_(&storage) {
        initializeMux();
        initializeStorage(preserve);
    }

    // Push data into the ring buffer
    // data: pointer to data that will be added to the buffer
    // data_len: number of data elements
    // overwrite: if true, older data is overwritten when buffer is full
    // overwritten: optional, set to the number of elements lost by this push (0 if none); set to 0 when push fails
    size_t push(const T& data, bool overwrite = false, size_t* overwritten = nullptr);
    size_t push(const T* data, size_t data_len, bool overwrite = false, size_t* overwritten = nullptr);

    // Pop a specified number of characters from the ring buffer
    size_t pop(T& output);
    size_t pop(T* output, size_t len);
    
    // Peek a specified number of characters from the ring buffer and leave them in the buffer
    size_t peek(T& output) const;
    size_t peek(T* output, size_t len) const;

    // Consume (remove) a specified number of characters from the ring buffer without copying them out
    size_t consume();
    size_t consume(size_t len);

    size_t available() const {
        // Snapshot under critical section to avoid torn reads
        #if defined(ARDUINO_ARCH_ESP32)
            RB_CRITICAL_ENTER();
            size_t c = storage_->count;
            RB_CRITICAL_EXIT();
            return c;
        #else
            return storage_->count;
        #endif
    }
    size_t capacity() const { return N; }
    void clear();

private:
    static constexpr bool isPowerOfTwo(size_t n) { return n != 0 && (n & (n - 1)) == 0; }
    static_assert(isPowerOfTwo(N), "RingBuffer capacity must be a power of 2 for efficiency");
    static constexpr uint32_t kStorageMagic = 0x52425546UL; // 'RBUF'

    void initializeMux() {
        #if defined(ARDUINO_ARCH_ESP32)
            mux_ = portMUX_INITIALIZER_UNLOCKED;
        #endif
    }

    void initializeStorage(bool preserve) {
        if (!preserve || storage_->magic != kStorageMagic) {
            storage_->head = 0;
            storage_->tail = 0;
            storage_->count = 0;
        }
        storage_->magic = kStorageMagic;
    }

    Storage localStorage_;
    Storage* storage_;

#if defined(ARDUINO_ARCH_ESP32)
    // Per-instance spinlock for critical sections. Mark mutable so it can be used in const methods.
    mutable portMUX_TYPE mux_;
#endif
};

// Push a single element
template <typename T, size_t N>
size_t ::RingBuffer<T, N>::push(const T& data, bool overwrite, size_t* overwritten) {
    return push(&data, 1, overwrite, overwritten);
}

// Push multiple elements
template <typename T, size_t N>
size_t ::RingBuffer<T, N>::push(const T* data, size_t data_len, bool overwrite, size_t* overwritten) {
    if (overwritten) *overwritten = 0;
    if (data_len == 0) return 0;
    size_t lost = 0;

    // If overwrite is enabled and the caller provides more than the capacity,
    // only the newest N elements can remain in the buffer.
    if (overwrite && data_len > N) {
        lost = data_len - N;                    // input that can never fit
        data += lost;
        data_len = N;
    }

    RB_CRITICAL_ENTER();

    size_t available_space = N - storage_->count;
    if (data_len > available_space && !overwrite) {
        RB_CRITICAL_EXIT();
        return 0;
    }

    if (data_len > available_space) {
        // Overwriting: advance tail to free up space
        size_t overflow = data_len - available_space;
        lost += overflow;
        storage_->tail = (storage_->tail + overflow) & (N - 1);
    }

    // Optimized single-element push
    if (data_len == 1) {
        storage_->buffer[storage_->head] = *data;
    } else {
        // Multi-element push
        size_t firstPart = RB_MIN(data_len, N - storage_->head);
        memcpy(&storage_->buffer[storage_->head], data, firstPart * sizeof(T));

        size_t secondPart = data_len - firstPart;
        if (secondPart > 0) {
            // Wrap around the buffer and write remaining bytes at the beginning
            memcpy(storage_->buffer, data + firstPart, secondPart * sizeof(T));
        }
    }

    // Update head and count
    storage_->head = (storage_->head + data_len) & (N - 1);
    storage_->count = static_cast<IndexType>(
        RB_MIN(static_cast<size_t>(N), static_cast<size_t>(storage_->count) + data_len));

    RB_CRITICAL_EXIT();
    if (overwritten) *overwritten = lost;
    return data_len;
}

// Pop a single element
template <typename T, size_t N>
size_t ::RingBuffer<T, N>::pop(T& output) {
    return pop(&output, 1);
}

// Pop multiple elements
template <typename T, size_t N>
size_t ::RingBuffer<T, N>::pop(T* output, size_t len) {
    if (len == 0) return 0;
    RB_CRITICAL_ENTER();

    if (storage_->count == 0) {
        RB_CRITICAL_EXIT();
        return 0; // Buffer empty
    }

    size_t charsToRead = RB_MIN(len, static_cast<size_t>(storage_->count));
    size_t firstPart   = RB_MIN(charsToRead, N - storage_->tail);

    if (charsToRead == 1) {
        *output = storage_->buffer[storage_->tail];
    } else {
        memcpy(output, &storage_->buffer[storage_->tail], firstPart * sizeof(T));
        size_t secondPart = charsToRead - firstPart;
        if (secondPart > 0) {
            // Wrap around the buffer and read remaining bytes from the beginning
            memcpy(output + firstPart, storage_->buffer, secondPart * sizeof(T));
        }
    }

    storage_->tail = (storage_->tail + charsToRead) & (N - 1);
    storage_->count = static_cast<IndexType>(static_cast<size_t>(storage_->count) - charsToRead);

    // Reset tail & head when buffer becomes empty
    if (storage_->count == 0) {
        storage_->tail = 0;
        storage_->head = 0;
    }

    RB_CRITICAL_EXIT();
    return charsToRead;
}

// Peek at single element
template <typename T, size_t N>
size_t ::RingBuffer<T, N>::peek(T& output) const {
    RB_CRITICAL_ENTER();
    if (storage_->count == 0) {
        RB_CRITICAL_EXIT();
        return 0; // Buffer empty
    }
    output = storage_->buffer[storage_->tail]; // Read the element at tail without modifying it
    RB_CRITICAL_EXIT();
    return 1;
}

// Peek at multiple elements
template <typename T, size_t N>
size_t ::RingBuffer<T, N>::peek(T* output, size_t len) const {
    if (len == 0) return 0; // Nothing to peek
    RB_CRITICAL_ENTER();
    if (storage_->count == 0) {
        RB_CRITICAL_EXIT();
        return 0; // Nothing to peek
    }

    size_t peekLen = RB_MIN(len, static_cast<size_t>(storage_->count));
    size_t firstPart = RB_MIN(peekLen, N - storage_->tail);

    // Copy first segment
    memcpy(output, &storage_->buffer[storage_->tail], firstPart * sizeof(T));

    // Copy second segment if wrap-around occurs
    size_t secondPart = peekLen - firstPart;
    if (secondPart > 0) {
        memcpy(output + firstPart, storage_->buffer, secondPart * sizeof(T));
    }

    RB_CRITICAL_EXIT();
    return peekLen;
}

// Consume a single element
template <typename T, size_t N>
size_t ::RingBuffer<T, N>::consume() {
    RB_CRITICAL_ENTER();
    if (storage_->count == 0) {
        RB_CRITICAL_EXIT();
        return 0; // empty
    }
    storage_->tail = (storage_->tail + 1) & (N - 1);
    storage_->count = static_cast<IndexType>(static_cast<size_t>(storage_->count) - 1);
    if (storage_->count == 0) { storage_->tail = 0; storage_->head = 0; }
    RB_CRITICAL_EXIT();
    return 1;
}

// Consume multiple elements
template <typename T, size_t N>
size_t ::RingBuffer<T, N>::consume(size_t len) {
    if (len == 0) return 0;
    RB_CRITICAL_ENTER();
    if (storage_->count == 0) {
        RB_CRITICAL_EXIT();
        return 0;
    }
    size_t to_consume = RB_MIN(len, static_cast<size_t>(storage_->count));
    storage_->tail = (storage_->tail + to_consume) & (N - 1);
    storage_->count = static_cast<IndexType>(static_cast<size_t>(storage_->count) - to_consume);
    if (storage_->count == 0) { storage_->tail = 0; storage_->head = 0; }
    RB_CRITICAL_EXIT();
    return to_consume;
}

// Clear the buffer
template <typename T, size_t N>
void ::RingBuffer<T, N>::clear() {
    RB_CRITICAL_ENTER();
    storage_->head = 0;
    storage_->tail = 0;
    storage_->count = 0;
    storage_->magic = kStorageMagic;
    RB_CRITICAL_EXIT();
}

#endif // RINGBUFFER_H
