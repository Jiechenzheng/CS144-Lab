#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _timer.time_passed(); }

void TCPConnection::segment_received(const TCPSegment &seg) {
    /* reset timer */
    _timer.restart();

    const TCPHeader &header = seg.header();
    /* if there is RST flag, set error state and kill the connection */
    if (header.rst == true)
    {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();

        active() = false;
        return;
    }

    /* if it is the first segment with syn, no ack received, so we need to active send one */
    if (_first == true && header.syn == true)
    {
        _first = false;

        _receiver.segment_received(seg);
        _sender.fill_window();

        while (_sender.segments_out().empty() == false)
        {
            TCPSegment segment_out = _sender.segments_out().front();
            _sender.segments_out().pop();
            // set ackno and window size calculated by receiver
            segment_out.header().ackno = _receiver.ackno().value();
            segment_out.header().win = _receiver.window_size();
            segment_out.header().ack = true;
            _segments_out.push(segment_out);
        }
    }
    else    // not the first segment
    {
        _receiver.segment_received(seg);
        if (header.ack == true)
        {
            const WrappingInt32 outbound_ackno = header.ackno;
            const size_t outbound_win = header.win;

            // give sender the akcno and windows calculate by remote peer
            _sender.ack_received(outbound_ackno, outbound_win);

            // if segment_out is empty, generate a empty segment
            if (_sender.segments_out().empty() == true && _outbound_fin_acked == false)
            {
                _sender.send_empty_segment();
                _sender.segments_out().front().header().seqno = _sender.next_seqno();
            }
            
            // populate the fields and send out
            while (_sender.segments_out().empty() == false)
            {
                TCPSegment segment_out = _sender.segments_out().front();
                _sender.segments_out().pop();
                segment_out.header().ackno = _receiver.ackno().value();
                segment_out.header().win = _receiver.window_size();
                segment_out.header().ack = true;

                _segments_out.push(segment_out);
            }
        }
        else
        {
            sponge_log(LOG_ERR, "Expect ack in received segment, but no");
        }
    }

    // if inbound stream have fin sent, assume we sent an ack already
    if (_receiver.stream_out().input_ended() == true)
    {
        _outbound_fin_acked = true;
    }
    

    /* if passive tear down */
    if (_receiver.stream_out().input_ended() == true && _sender.stream_in().eof() == false)
    {
        _linger_after_streams_finish = false;
    }

    if (inbound_stream_ended() == true && outbound_stream_ended_fin_sent_acked() == true)  // sender's fin is acked
    {
        if (_linger_after_streams_finish == false)
        {
            active() = false;
        }
        else
        {
            _is_lingering = true;
        }
    }
    
    return;
}

bool TCPConnection::active() const { return _active; }

bool &TCPConnection::active() { return _active; }

size_t TCPConnection::write(const string &data) {
    ByteStream &bs = _sender.stream_in();
    size_t res = bs.write(data);
    return res;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // pass time to timer
    _timer.time_passed() += ms_since_last_tick;

    /* pass the time to sender, send out if sender send new segs */
    _sender.tick(ms_since_last_tick);
    while (_sender.segments_out().empty() == false)
    {
        TCPSegment segment_out = _sender.segments_out().front();
        _sender.segments_out().pop();

        // set ackno and window size calculated by receiver
        segment_out.header().ackno = _receiver.ackno().value();
        segment_out.header().win = _receiver.window_size();
        segment_out.header().ack = true;
        _segments_out.push(segment_out);
    }

    /* if consective retransmission larger than the limit*/
    if (_sender.consecutive_retransmissions() >= _cfg.MAX_RETX_ATTEMPTS)
    {
        // send segment with RST and abort the connection
        _sender.send_empty_segment();
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        seg.header().seqno = _sender.next_seqno();
        seg.header().rst = true;
        _segments_out.push(seg);

        // end the connection
        active() = false;
    }

    /* if lingering time is passed, cleanly end the connection */
    if (_is_lingering && time_since_last_segment_received() >= 10 * _cfg.rt_timeout)
    {
        active() = false;
    }
    
}

void TCPConnection::end_input_stream() {
    // need to send fin sync
    ByteStream &bs = _sender.stream_in();
    bs.end_input();

    // send fin segment intermediately
    _sender.fill_window();

    while (_sender.segments_out().empty() == false)
    {
        TCPSegment segment_out = _sender.segments_out().front();
        _sender.segments_out().pop();
        // set ackno and window size calculated by receiver
        segment_out.header().ackno = _receiver.ackno().value();
        segment_out.header().win = _receiver.window_size();
        segment_out.header().ack = true;
        _segments_out.push(segment_out);
    }

    return;
}

void TCPConnection::connect() {
    _first = false;
    _sender.fill_window();

    if (_sender.segments_out().empty() == false)
    {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        _segments_out.push(seg);
    }
    else
    {
        sponge_log(LOG_ERR, "Expect one segment, but got no");
    }

    return;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            _sender.send_empty_segment();
            TCPSegment seg = _sender.segments_out().front();
            _sender.segments_out().pop();
            seg.header().seqno = _sender.next_seqno();
            seg.header().rst = true;
            _segments_out.push(seg);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
