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
        sponge_log(LOG_INFO, "receive RST flag from peer, %d", seg.header().sport);
        sponge_log(LOG_INFO, "mark streams with error, and set active to false directly");
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();

        set_active(false);
        return;
    }

    /* if it is the first segment with syn, no ack received, so we need to active send one */
    if (header.ack == false)
    {
        if (header.syn == false)
        {
            sponge_log(LOG_ERR, "assume it is the first received segment, but no syn flat");
            return;
        }

        sponge_log(LOG_INFO, "passive connection from peer, header: %s", seg.header().summary().c_str());
        _receiver.segment_received(seg);
        _sender.fill_window();

        // if segment_out is empty, generate an empty segment, seg.length_in_sequence_space() != 0 is to filter only ack segment
        if (_sender.segments_out().empty() == true && seg.length_in_sequence_space() != 0)
        {
            _sender.send_empty_segment();
            _sender.segments_out().front().header().seqno = _sender.next_seqno();
        }

        while (_sender.segments_out().empty() == false)
        {
            TCPSegment segment_out = _sender.segments_out().front();
            _sender.segments_out().pop();
            // set ackno and window size calculated by receiver
            if (_receiver.ackno().has_value()) {
                segment_out.header().ackno = _receiver.ackno().value();
                segment_out.header().ack = true;
            }
            segment_out.header().win = _receiver.window_size();

            _segments_out.push(segment_out);
        }
    }
    else    // not the first segment
    {
        // sponge_log(LOG_INFO, "receive message from peer, header: %s, length: %lu", seg.header().summary().c_str(), seg.length_in_sequence_space());
        _receiver.segment_received(seg);

        const WrappingInt32 outbound_ackno = header.ackno;
        const size_t outbound_win = header.win;

        // give sender the akcno and windows calculate by remote peer
        _sender.ack_received(outbound_ackno, outbound_win);

        // if segment_out is empty, generate a empty segment, seg.length_in_sequence_space() != 0 is to ignore only ack segment
        if (_sender.segments_out().empty() == true && seg.length_in_sequence_space() != 0)
        {
            _sender.send_empty_segment();
            _sender.segments_out().front().header().seqno = _sender.next_seqno();
        }

        // populate the fields and send out
        while (_sender.segments_out().empty() == false)
        {
            TCPSegment segment_out = _sender.segments_out().front();
            _sender.segments_out().pop();
            if (_receiver.ackno().has_value()) {
                segment_out.header().ackno = _receiver.ackno().value();
                segment_out.header().ack = true;
            }
            segment_out.header().win = _receiver.window_size();
            // sponge_log(LOG_INFO, "send message to peer, header: %s, length: %lu", segment_out.header().summary().c_str(), seg.length_in_sequence_space());


            _segments_out.push(segment_out);
        }

    }

    /* passive tear down when sender still has bytes while receiver has already been input ended */
    if (_receiver.stream_out().input_ended() == true && _sender.stream_in().eof() == false)
    {
        _linger_after_streams_finish = false;
    }

    /* inbound stream ends input and also sender's fin is acked */
    if (inbound_stream_ended() == true && outbound_stream_ended_fin_sent_acked() == true)
    {
        if (_linger_after_streams_finish == false)
        {
            sponge_log(LOG_INFO, "passive tear down, set active to false directly");
            set_active(false);
        }
        else
        {
            sponge_log(LOG_INFO, "active tear down, go to lingering");
            _is_lingering = true;
        }
    }

    return;
}

bool TCPConnection::active() const { return _active; }

void TCPConnection::set_active(bool val) { _active = val; return; }

size_t TCPConnection::write(const string &data) {
    ByteStream &bs = _sender.stream_in();
    size_t res = bs.write(data);

    // send the bytes over TCP if possible
    // shouldn't fill_window if establishment hasn't done, in this case, window size would be zero, so don't worry
    _sender.fill_window();

    // if more segments are generated populate the fields and send out
    while (_sender.segments_out().empty() == false)
    {
        TCPSegment segment_out = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            segment_out.header().ackno = _receiver.ackno().value();
            segment_out.header().ack = true;
        }
        segment_out.header().win = _receiver.window_size();
        _segments_out.push(segment_out);
    }

    return res;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // pass time to timer
    _timer.update_time_by_last_time_passed(ms_since_last_tick);

    /* pass the time to sender, send out if sender send new segs */
    _sender.tick(ms_since_last_tick);

    /* if consective retransmission larger than the limit*/
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS)
    {
        sponge_log(LOG_INFO, "consective retransmission larger than the limit");
        sponge_log(LOG_INFO, "send segment with RST flag, and set active to false");
        // send segment with RST and abort the connection
        _sender.send_empty_segment();
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        seg.header().seqno = _sender.next_seqno();
        seg.header().rst = true;
        _segments_out.push(seg);

        // set error for streams
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();

        // end the connection
        set_active(false);

        return;
    }

    while (_sender.segments_out().empty() == false)
    {
        TCPSegment segment_out = _sender.segments_out().front();
        _sender.segments_out().pop();

        // set ackno and window size calculated by receiver
        if (_receiver.ackno().has_value()) {
            segment_out.header().ackno = _receiver.ackno().value();
            segment_out.header().ack = true;
        }
        segment_out.header().win = _receiver.window_size();
        _segments_out.push(segment_out);
    }

    /* if lingering time is passed, cleanly end the connection */
    if (_is_lingering && time_since_last_segment_received() >= 10 * _cfg.rt_timeout)
    {
        sponge_log(LOG_INFO, "lingering time is passed, cleanly end the connection");
        set_active(false);
    }
    
    return;
}

void TCPConnection::end_input_stream() {
    // need to send fin sync
    ByteStream &bs = _sender.stream_in();
    bs.end_input();
    sponge_log(LOG_INFO,"end input stream, sequence number of Fin should be: %lu", _sender.get_isn() + bs.bytes_written() + 1);

    // send fin segment intermediately
    _sender.fill_window();

    while (_sender.segments_out().empty() == false)
    {
        TCPSegment segment_out = _sender.segments_out().front();
        _sender.segments_out().pop();
        // set ackno and window size calculated by receiver
        if (_receiver.ackno().has_value()) {
            segment_out.header().ackno = _receiver.ackno().value();
            segment_out.header().ack = true;
        }
        segment_out.header().win = _receiver.window_size();
        _segments_out.push(segment_out);
    }

    return;
}

void TCPConnection::connect() {
    _sender.fill_window();

    if (_sender.segments_out().empty() == false)
    {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        _segments_out.push(seg);
    }
    else
    {
        sponge_log(LOG_ERR, "expect one segment, but got no");
    }

    sponge_log(LOG_INFO, "active connection with peer, header: %s", _segments_out.front().header().summary().c_str());

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
