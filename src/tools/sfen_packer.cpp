#include "sfen_packer.h"

#include "packed_sfen.h"
#include "bitstream.h"
#include "piececode.h"
#include "sfen_stream.h"

#include "misc.h"
#include "position.h"

#include "uci.h"

#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstring> // std::memset()

using namespace std;

namespace Stockfish::Tools {

    int PieceCode::code_size = PIECE_TYPE_BITS + 1;

    struct PosPacker {};

    struct Bin2Packer : public PosPacker {};

    // Class for compressing/decompressing sfen
    // sfen can be packed to 256bit (32bytes) by Huffman coding.
    // This is proven by mini. The above is Huffman coding.
    //
    // Internal format = 1-bit turn + 7-bit king position *2 + piece on board (Huffman coding) + hand piece (Huffman coding)
    // Side to move (White = 0, Black = 1) (1bit)
    // White King Position (6 bits)
    // Black King Position (6 bits)
    // Huffman Encoding of the board
    // Castling availability (1 bit x 4)
    // En passant square (1 or 1 + 6 bits)
    // Rule 50 (6 bits)
    // Game play (8 bits)
    //
    // TODO(someone): Rename SFEN to FEN.
    //
    struct SfenPacker : public PosPacker
    {
        void pack(const Position& pos);

        // sfen packed by pack() (256bit = 32bytes)
        // Or sfen to decode with unpack()
        uint8_t *data; // uint8_t[32];

        BitStream stream;

        // Output the board pieces to stream.
        void write_board_piece_to_stream(const Position& pos, Piece pc);

        // Read one board piece from stream
        Piece read_board_piece_from_stream(const Position& pos);

        // Bin2 related.
        uint8_t* data_v2;
        void pack_v2(const Position& pos);
        void unpack_v2(const Position& pos);
        PieceCode read_piece_code_from_stream(const Position& pos);
    };


    // Huffman coding
    // * is simplified from mini encoding to make conversion easier.
    //
    // Huffman Encoding
    //
    // Empty  xxxxxxx0
    // Pawn   xxxxx001 + 1 bit (Color)
    // Knight xxxxx011 + 1 bit (Color)
    // Bishop xxxxx101 + 1 bit (Color)
    // Rook   xxxxx111 + 1 bit (Color)
    // Queen   xxxx1001 + 1 bit (Color)
    //
    // Worst case:
    // - 80 empty squares    80 bits
    // - 40 pieces           240 bits
    // - 20 pockets          100 bits
    // - 2 kings             14 bits
    // - castling rights     4 bits
    // - ep square           8 bits
    // - rule50              7 bits
    // - game ply            16 bits
    // - TOTAL               469 bits < 512 bits

    struct HuffmanedPiece
    {
        int code; // how it will be coded
        int bits; // How many bits do you have
    };

    constexpr HuffmanedPiece huffman_table[] =
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

    /* Moved to class Position,
    inline Square to_variant_square(Square s, const Position& pos) {
        return Square(s - rank_of(s) * (FILE_MAX - pos.max_file()));
    }

    inline Square from_variant_square(Square s, const Position& pos) {
        return Square(s + s / pos.files() * (FILE_MAX - pos.max_file()));
    }
    //*/

    // Pack sfen and store in data[64].
    void SfenPacker::pack(const Position& pos)
    {
        memset(data, 0, DATA_SIZE / 8 /* 512bit */);
        stream.set_data(data);

        // turn
        // Side to move.
        stream.write_one_bit((int)(pos.side_to_move()));

        // 7-bit positions for leading and trailing balls
        // White king and black king, 6 bits for each.
        for(auto c: Colors)
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

        for(auto c: Colors)
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

        const int fm = 1 + (pos.game_ply()-(pos.side_to_move() == BLACK)) / 2;
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

    // Output the board pieces to stream.
    void SfenPacker::write_board_piece_to_stream(const Position& pos, Piece pc)
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
    Piece SfenPacker::read_board_piece_from_stream(const Position& pos)
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

        for(PieceSet ps = pos.piece_types(); ps;)
        {
            PieceType pt = pop_lsb(ps);
            if (pos.variant()->pieceIndex[pt] + 1 == pr)
                return make_piece(c, pt);
        }
        assert(false);
        return NO_PIECE;
    }

    void SfenPacker::pack_v2(const Position& pos)
    {
        const Square max_sq = pos.to_variant_square(pos.max_square());
        PieceCode::calc_code_size(pos.piece_types_count());

        memset(data_v2, 0, DATA_SIZE / 8 /* 512 bits */);
        //memset(data_v2, 0, BIN2_DATA_SIZE / 8 /* 2048 bits */);
        stream.set_data(data_v2);

        // Encodes both side to move and game ply.
        const int ply_count = pos.ply_from_start();
        stream.write_n_bit(ply_count, 16);

        /* TODO: decide whether or not to leave this out and decode location in trainer.
        // 7-bit positions for leading and trailing balls
        // White king and black king, 6 bits for each.
        for (auto c : Colors)
            stream.write_n_bit(pos.nnue_king() ? to_variant_square(pos.king_square(c), pos) : (pos.max_file() + 1) * (pos.max_rank() + 1), 7);
        //*/

        //cout << "\npc:[";

        for (Square i = SQ_A1; i <= max_sq; ++i)
        {
            Square sq = pos.from_variant_square(i);
            Piece pc = pos.piece_on(sq);
            //cout << setfill('0') << setw(2) << std::hex << pc;
            //if (i < max_sq) cout << ",";
        }

        //cout << "]\nocc:";

        // Write board occupancy.
        for (Square i = SQ_A1; i <= max_sq; ++i)
        {
            Square sq = pos.from_variant_square(i);
            Piece pc = pos.piece_on(sq);
            stream.write_one_bit( pc == NO_PIECE ? 0 : 1);
            //cout << (pc == NO_PIECE ? "0" : "1");
        }

        //cout << "\ncodes:[";

        // Write piece codes.
        for (Square i = SQ_A1; i <= max_sq; ++i)
        {
            Square sq = pos.from_variant_square(i);
            Piece pc = pos.piece_on(sq);
            PieceCode pcc = pc;

            if (pcc.is_piece())
            {
                stream.write_n_bit(pcc.code(), pcc.bits());
                //cout << setfill('0') << setw(2) << std::hex << pcc.code();
                //if ( i < max_sq ) cout << ",";
            }
        }

        //cout << "]\n";

        // Write out pieces in hand only if drops are enabled?
        if (pos.variant()->freeDrops == true)
        {
            for (auto c : Colors)
                for (PieceSet ps = pos.piece_types(); ps;)
                    stream.write_n_bit(pos.count_in_hand(c, pop_lsb(ps)), 7 );
        }


        int n_move_count = pos.rule50_count();

        //cout <<  std::dec << "ply:" << ply_count << "\nn_move:" << n_move_count << "\n";

        stream.write_n_bit(n_move_count, 8);

        /* FIXME: not using these for now. 
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

        assert(stream.get_cursor() <= DATA_SIZE);
        //assert(stream.get_cursor() <= BIN2_DATA_SIZE);
    }

    void SfenPacker::unpack_v2(const Position& pos)
    {

    }

    PieceCode SfenPacker::read_piece_code_from_stream(const Position& pos)
    {
        return PieceCode(stream.read_n_bit(PieceCode::code_size));
    }

    /* FIXME: should not be needed with new architecture.
    int set_from_packed_sfen(Position& pos, const PackedSfen& sfen, StateInfo* si, Thread* th)
    {
        SfenPacker packer;
        auto& stream = packer.stream;

        // TODO: separate streams for writing and reading. Here we actually have to
        // const_cast which is not safe in the long run.
        stream.set_data(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(&sfen)));

        pos.clear();
        std::memset(si, 0, sizeof(StateInfo));
        si->accumulator.computed[WHITE] = false;
        si->accumulator.computed[BLACK] = false;
        pos.st = si;
        pos.var = variants.find(Options["UCI_Variant"])->second;

        // Active color
        pos.sideToMove = (Color)stream.read_one_bit();

        // First the position of the ball
        for (auto c : Colors)
            pos.board[pos.from_variant_square(Square(stream.read_n_bit(7)))] = make_piece(c, pos.nnue_king());

        // Piece placement
        for (Rank r = pos.max_rank(); r >= RANK_1; --r)
        {
            for (File f = FILE_A; f <= pos.max_file(); ++f)
            {
                auto sq = make_square(f, r);

                // it seems there are already balls
                Piece pc;
                if (type_of(pos.board[sq]) != pos.nnue_king())
                {
                    assert(pos.board[sq] == NO_PIECE);
                    pc = packer.read_board_piece_from_stream(pos);
                }
                else
                {
                    pc = pos.board[sq];
                    // put_piece() will catch ASSERT unless you remove it all.
                    pos.board[sq] = NO_PIECE;
                }

                // There may be no pieces, so skip in that case.
                if (pc == NO_PIECE)
                    continue;

                pos.put_piece(Piece(pc), sq);

                if (stream.get_cursor()> 512)
                    return 1;
            }
        }

        // Castling availability.
        // TODO(someone): Support chess960.
        pos.st->castlingRights = 0;
        if (stream.read_one_bit()) {
            Square rsq;
            for (rsq = relative_square(WHITE, SQ_H1); pos.piece_on(rsq) != W_ROOK; --rsq) {}
            pos.set_castling_right(WHITE, rsq);
        }
        if (stream.read_one_bit()) {
            Square rsq;
            for (rsq = relative_square(WHITE, SQ_A1); pos.piece_on(rsq) != W_ROOK; ++rsq) {}
            pos.set_castling_right(WHITE, rsq);
        }
        if (stream.read_one_bit()) {
            Square rsq;
            for (rsq = relative_square(BLACK, SQ_H1); pos.piece_on(rsq) != B_ROOK; --rsq) {}
            pos.set_castling_right(BLACK, rsq);
        }
        if (stream.read_one_bit()) {
            Square rsq;
            for (rsq = relative_square(BLACK, SQ_A1); pos.piece_on(rsq) != B_ROOK; ++rsq) {}
            pos.set_castling_right(BLACK, rsq);
        }

        // En passant square.
        if (stream.read_one_bit()) {
            Square ep_square = static_cast<Square>(stream.read_n_bit(7));
            pos.st->epSquares = square_bb(ep_square);
        }
        else {
            pos.st->epSquares = 0;
        }

        // Halfmove clock
        pos.st->rule50 = stream.read_n_bit(6);

        // Fullmove number
        pos.gamePly = stream.read_n_bit(8);

        // Read the highest bit of rule50. This was added as a fix for rule50
        // counter having only 6 bits stored.
        // In older entries this will just be a zero bit.
        pos.gamePly |= stream.read_n_bit(8) << 8;

        // Read the highest bit of rule50. This was added as a fix for rule50
        // counter having only 6 bits stored.
        // In older entries this will just be a zero bit.
        pos.st->rule50 |= stream.read_n_bit(1) << 6;

        // Convert from fullmove starting from 1 to gamePly starting from 0,
        // handle also common incorrect FEN with fullmove = 0.
        pos.gamePly = std::max(2 * (pos.gamePly - 1), 0) + (pos.sideToMove == BLACK);

        assert(stream.get_cursor() <= DATA_SIZE);

        pos.chess960 = false;
        pos.thisThread = th;
        pos.set_state(pos.st);

        assert(pos.pos_is_ok());

        return 0;
    }
    //*/

    inline std::unique_ptr<PosPacker> create_new_pos_packer(SfenOutputType sfen_output_type)
    {
        switch (sfen_output_type)
        {
        case SfenOutputType::Bin:
            return std::make_unique<SfenPacker>();
        case SfenOutputType::Bin2:
            return std::make_unique<Bin2Packer>();
        }

        assert(false);
        return nullptr;
    }

    PackedPos sfen_pack(Position& pos, SfenOutputType sfen_format)
    {
        PackedPos ret;
        //BinPackedPos bin_pos;
        //Bin2PackedPos bin2_pos;
        BinPackedPosValue bin_pos;
        Bin2PackedPosValue bin2_pos;

        PackedPos* r2 = new Bin2PackedPos();

        SfenPacker sp;
        sp.data = bin_pos.stream_data.data;
        sp.data_v2 = bin2_pos.stream_data.data;

        if (sfen_format == SfenOutputType::Bin)
        {
            sp.pack(pos);
            ret.buffer = bin_pos.stream_data.data;
            ret.buffer_size = sizeof(bin_pos);
        }
        else if (sfen_format == SfenOutputType::Bin2)
        {
            sp.pack_v2(pos);
            ret.buffer = bin2_pos.stream_data.data;
            ret.buffer_size = sp.stream.get_cursor() / 8;
        }

        return ret;
    }
}
