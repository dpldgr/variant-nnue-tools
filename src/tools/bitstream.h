#pragma once

#include <cstdint>
#include <cmath>

namespace Stockfish::Tools {

    // Class that handles bitstream
    // useful when doing aspect encoding
    struct BitStream
    {
        // Set the memory to store the data in advance.
        // Assume that memory is cleared to 0.
        void set_data(std::uint8_t* data_) { data = data_; reset(); }

        // Get the pointer passed in set_data().
        uint8_t* get_data() const { return data; }

        // Get the cursor.
        int get_cursor() const { return bit_cursor; }

        // Set the cursor.
        void set_cursor( int bit_cursor_ ) { bit_cursor = bit_cursor_; }

        // Get the data size in bytes.
        int size_bytes() const { return ceil(bit_cursor / 8.0); }

        // reset the cursor
        void reset() { bit_cursor = 0; }

        // Write 1bit to the stream.
        // If b is non-zero, write out 1. If 0, write 0.
        void write_one_bit(int b)
        {
            if (b)
                data[bit_cursor / 8] |= 1 << (bit_cursor & 7);

            ++bit_cursor;
        }

        // Get 1 bit from the stream.
        int read_one_bit()
        {
            int b = (data[bit_cursor / 8] >> (bit_cursor & 7)) & 1;
            ++bit_cursor;

            return b;
        }

        // write n bits of data
        // Data shall be written out from the lower order of d.
        void write_n_bit(int d, int n)
        {
            for (int i = 0; i < n; ++i)
                write_one_bit(d & (1 << i));
        }

        // read n bits of data
        // Reverse conversion of write_n_bit().
        int read_n_bit(int n)
        {
            int result = 0;
            for (int i = 0; i < n; ++i)
                result |= read_one_bit() ? (1 << i) : 0;

            return result;
        }

        template<typename T>
        T read()
        {
            return T(read_n_bit(sizeof(T)));
        }

        template<typename T>
        void write( T t )
        {
            write_n_bit(t, sizeof(T));
        }

    private:
        // Next bit position to read/write.
        int bit_cursor;

        // data entity
        std::uint8_t* data;
    };
}
