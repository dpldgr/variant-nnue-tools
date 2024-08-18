#pragma once

#include "bitstream.h"
#include "piececode.h"
#include "position.h"
#include "sfen_stream.h"
#include "uci.h"

#include <vector>

using namespace std;

namespace Stockfish
{
    extern UCI::OptionsMap Options; // Global object
}

namespace Stockfish::Tools
{
    struct EncodedPosData
    {
        size_t s = 0;
        uint8_t* d = nullptr;
    };

    class PosCodec
    {
    public:
        virtual bool is_decoder() { return false; };
        virtual bool is_encoder() { return false; };

        virtual uint8_t* data() = 0;
        virtual size_t size() = 0;
        virtual size_t max_size() = 0;

        virtual void operator=(const vector<uint8_t>& data) = 0;
        virtual operator vector<uint8_t>() const = 0;
      
        virtual void encode(const Position& pos) = 0;
        virtual void decode(Position& pos) = 0;
        
        virtual std::string name() const = 0;
        virtual std::string ext() const = 0;
    };

    class PlainCodec : public PosCodec
    {
    public:
        virtual uint8_t* data() { return nullptr; }
        virtual size_t size() { return 0; }
        virtual size_t max_size() { return 0; }
        virtual bool is_decoder() { return true; }
        virtual bool is_encoder() { return true; }
        virtual std::string name() const { return "PLAIN"; }
        virtual std::string ext() const { return ".plain"; }

        virtual void operator=(const vector<uint8_t>& data)
        {
            // TODO.
        }

        virtual operator vector<uint8_t>() const
        {
            // TODO.

            return vector<uint8_t>();
        }

        virtual void encode(const Position& pos)
        {
            // TODO.
        }

        virtual void decode(Position& pos)
        {
            // TODO.
        }
    };

    class Bin2Codec : public PosCodec
    {
    private:
        BitStream stream;
        uint8_t _data[BIN2_DATA_SIZE];
    public:
        virtual uint8_t* data() { return _data; }
        virtual size_t size() { return ceil(stream.get_cursor()/8.0); }
        virtual size_t max_size() { return ceil(BIN2_DATA_SIZE/8.0); }
        virtual bool is_decoder() { return true; }
        virtual bool is_encoder() { return true; }
        virtual std::string name() const { return "BIN2"; }
        virtual std::string ext() const { return ".bin2"; }

        virtual void operator=(const vector<uint8_t>& d)
        {
            std::copy(d.begin(), d.end(), _data);
        }

        virtual operator vector<uint8_t>() const
        {
            return vector<uint8_t>( _data, _data + (int)ceil(stream.get_cursor() / 8.0));
        }

        virtual void encode(const Position& pos)
        {
            const Square max_sq = pos.to_variant_square(pos.max_square());
            PieceCode::calc_code_size(pos.piece_types_count());

            // TODO: change to std::vector.
            memset(_data, 0, BIN2_DATA_SIZE / 8 /* 2048 bits */);
            stream.set_data(_data);

            // Encodes both side to move and game ply.
            const int ply_count = pos.ply_from_start();
            stream.write_n_bit(ply_count, 16);

            /* TODO: decide whether or not to leave this out and decode location in trainer.
            // 7-bit positions for leading and trailing balls
            // White king and black king, 6 bits for each.
            for (auto c : Colors)
                stream.write_n_bit(pos.nnue_king() ? to_variant_square(pos.king_square(c), pos) : (pos.max_file() + 1) * (pos.max_rank() + 1), 7);
            //*/

            // Write board occupancy.
            for (Square i = SQ_A1; i <= max_sq; ++i)
            {
                Square sq = pos.from_variant_square(i);
                Piece pc = pos.piece_on(sq);
                stream.write_one_bit(pc == NO_PIECE ? 0 : 1);
            }

            // Write piece codes.
            for (Square i = SQ_A1; i <= max_sq; ++i)
            {
                Square sq = pos.from_variant_square(i);
                Piece pc = pos.piece_on(sq);
                PieceCode pcc = pc;

                if (pcc.is_piece())
                {
                    stream.write_n_bit(pcc.code(), pcc.bits());
                }
            }

            // Write out pieces in hand only if drops are enabled?
            if (pos.variant()->freeDrops == true)
            {
                for (auto c : Colors)
                    for (PieceSet ps = pos.piece_types(); ps;)
                        stream.write_n_bit(pos.count_in_hand(c, pop_lsb(ps)), 7);
            }

            stream.write_n_bit(pos.rule50_count(), 8);

            /* FIXME: Ignoring castling and en passant for now.
            stream.write_one_bit(pos.can_castle(WHITE_OO));
            stream.write_one_bit(pos.can_castle(WHITE_OOO));
            stream.write_one_bit(pos.can_castle(BLACK_OO));
            stream.write_one_bit(pos.can_castle(BLACK_OOO));

            if (!pos.ep_squares()) {
                stream.write_one_bit(0);
            }
            else {
                stream.write_one_bit(1);
                // Additional ep squares (e.g., for berolina) are not encoded
                stream.write_n_bit(static_cast<int>(pos.to_variant_square(lsb(pos.ep_squares()))), 7);
            }
            //*/

            assert(stream.get_cursor() <= BIN2_DATA_SIZE);
        }

        virtual void decode(Position& pos)
        {
            const Variant* v = variants.find(Options["UCI_Variant"])->second;
            StateInfo si;
            const Square max_sq = (Square)63;
            PieceCode board[max_sq + 1];
            //Piece board_pc[max_sq+1];
            PosCodecHelper hlp(&pos, &si, v);

            stream.set_data(_data);
            const int ply_count = stream.read_n_bit(16);

            // Read board occupancy.
            for (Square i = SQ_A1; i <= max_sq; ++i)
            {
                bool build = (bool)stream.read_one_bit();
                board[i].build_piece(build);
            }

            // Read piece codes.
            for (Square i = SQ_A1; i <= max_sq; ++i)
            {
                if (board[i].is_piece())
                {
                    int code = stream.read_n_bit(PieceCode::code_size);
                    board[i].build_piece(code);
                    pos.put_piece(board[i], i);
                    //board_pc[i] = board[i];
                    //pos.put_piece(board_pc[i], i);
                }
                /*
                else
                {
                    board_pc[i] = NO_PIECE;
                }
                //*/
            }

            hlp.n_move_rule(stream.read_n_bit(8));

            /* FIXME: Ignoring castling and en passant for now.
            // Castling availability.
            if (stream.read_one_bit()) { hlp.set_castle(WHITE_OO);}
            if (stream.read_one_bit()) { hlp.set_castle(WHITE_OOO); }
            if (stream.read_one_bit()) { hlp.set_castle(BLACK_OO); }
            if (stream.read_one_bit()) { hlp.set_castle(BLACK_OOO); }

            // En passant square.
            // TODO: fix this so an arbitrary number of ep squares are possible?
            if (stream.read_one_bit()) {
                hlp.set_ep_squares(static_cast<Square>(stream.read_n_bit(7)));
            }
            //*/
        }
    };

    class BinCodec : public PosCodec
    {
    private:
        BitStream stream;
        std::array<uint8_t, DATA_SIZE / 8 + 8> _data;
    public:
        virtual uint8_t* data() { return _data.data(); }
        virtual size_t size() { return _data.size(); }
        virtual size_t max_size() { return (DATA_SIZE / 8 + 8); }
        virtual bool is_decoder() { return true; }
        virtual bool is_encoder() { return true; }
        virtual std::string name() const { return "BIN"; }
        virtual std::string ext() const { return ".bin"; }

        virtual void operator=(const vector<uint8_t>& data)
        {
            // TODO.
        }

        virtual operator vector<uint8_t>() const
        {
            vector<uint8_t> ret;

            ret.insert(ret.end(), _data.begin(), _data.end());

            return ret;
        }

        virtual void encode(const Position& pos)
        {
            //std::memset(data, 0, DATA_SIZE / 8 /* 512bit */);
            stream.set_data(_data.data());

            // turn
            // Side to move.
            stream.write_one_bit((int)(pos.side_to_move()));

            // 7-bit positions for leading and trailing balls
            // White king and black king, 6 bits for each.
            for (auto c : Colors)
                stream.write_n_bit(pos.nnue_king() ? pos.to_variant_square(pos.king_square(c)) : (pos.max_file() + 1) * (pos.max_rank() + 1), 7);

            // Write the pieces on the board other than the kings.
            for (Rank r = pos.max_rank(); r >= RANK_1; --r)
            {
                for (File f = FILE_A; f <= pos.max_file(); ++f)
                {
                    Piece pc = pos.piece_on(make_square(f, r));
                    if (pos.nnue_king() && type_of(pc) == pos.nnue_king())
                        continue;
                    write_board_piece_to_stream(pos, pc);
                }
            }

            for (auto c : Colors)
                for (PieceSet ps = pos.piece_types(); ps;)
                    stream.write_n_bit(pos.count_in_hand(c, pop_lsb(ps)), DATA_SIZE > 512 ? 7 : 5);

            // TODO(someone): Support chess960.
            stream.write_one_bit(pos.can_castle(WHITE_OO));
            stream.write_one_bit(pos.can_castle(WHITE_OOO));
            stream.write_one_bit(pos.can_castle(BLACK_OO));
            stream.write_one_bit(pos.can_castle(BLACK_OOO));

            if (!pos.ep_squares()) {
                stream.write_one_bit(0);
            }
            else {
                stream.write_one_bit(1);
                // Additional ep squares (e.g., for berolina) are not encoded
                stream.write_n_bit(static_cast<int>(pos.to_variant_square(lsb(pos.ep_squares()))), 7);
            }

            stream.write_n_bit(pos.state()->rule50, 6);

            const int fm = 1 + (pos.game_ply() - (pos.side_to_move() == BLACK)) / 2;
            stream.write_n_bit(fm, 8);

            // Write high bits of half move. This is a fix for the
            // limited range of half move counter.
            // This is backwards compatible.
            stream.write_n_bit(fm >> 8, 8);

            // Write the highest bit of rule50 at the end. This is a backwards
            // compatible fix for rule50 having only 6 bits stored.
            // This bit is just ignored by the old parsers.
            stream.write_n_bit(pos.state()->rule50 >> 6, 1);

            assert(stream.get_cursor() <= DATA_SIZE);
        }

        virtual void decode(Position& pos)
        {
            // TODO.
        }

    private:
        struct HuffmanedPiece
        {
            int code; // how it will be coded
            int bits; // How many bits do you have
        };

        const HuffmanedPiece huffman_table[17] =
        {
            {0b00000,1}, // NO_PIECE
            {0b00001,5}, // PAWN
            {0b00011,5}, // KNIGHT
            {0b00101,5}, // BISHOP
            {0b00111,5}, // ROOK
            {0b01001,5}, // QUEEN
            {0b01011,5}, //
            {0b01101,5}, //
            {0b01111,5}, //
            {0b10001,5}, //
            {0b10011,5}, //
            {0b10101,5}, //
            {0b10111,5}, //
            {0b11001,5}, //
            {0b11011,5}, //
            {0b11101,5}, //
            {0b11111,5}, //
        };

        // Output the board pieces to stream.
        void write_board_piece_to_stream(const Position& pos, Piece pc)
        {
            // piece type
            PieceType pr = PieceType(pc == NO_PIECE ? NO_PIECE_TYPE : pos.variant()->pieceIndex[type_of(pc)] + 1);
            auto c = huffman_table[pr];
            stream.write_n_bit(c.code, c.bits);

            if (pc == NO_PIECE)
                return;

            // first and second flag
            stream.write_one_bit(color_of(pc));
        }

        // Read one board piece from stream
        Piece read_board_piece_from_stream(const Position& pos)
        {
            PieceType pr = NO_PIECE_TYPE;
            int code = 0, bits = 0;
            while (true)
            {
                code |= stream.read_one_bit() << bits;
                ++bits;

                assert(bits <= 6);

                for (pr = NO_PIECE_TYPE; pr <= 16; ++pr)
                    if (huffman_table[pr].code == code
                        && huffman_table[pr].bits == bits)
                        goto Found;
            }
        Found:;
            if (pr == NO_PIECE_TYPE)
                return NO_PIECE;

            // first and second flag
            Color c = (Color)stream.read_one_bit();

            for (PieceSet ps = pos.piece_types(); ps;)
            {
                PieceType pt = pop_lsb(ps);
                if (pos.variant()->pieceIndex[pt] + 1 == pr)
                    return make_piece(c, pt);
            }
            assert(false);
            return NO_PIECE;
        }
    };

    extern const std::string plain_extension;
    extern const std::string bin_extension;
    extern const std::string binpack_extension;
    extern const std::string bin2_extension;

    static PosCodec* get_codec(const std::string& path)
    {
        if (ends_with(path, plain_extension))
            return new PlainCodec();
        else if (ends_with(path, bin_extension))
            return new BinCodec();
        else if (ends_with(path, bin2_extension))
            return new Bin2Codec();
        else
            return nullptr;
    }

    static PosCodec* get_codec_ext(const std::string& ext)
    {
        if (ext == plain_extension)
            return new PlainCodec();
        else if (ext == bin_extension)
            return new BinCodec();
        else if (ext == bin2_extension)
            return new Bin2Codec();
        else
            return nullptr;
    }

    static PosCodec* get_codec_type(const SfenOutputType type)
    {
        if (type == SfenOutputType::Bin)
            return new BinCodec();
        else if (type == SfenOutputType::Bin2)
            return new Bin2Codec();
        else
            return nullptr;
    }


    extern SfenOutputType data_format;

    extern PosCodec* pos_codec;
}
