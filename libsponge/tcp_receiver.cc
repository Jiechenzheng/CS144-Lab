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
    if (_first == true)
    {
        if (header.syn == true)
        {
            _isn = header.seqno;
            _first = false;
            sponge_log(LOG_INFO, "received first sync from peer: header: %s, port: %u", header.summary().c_str(), header.sport);
        }
        else
        {
            sponge_log(LOG_ERR, "no syn flag in the first-received segment from peer: header: %s, port %u", header.summary().c_str(), header.sport);
        }
    }


    /* all cases */
    if (_isn.has_value())
    {
        abs_seqno = unwrap(header.seqno, _isn.value(), _checkpoint);

        /* invalid seqno */
        if (abs_seqno == 0 && !header.syn){
            sponge_log(LOG_ERR, "absolute sequence number is 0, expect not 0.");
            return;
        }

        /* write data, along with fin flag if has */
        uint64_t index = std::max(static_cast<int64_t>(abs_seqno) - 1, 0l); // this is index in byte stream

        _reassembler.push_substring(seg.payload().copy(), index, header.fin);

        /* set ackno, according to first unressambled byte */
        _ackno = wrap( _reassembler.stream_out().input_ended() ? _reassembler.stream_out().bytes_written() + 2 : _reassembler.stream_out().bytes_written() + 1, _isn.value());

        /* update checkpoint */
        _checkpoint = abs_seqno + seg.length_in_sequence_space() - 1;

        if (_reassembler.stream_out().input_ended())
        {
            sponge_log(LOG_INFO, "tcp_receiver.cc: input ended, total received bytes are %lu", _reassembler.stream_out().bytes_written());
        }
    }
    else
    {
        sponge_log(LOG_ERR, "_isn have no value, expect value");
    }
    
    return;
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (!_isn.has_value())
        return {};
    else
        return _ackno;
 }

size_t TCPReceiver::window_size() const {
    return _reassembler.stream_out().remaining_capacity();
 }
