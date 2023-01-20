#include "stream_reassembler.hh"
#include "log.h"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
    _unreass_buffer(0),
    _first_unreass_index(0),
    _unreass_cap(0),
    _output(capacity),
    _capacity(capacity),
    _eof(false),
    _last_byte_index(-1) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof)
    {
        _last_byte_index = index + data.length() - 1;
        _eof = true;
    }

    /* update unreassemble buffer size according to reassembled byte stream */
    _unreass_cap = _capacity - _output.buffer_size();

    /* write to unreassemble buffer */
    size_t len = data.length();
    size_t end_string_index = min(index + len, _first_unreass_index + _unreass_cap);    // end index + 1
    size_t ptr = _first_unreass_index;
    while (ptr < end_string_index)
    {
        if (ptr < index)    // when first byte of data is not the first unreass index
        {
            // when the capaity in the queue is not enought, add one
            if ((ptr - _first_unreass_index) >= _unreass_buffer.size())
            {
                _unreass_buffer.push_back(std::make_pair('\0', false));
            }
            ptr++;
        }
        else {      // when first byte of data is just right the first unreass index
            // when the capaity in the queue is not enought, add one
            if ((ptr - _first_unreass_index) >= _unreass_buffer.size())
            {
                _unreass_buffer.push_back(std::make_pair(data[ptr - index], true));
            }
            else    // when the position is previous mark to false, modify it
            {
                _unreass_buffer[ptr - _first_unreass_index].first = data[ptr - index];
                _unreass_buffer[ptr - _first_unreass_index].second = true;
            }
            ptr++;
        }
    }

    /* if there are contiguous bytes in the buffer, write the to byte stream */
    check_contiguous();

    return;
}

void StreamReassembler::check_contiguous(){
    std::string tmp = "";
    while (_unreass_buffer.size() && _unreass_buffer.front().second)
    {
        tmp += _unreass_buffer.front().first;
        _unreass_buffer.pop_front();
        _first_unreass_index++;
    }

    if (tmp.length())
        _output.write(tmp);

    /* if last byte is written to the bytestream, signal end_input */
    if (_eof && _last_byte_index < static_cast<int>(_first_unreass_index))
        _output.end_input();

    return;
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t count = 0;

    for (auto it = _unreass_buffer.begin(); it != _unreass_buffer.end(); it++)
        if ((*it).second) count++;

    return count;
}

bool StreamReassembler::empty() const {
    return _unreass_buffer.size() ? false : true;
}
