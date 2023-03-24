#include "byte_stream.hh"
#include <iostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) {
    _capacity = capacity;
    _stream.resize(capacity);
}

size_t ByteStream::write(const string &data) {
    size_t i = 0;
    int size = 0;
    while ((_w_ptr != _r_ptr || _empty) && (i < data.length()))
    {
        _stream[_w_ptr] = data[i++];
        size++;
        _w_ptr = (_w_ptr + 1)%_capacity;
        _empty = false;
    }

    _w_tot += size;

    return size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t l = min(len, buffer_size());
    std::string buf;

    int ptr = _r_ptr;
    for (size_t i = 0; i < l; i++)
    {
        buf += _stream[ptr];
        ptr = (ptr + 1)%_capacity;
    }

    return buf;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t length = len;

    while (length && !_empty)
    {
        _r_ptr = (_r_ptr + 1)%_capacity;
        length--;
        if (_r_ptr == _w_ptr)
        {
            _empty = true;
        }
    }

    _r_tot += len - length;

    return;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t length = len;
    std::string buf;

    while (length && !_empty)
    {
        buf += _stream[_r_ptr];
        _r_ptr = (_r_ptr + 1)%_capacity;
        length--;
        if (_r_ptr == _w_ptr)
        {
            _empty = true;
        }
    }

    _r_tot += len - length;

    return buf;
}

void ByteStream::end_input() {
    _input_ended = true;
    return;
}

// TODO: ??? not sure
bool ByteStream::input_ended() const {
    return _input_ended;
}

size_t ByteStream::buffer_size() const {
    // if w > r
    if (_w_ptr > _r_ptr)
    {
        return (_w_ptr - _r_ptr);
    }

    // if w <= r && !_empty
    if (_w_ptr <= _r_ptr && !_empty)
    {
        return (_w_ptr + _capacity - _r_ptr);
    }

    return 0;
}

bool ByteStream::buffer_empty() const {
    return _empty;
}

bool ByteStream::eof() const {
    return (_input_ended ? _empty : false);
}

size_t ByteStream::bytes_written() const {
    return _w_tot;
}

size_t ByteStream::bytes_read() const {
    return _r_tot;
}

size_t ByteStream::remaining_capacity() const {
    return (_capacity - buffer_size());
}
