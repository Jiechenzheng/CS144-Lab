#include "tcp_sender.hh"
#include "buffer.hh"

#include "tcp_config.hh"

#include <algorithm>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _timer(retx_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    _rcv_window_free_space = _rcv_window_size >= _bytes_in_flight ? _rcv_window_size - _bytes_in_flight : 0;

    /* read until no buffer or no window_size */
    while (_first_send ||
        (_rcv_window_free_space != 0 && _stream.buffer_empty() == false) ||
        (_rcv_window_free_space != 0 && _stream.buffer_empty() == true && _stream.eof() == true && _fin_sent == false))
    {
        /* read bytes into segment */
        size_t length = 0;
        length = std::min(std::min(_stream.buffer_size(), _first_send ? _rcv_window_free_space - 1 : _rcv_window_free_space), TCPConfig::MAX_PAYLOAD_SIZE); // in the reference of stream bytes
        TCPSegment seg = make_segment(length);
        _bytes_in_flight += seg.length_in_sequence_space();
        _next_seqno += seg.length_in_sequence_space();

        /* push into segment_out queue */
        _segments_out.push(seg);
        _outstanding_segments.push(seg);

        if (!_timer.if_start()) _timer.if_start() = true;
        
        _rcv_window_free_space -= seg.length_in_sequence_space();
    }

    return;
}

void TCPSender::send_test_segment() {
    /* send 1 byte segment */
    if (_stream.buffer_empty() == false ||
        (_stream.buffer_empty() == true && _stream.eof() == true && _fin_sent == false)) {

        /* read one byte into segment, taken consider the case if no bytes remaining */
        TCPSegment seg = make_segment(1, true);
        _bytes_in_flight += seg.length_in_sequence_space();
        _next_seqno += seg.length_in_sequence_space();

        /* push into segment out queue */
        _segments_out.push(seg);
        _outstanding_segments.push(seg);
        if (!_timer.if_start())
            _timer.if_start() = true;
    } else {
        return;
    }

    return;
}

TCPSegment TCPSender::make_segment(size_t len, bool test) {
    uint64_t index = _stream.bytes_read();

    /* construct payload */
    std::string payload;

    if (test == true)
    {
        payload = _stream.read(len);
        TCPSegment seg;
        seg.payload() = Buffer(payload.data());
        uint64_t abs_seqno = index + 1;
        seg.header().seqno = wrap(abs_seqno, _isn);

        // if no byte but fin
        if (_stream.eof() && seg.length_in_sequence_space() < 1)
        {
            seg.header().fin = true;
            _fin_sent = true;
        }

        return seg;
    }
    else
    {
        payload = _stream.read(len);
        TCPSegment seg;
        // seg.payload() = Buffer{_stream.read(len)};
        seg.payload() = Buffer(payload.data());    // ??
        if (_first_send == true)    // the first segment
        {
            seg.header().syn = true;
            seg.header().seqno = _isn;
            _first_send = false;
        }
        else    // segments except the first one
        {
            uint64_t abs_seqno = index + 1;
            seg.header().seqno = wrap(abs_seqno, _isn);
        }

        /* if the last segment, set fin flag */
        if (_stream.eof() && seg.length_in_sequence_space() < _rcv_window_free_space)
        {
            seg.header().fin = true;
            _fin_sent = true;
        }

        return seg;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _checkpoint);

    /* check invalid ackno */
    if (abs_ackno > _next_seqno)
    {
        sponge_log(LOG_ERR, "impossible ackno number: it is larger than _next_seqno");
        return;
    }


    if (abs_ackno < _least_abs_seqno_not_acked) // invalid ackno
    {
        sponge_log(LOG_ERR, "ackno small than _least_abs_seqno_not_acked");
        return;
    }
    else if (abs_ackno == _least_abs_seqno_not_acked)   // this ackno is in flight
    {
        _rcv_window_size = window_size;
        if (_rcv_window_size == 0)
        {
            send_test_segment();
            return;
        }
        else
        {
            fill_window();
            return;
        }
    }
    else    // abs_ackno > _least_abs_seqno_not_acked
    {
        _rcv_window_size = window_size;

        /* pop out the acknowledged segments */
        while (!_outstanding_segments.empty() &&
               unwrap(_outstanding_segments.front().header().seqno, _isn, _checkpoint) +
                       _outstanding_segments.front().length_in_sequence_space() <=
                   abs_ackno) {

            _outstanding_segments.pop();
        }

        _bytes_in_flight -= (abs_ackno - _least_abs_seqno_not_acked);

        if (_rcv_window_size == 0)  // window size == 0, send only 1 byte test_segment
        {
            send_test_segment();
            // _timer.setRTO(_initial_retransmission_timeout);

            _checkpoint = _least_abs_seqno_not_acked;
            _least_abs_seqno_not_acked = abs_ackno;

            if (!_outstanding_segments.empty())
            {
                _timer.restart();
            }

            _consecutive_retransmissions = 0;
            return;
        }
        else    // window_size > 0, fill_window will read bytes
        {
            fill_window();

            // since the oldest segment gets acked, update the timer
            _timer.setRTO(_initial_retransmission_timeout);

            _checkpoint = _least_abs_seqno_not_acked;
            _least_abs_seqno_not_acked = abs_ackno;

            if (!_outstanding_segments.empty())
            {
                _timer.restart();
            }
            
            _consecutive_retransmissions = 0;
            return;
        }
    }
    
    return;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.time_passed() += ms_since_last_tick;
    time_t time_passed = _timer.time_passed();

    /* compare with RTO, if larger, resend the segment */
    if (time_passed >= _timer.get_retx_timeout() && !_outstanding_segments.empty())
    {
        _segments_out.push(_outstanding_segments.front());
        if (_rcv_window_size != 0)
        {
            _consecutive_retransmissions++;
            _timer.setRTO(_timer.getRTO() * 2);
        }

        _timer.restart();
    }

    return;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg = make_segment(0);
    _segments_out.push(seg);
    return;
}
