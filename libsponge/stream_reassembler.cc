#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
    _unreass_buffer(0),
    _bitmap(0),
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
        if (ptr < index)
        {
            if ((ptr - _first_unreass_index) >= _unreass_buffer.size()) // when no capacity in the queue
            {
                _unreass_buffer.push_back('\0');
                _bitmap.push_back(false);
            }
            ptr++;
        }
        else {
            if ((ptr - _first_unreass_index) >= _unreass_buffer.size())
            {
                _unreass_buffer.push_back(data[ptr - index]);
                _bitmap.push_back(true);
            }
            else
            {
                _unreass_buffer[ptr - _first_unreass_index] = data[ptr - index];
                _bitmap[ptr - _first_unreass_index] = true;
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
    while (_bitmap.size() && _bitmap.front())
    {
        tmp += _unreass_buffer.front();
        _unreass_buffer.pop_front();
        _bitmap.pop_front();
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

    for (auto it = _bitmap.begin(); it != _bitmap.end(); it++)
    {
        if (*it) count++;
    }

    return count;
}

bool StreamReassembler::empty() const {
    return _bitmap.size() ? false : true;
}
