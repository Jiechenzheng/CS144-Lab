#include "wrapping_integers.hh"
#include <iostream>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

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
    uint64_t res_cur = 0;
    uint64_t res_prev = 0;
    uint64_t i = 0;
    uint64_t res = 0;
    for (i = 0; i < (1ll<<32); i++)
    {
        // cout << "the i is " << i << endl;
        // cout << "the raw_value() is " << n.raw_value() << endl;
        // cout << "the static_cast raw_value() is " << static_cast<uint64_t>(n.raw_value()) << endl;
        // cout << "the isn.raw_value() is " << isn.raw_value() << endl;
        // cout << "the static_cast isn.raw_value is " << static_cast<uint64_t>(isn.raw_value()) << endl;
        
        if ((static_cast<uint64_t>(n.raw_value()) + i * (1ll<<32)) > static_cast<uint64_t>(isn.raw_value()))
        {
            res_prev = res_cur;
            res_cur = static_cast<uint64_t>(n.raw_value()) + i * (1ll<<32) - static_cast<uint64_t>(isn.raw_value());
            if (res_cur > checkpoint)
                break;
        }

        // cout << "the unwrap res_cur is "  << res_cur << endl;
        // cout << "the checkpoint is " << checkpoint << endl;

    }

    if ((res_cur - res_prev) != (1ll<<32) || (res_prev < checkpoint && res_cur < checkpoint))
    {
        res = res_cur;
    }
    else
    {
        res = (res_cur - checkpoint) <= (checkpoint - res_prev) ? res_cur : res_prev;
    }
    // cout << "the final unwrap res is "  << res << endl;
    // cout << "the checkpoint is " << checkpoint << endl;
    // cout << "the i is " << i << endl;
    return res;
}
