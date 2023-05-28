#include "wrapping_integers.hh"
#include <iostream>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

static uint64_t abs_uint64(const uint64_t &val1, const uint64_t &val2)
{
    return val1 >= val2 ? (val1 - val2) : (val2 - val1);
}

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t wrapping_int32 = (static_cast<uint64_t>(isn.raw_value()) + n) % (static_cast<uint64_t>(1ll<<32));
    // cout << "the wrap result is " << wrapping_int32 << endl;
    return WrappingInt32{static_cast<uint32_t>(wrapping_int32)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t i = 0;
    uint64_t res = 0;

    i = checkpoint / (1ll<<32);
    if (static_cast<uint64_t>(n.raw_value() + i * (1ll<<32)) > static_cast<uint64_t>(isn.raw_value()))
    {
        res = static_cast<uint64_t>(n.raw_value() + i * (1ll<<32)) - static_cast<uint64_t>(isn.raw_value());
    }
    else
    {
        res = static_cast<uint64_t>(n.raw_value() + (i+1) * (1ll<<32)) - static_cast<uint64_t>(isn.raw_value());
    }

    /* when the res is not near checkpoint within (1ull<<32)/2, move one cycle forward or backward */
    while (abs_uint64(checkpoint, res) > ((1ull<<32)/2))
    {
        if (res > checkpoint)   // when the res is larger than checkpoint, move one cycle backward
        {
            if (res < (1ll<<32))    // unless the res is in the beginning cycle
                break;
            else
                res -= (1ll<<32);
        }
        else    // when the res is smaller than checkpoint, move one cycle forward
        {
            if (res > ((1ull<<63) - 1)) // unless the res is in the end cycle
                break;
            else
                res += (1ll<<32);
        }
    }
    return res;
}
