#include "tcp_receiver.hh"
#include <algorithm>
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader& header = seg.header();
    uint64_t abs_seqno;
    
    /* when syn first appears */
    if (_first == true && header.syn == true){
        _isn = header.seqno;
        _first = false;
    }

    /* all cases */
    if (_isn.has_value())
    {
        abs_seqno = unwrap(header.seqno, _isn.value(), _checkpoint);

        /* invalid seqno */
        if (abs_seqno == 0 && !header.syn) return;

        /* write data, along with fin flag if has */
        uint64_t index = std::max(static_cast<int64_t>(abs_seqno) - 1, 0l); // this is index in byte stream
        
        _reassembler.push_substring(seg.payload().copy(), index, header.fin);

        /* set ackno, according to first unressambled byte */
        _ackno = wrap( _reassembler.stream_out().input_ended() ? _reassembler.stream_out().bytes_written() + 2 : _reassembler.stream_out().bytes_written() + 1, _isn.value());

        /* update checkpoint */
        _checkpoint = abs_seqno + seg.length_in_sequence_space() - 1;
    }

    return;
    
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (!_isn.has_value())
        return {};
    else
        return _ackno.value();
    
    
 }

size_t TCPReceiver::window_size() const { 
    return _reassembler.stream_out().remaining_capacity();
 }
