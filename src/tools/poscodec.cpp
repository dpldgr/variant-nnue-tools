#include "poscodec.h"

#include "thread.h"

#include <iomanip>

using namespace std;

namespace Stockfish
{
    extern UCI::OptionsMap Options; // Global object
}

namespace Stockfish::Tools
{
    void PosCodec::flush()
    {
        for (size_t i = 0; i < file_buffer.size(); ++i)
        {
            flush(i);
        }
    }

    void PosCodec::flush(size_t thread_id)
    {
        std::unique_lock<std::mutex> lk(mutex);

        auto& buf = thread_buffers[thread_id];

        // There is a case that buf==nullptr, so that check is necessary.
        if (buf && buf->size() != 0)
        {
            file_buffer.emplace_back(std::move(buf));
        }
    }

    // Dedicated thread to write to file
    void PosCodec::file_write_worker()
    {
        while (!finished || file_buffer.size())
        {
            vector<unique_ptr<PosBuffer>> buffers;
            {
                std::unique_lock<std::mutex> lk(mutex);

                // Atomically swap take the filled buffers and
                // create a new buffer pool for threads to fill.
                buffers = std::move(file_buffer);
                file_buffer = std::vector<std::unique_ptr<PosBuffer>>();
            }

            if (!buffers.size())
            {
                // Poor man's condition variable.
                sleep(100);
            }
            else
            {
                for (auto& buf : buffers)
                {
                    file_stream.write(buf->data(), buf->size());

                    sfen_write_count += buf->size();

                    // Add the processed number here, and if it exceeds save_every,
                    // change the file name and reset this counter.
                    sfen_write_count_current_file += buf->size();

                    /* FIXME: Do we really need this?
                    if (sfen_write_count_current_file >= save_every)
                    {
                        sfen_write_count_current_file = 0;

                        // Sequential number attached to the file
                        int n = (int)(sfen_write_count / save_every);

                        // Rename the file and open it again.
                        // Add ios::app in consideration of overwriting.
                        // (Depending on the operation, it may not be necessary.)
                        std::string new_filename = filename + "_" + std::to_string(n);
                        file_stream = create_new_sfen_output(new_filename, sfen_format);

                        auto out = sync_region_cout.new_region();
                        out << "INFO (PosCodec): Creating new data file at " << new_filename << std::endl;
                    }
                    //*/
                }
            }
        }
    }

    void PlainCodec::buffer(PosBuffer& pb)
    {
        // TODO.
    }

    PosBuffer& PlainCodec::buffer()
    {
        // TODO.

        return *reinterpret_cast<PosBuffer*>(0);
    }

    PosBuffer* PlainCodec::copy()
    {
        // TODO.

        return reinterpret_cast<PosBuffer*>(0);
    }

    void PlainCodec::encode(const PosData& pos)
    {
        // TODO.
    }

    void PlainCodec::decode(PosData& pos)
    {
        // TODO.
    }

    void EpdCodec::buffer(PosBuffer& pb)
    {
        // TODO.
    }

    PosBuffer& EpdCodec::buffer()
    {
        // TODO.

        return *reinterpret_cast<PosBuffer*>(0);
    }

    PosBuffer* EpdCodec::copy()
    {
        // TODO.

        return reinterpret_cast<PosBuffer*>(0);
    }

    void EpdCodec::encode(const PosData& pos)
    {
        // TODO.
    }

    void EpdCodec::decode(PosData& pos)
    {
        // TODO.
    }

    void FenCodec::buffer(PosBuffer& pb)
    {
        // TODO.
    }

    PosBuffer& FenCodec::buffer()
    {
        // TODO.

        return *reinterpret_cast<PosBuffer*>(0);
    }

    PosBuffer* FenCodec::copy()
    {
        // TODO.

        return reinterpret_cast<PosBuffer*>(0);
    }

    void FenCodec::encode(const PosData& pos)
    {
        // TODO.
    }

    void FenCodec::decode(PosData& pos)
    {
        // TODO.
    }

    void Bin2Codec::buffer(PosBuffer& pb)
    {
        std::copy(pb.data(), pb.data() + pb.size(), m_data.data());
        m_data.size(pb.size());
    }

    PosBuffer& Bin2Codec::buffer()
    {
        return m_data;
    }

    PosBuffer* Bin2Codec::copy()
    {
        return m_data.copy();
    }

    void Bin2Codec::encode(const PosData& pd)
    {
        const Position& pos = pd.pos;
        const Square max_sq = pos.to_variant_square(pos.max_square());
        PieceCode::calc_code_size(pos.piece_types_count());

        m_data.clear();
        m_stream.reset();

        // Encodes both side to move and game ply.
        const int ply_count = pos.ply_from_start();
        m_stream.write_n_bit(ply_count, 16);

        // Write board occupancy.
        for (Square i = SQ_A1; i <= max_sq; ++i)
        {
            Square sq = pos.from_variant_square(i);
            Piece pc = pos.piece_on(sq);
            m_stream.write_one_bit(pc == NO_PIECE ? 0 : 1);
        }

        // Write piece codes.
        for (Square i = SQ_A1; i <= max_sq; ++i)
        {
            Square sq = pos.from_variant_square(i);
            Piece pc = pos.piece_on(sq);
            PieceCode pcc = pc;

            if (pcc.is_piece())
            {
                m_stream.write_n_bit(pcc.code(), pcc.bits());
            }
        }

        // Write out pieces in hand only if drops are enabled?
        if (pos.variant()->freeDrops == true)
        {
            for (auto c : Colors)
                for (PieceSet ps = pos.piece_types(); ps;)
                    m_stream.write_n_bit(pos.count_in_hand(c, pop_lsb(ps)), 7);
        }

        m_stream.write_n_bit(pos.rule50_count(), 8);

//#ifdef true // { FIXME: Ignoring castling and en passant for now.
        m_stream.write_one_bit(pos.can_castle(WHITE_OO));
        m_stream.write_one_bit(pos.can_castle(WHITE_OOO));
        m_stream.write_one_bit(pos.can_castle(BLACK_OO));
        m_stream.write_one_bit(pos.can_castle(BLACK_OOO));

        if (!pos.ep_squares()) {
            m_stream.write_one_bit(0);
        }
        else {
            m_stream.write_one_bit(1);
            // Additional ep squares (e.g., for berolina) are not encoded
            m_stream.write_n_bit(static_cast<int>(pos.to_variant_square(lsb(pos.ep_squares()))), 7);
        }
//#endif // } FIXME: Ignoring castling and en passant for now.

        m_stream.write_n_bit(pd.score,16);
        m_stream.write_n_bit(pd.move,16);
        m_stream.write_n_bit(pd.game_result,8);

        assert(m_stream.get_cursor() <= BIN2_DATA_SIZE);

        m_data.size(m_stream.size_bytes());
    }

    void Bin2Codec::decode(PosData& pd)
    {
        Position& pos = pd.pos;
        const Variant* v = variants.find(Options["UCI_Variant"])->second;
        StateInfo si;
        const Square max_sq = (Square)63;
        PieceCode board[max_sq + 1];
        //Piece board_pc[max_sq+1];
        PosCodecHelper hlp(&pos, &si, v);

        m_stream.set_data(m_data.data());

        pd.score = m_stream.read_n_bit(16);
        pd.move = m_stream.read_n_bit(16);
        pd.game_ply = m_stream.read_n_bit(16);
        pd.game_result = m_stream.read_n_bit(8);

        const int ply_count = m_stream.read_n_bit(16);
        pd.game_ply = ply_count;
        hlp.ply_from_start(ply_count);

        // Read board occupancy.
        for (Square i = SQ_A1; i <= max_sq; ++i)
        {
            bool build = (bool)m_stream.read_one_bit();
            board[i].build_piece(build);
        }

        // Read piece codes.
        for (Square i = SQ_A1; i <= max_sq; ++i)
        {
            if (board[i].is_piece())
            {
                int code = m_stream.read_n_bit(PieceCode::code_size);
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

        hlp.n_move_rule(m_stream.read_n_bit(8));

//#ifdef false // { FIXME: Ignoring castling and en passant for now.
        // Castling availability.
        if (m_stream.read_one_bit()) { hlp.set_castle(WHITE_OO); }
        if (m_stream.read_one_bit()) { hlp.set_castle(WHITE_OOO); }
        if (m_stream.read_one_bit()) { hlp.set_castle(BLACK_OO); }
        if (m_stream.read_one_bit()) { hlp.set_castle(BLACK_OOO); }

        // En passant square.
        // TODO: fix this so an arbitrary number of ep squares are possible?
        if (m_stream.read_one_bit()) {
            hlp.set_ep_squares(static_cast<Square>(m_stream.read_n_bit(7)));
        }
//#endif // } FIXME: Ignoring castling and en passant for now.
    }

    void JpnCodec::buffer(PosBuffer& pb)
    {
        // TODO.
    }

    PosBuffer& JpnCodec::buffer()
    {
        return m_data;
    }

    PosBuffer* JpnCodec::copy()
    {
        return new JpnPosBuffer(m_str);
    }

    void JpnCodec::encode(const PosData& pd)
    {
        const Position& pos = pd.pos;
        m_data.clear();

        m_stream.str("");
        m_stream.clear();
        m_stream.seekp(0);

        m_stream << "{\"p\":["; // Start of position.

        // Encodes both side to move and game ply.
        int ply_count = pos.ply_from_start();
        ply_count = pd.game_ply;

        const Square max_sq = pos.to_variant_square(pos.max_square());
        PieceCode::calc_code_size(pos.piece_types_count());

        m_stream << "\n";

        // Write piece codes.
        for (Rank r = pos.max_rank(); r >= RANK_1; --r)
        {
            for (File f = FILE_A; f <= pos.max_file(); ++f)
            {
                Piece pc = pos.piece_on(make_square(f, r));
                m_stream << "\"" << std::hex << std::setw(2) << std::setfill('0') << pc << "\"" << (r == RANK_1 && f == pos.max_file() ? "" : ",");
            }
            m_stream << "\n";
        }

        m_stream << std::dec << "]"; // End of pieces.

        m_stream << ",\"m\":" << ply_count;

        if ( pos.rule50_count() != 0 )
            m_stream << ",\"n\":" << pos.rule50_count();

        // Write out pieces in hand only if drops are enabled?
        if (pos.variant()->freeDrops == true)
        {
            m_stream << ",\"d\":["; // Start of pieces in hand.

            for (auto c : Colors)
            {
                for (PieceSet ps = pos.piece_types(); ps;)
                {
                    m_stream << pos.count_in_hand(c, pop_lsb(ps));
                }
            }

            m_stream << "]"; // End of pieces in hand.
        }

        m_stream << ",\"sc\":" << pd.score << ",\"mv\":\"" << std::hex << std::setw(4) << std::setfill('0') << pd.move << std::dec << "\",\"r\":" << int(pd.game_result);

        m_stream << "}"; // End of position.

        m_str = m_stream.str();

#ifdef false // { FIXME: Ignoring castling and en passant for now.
        m_stream.write_one_bit(pos.can_castle(WHITE_OO));
        m_stream.write_one_bit(pos.can_castle(WHITE_OOO));
        m_stream.write_one_bit(pos.can_castle(BLACK_OO));
        m_stream.write_one_bit(pos.can_castle(BLACK_OOO));

        if (!pos.ep_squares()) {
            m_stream.write_one_bit(0);
        }
        else {
            m_stream.write_one_bit(1);
            // Additional ep squares (e.g., for berolina) are not encoded
            m_stream.write_n_bit(static_cast<int>(pos.to_variant_square(lsb(pos.ep_squares()))), 7);
        }
#endif // } FIXME: Ignoring castling and en passant for now.
    }

    void JpnCodec::decode(PosData& pd)
    {
        // TODO.
    }

    /*
    void BinCodec::operator=(PosBuffer& data)
    {
        // TODO.
    }

    BinCodec::operator PosBuffer& ()
    {
        return m_data;
    }
    */

    void BinCodec::buffer( PosBuffer& pb )
    {
        std::copy(pb.data(), pb.data() + pb.size(), m_data.data());
        m_data.size(pb.size());
    }

    PosBuffer& BinCodec::buffer()
    {
        return m_data;
    }

    PosBuffer* BinCodec::copy()
    {
        return m_data.copy();
    }

    void BinCodec::encode(const PosData& pd)
    {
        const Position& pos = pd.pos;
        m_data.clear();
        m_stream.reset();

        // turn
        // Side to move.
        m_stream.write_one_bit((int)(pos.side_to_move()));

        // 7-bit positions for leading and trailing balls
        // White king and black king, 6 bits for each.
        for (auto c : Colors)
            m_stream.write_n_bit(pos.nnue_king() ? pos.to_variant_square(pos.king_square(c)) : (pos.max_file() + 1) * (pos.max_rank() + 1), 7);

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
                m_stream.write_n_bit(pos.count_in_hand(c, pop_lsb(ps)), DATA_SIZE > 512 ? 7 : 5);

        // TODO(someone): Support chess960.
        m_stream.write_one_bit(pos.can_castle(WHITE_OO));
        m_stream.write_one_bit(pos.can_castle(WHITE_OOO));
        m_stream.write_one_bit(pos.can_castle(BLACK_OO));
        m_stream.write_one_bit(pos.can_castle(BLACK_OOO));

        if (!pos.ep_squares()) {
            m_stream.write_one_bit(0);
        }
        else {
            m_stream.write_one_bit(1);
            // Additional ep squares (e.g., for berolina) are not encoded
            m_stream.write_n_bit(static_cast<int>(pos.to_variant_square(lsb(pos.ep_squares()))), 7);
        }

        m_stream.write_n_bit(pos.state()->rule50, 6);

        const int fm = 1 + (pos.game_ply() - (pos.side_to_move() == BLACK)) / 2;
        m_stream.write_n_bit(fm, 8);

        // Write high bits of half move. This is a fix for the
        // limited range of half move counter.
        // This is backwards compatible.
        m_stream.write_n_bit(fm >> 8, 8);

        // Write the highest bit of rule50 at the end. This is a backwards
        // compatible fix for rule50 having only 6 bits stored.
        // This bit is just ignored by the old parsers.
        m_stream.write_n_bit(pos.state()->rule50 >> 6, 1);

        int old_cursor = m_stream.get_cursor();
        m_stream.set_cursor(512);
        m_stream.write_n_bit(pd.score,16);
        m_stream.write_n_bit(pd.move,16);
        m_stream.write_n_bit(pd.game_ply,16);
        m_stream.write_n_bit(pd.game_result,8);
        m_stream.set_cursor(old_cursor);

        m_data.size(m_stream.size_bytes());

        assert(m_stream.get_cursor() <= DATA_SIZE);
    }

    void BinCodec::decode(PosData& pd)
    {
        Position& pos = pd.pos;
        const Variant* v = variants.find(Options["UCI_Variant"])->second;
        StateInfo si;
        int max_file = v->maxFile + 1;
        int max_rank = v->maxRank + 1;
        int max_squares = max_file * max_rank;
        Square max_sq = (Square)(max_squares - 1);
        vector<PieceCode> board;
        //Piece board_pc[max_sq+1];
        PosCodecHelper hlp(&pos, &si, v);

        board.reserve(max_squares);

        hlp.set_thread(Threads.main());

        m_stream.reset();
        m_stream.set_data(m_data.data());

        // Active color
        int stm = m_stream.read_one_bit();
        hlp.side_to_move(stm);

        // Place king.
        for (auto c : Colors)
            pos.put_piece(make_piece(c, pos.nnue_king()), pos.from_variant_square(Square(m_stream.read_n_bit(7))));

        // Piece placement
        for (Rank r = pos.max_rank(); r >= RANK_1; --r)
        {
            for (File f = FILE_A; f <= pos.max_file(); ++f)
            {
                auto sq = make_square(f, r);

                // it seems there are already balls
                Piece pc;
                if (type_of(pos.piece_on(sq)) != pos.nnue_king())
                {
                    assert(pos.empty(sq));
                    pc = read_board_piece_from_stream(pos);
                }
                else
                {
                    //pc = pos.piece_on(sq);
                    // put_piece() will catch ASSERT unless you remove it all.
                    //hlp.clear_sq(sq);
                    //pos.board[sq] = NO_PIECE; // FIXME: do we need this as the pos has been cleared?
                    pc = NO_PIECE;
                }

                // There may be no pieces, so skip in that case.
                if (pc == NO_PIECE)
                    continue;

                pos.put_piece(Piece(pc), sq);
            }
        }

        //m_stream.read_n_bit(5); // FIXME: just read and ignore for now.
        //* FIXME: skipping castling and en passant for the moment. 
        // Castling availability.
        // TODO(someone): Support chess960.
        //pos.st->castlingRights = 0; // Don't need this?
        if (m_stream.read_one_bit()) {
            hlp.set_castle(WHITE_OO);

            /*
            Square rsq;
            for (rsq = relative_square(WHITE, SQ_H1); pos.piece_on(rsq) != W_ROOK; --rsq) {}
            pos.set_castling_right(WHITE, rsq);
            //*/
        }
        if (m_stream.read_one_bit()) {
            hlp.set_castle(WHITE_OOO);

            /*
            Square rsq;
            for (rsq = relative_square(WHITE, SQ_A1); pos.piece_on(rsq) != W_ROOK; ++rsq) {}
            pos.set_castling_right(WHITE, rsq);
            //*/
        }
        if (m_stream.read_one_bit()) {
            hlp.set_castle(BLACK_OO);

            /*
            Square rsq;
            for (rsq = relative_square(BLACK, SQ_H1); pos.piece_on(rsq) != B_ROOK; --rsq) {}
            pos.set_castling_right(BLACK, rsq);
            //*/
        }
        if (m_stream.read_one_bit()) {
            hlp.set_castle(BLACK_OOO);

            /*
            Square rsq;
            for (rsq = relative_square(BLACK, SQ_A1); pos.piece_on(rsq) != B_ROOK; ++rsq) {}
            pos.set_castling_right(BLACK, rsq);
            //*/
        }

        // En passant square.
        if (m_stream.read_one_bit()) {
            Square ep_square = static_cast<Square>(m_stream.read_n_bit(7));
            hlp.set_ep_squares(ep_square);
            //pos.st->epSquares = square_bb(ep_square);
        }
        else {
            //pos.st->epSquares = 0;
        }
        //*/

        int game_ply;
        int n_move;

        n_move = m_stream.read_n_bit(6);
        game_ply = m_stream.read_n_bit(8);
        game_ply |= m_stream.read_n_bit(8) << 8;
        n_move |= m_stream.read_n_bit(1) << 6;
        game_ply = std::max(2 * (game_ply - 1), 0) + (stm == BLACK);

        hlp.n_move_rule(n_move);
        hlp.ply_from_start(game_ply-1);

        int old_cursor = m_stream.get_cursor();
        m_stream.set_cursor(512);
        pd.score = m_stream.read_n_bit(16);
        pd.move = m_stream.read_n_bit(16);
        pd.game_ply = m_stream.read_n_bit(16);
        pd.game_result = m_stream.read_n_bit(8);
        m_stream.set_cursor(old_cursor);

        assert(m_stream.get_cursor() <= DATA_SIZE);

        hlp.set_thread(Threads.main());
        hlp.set_state();

        assert(pos.pos_is_ok());
    }

    // Output the board pieces to stream.
    void BinCodec::write_board_piece_to_stream(const Position& pos, Piece pc)
    {
        // piece type
        PieceType pr = PieceType(pc == NO_PIECE ? NO_PIECE_TYPE : pos.variant()->pieceIndex[type_of(pc)] + 1);
        auto c = huffman_table[pr];
        m_stream.write_n_bit(c.code, c.bits);

        if (pc == NO_PIECE)
            return;

        // first and second flag
        m_stream.write_one_bit(color_of(pc));
    }

    // Read one board piece from stream
    Piece BinCodec::read_board_piece_from_stream(const Position& pos)
    {
        PieceType pr = NO_PIECE_TYPE;
        int code = 0, bits = 0;
        while (true)
        {
            code |= m_stream.read_one_bit() << bits;
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
        Color c = (Color)m_stream.read_one_bit();

        for (PieceSet ps = pos.piece_types(); ps;)
        {
            PieceType pt = pop_lsb(ps);
            if (pos.variant()->pieceIndex[pt] + 1 == pr)
                return make_piece(c, pt);
        }
        assert(false);
        return NO_PIECE;
    }

    CodecRegister::CodecRegister()
    {
        register_codec(new BinCodec());
        register_codec(new Bin2Codec());
        register_codec(new JpnCodec());
        register_codec(new PlainCodec());
        register_codec(new EpdCodec());
        register_codec(new FenCodec());
    }

    void CodecRegister::register_codec(PosCodec* codec)
    {   
        codec_names.emplace(codec->name(), codec);
        codec_exts.emplace(codec->ext(), codec);
        codec_types.emplace(codec->type(), codec);
        m_codecs.emplace_back(codec);
    }

    PosCodec* CodecRegister::get_name(const string& name)
    {
        return codec_names[name];
        /*
        PosCodec* ret = nullptr;

        for (int i = 0; i < m_codecs.size(); i++)
        {
            if (m_codecs[i]->name() == name)
            {
                return m_codecs[i];
            }
        }

        return ret;
        //*/
    }

    PosCodec* CodecRegister::get_ext(const string& ext)
    {
        return codec_names[ext];
        /*
        PosCodec* ret = nullptr;

        for (int i = 0; i < m_codecs.size(); i++)
        {
            if (m_codecs[i]->ext() == ext)
            {
                return m_codecs[i];
            }
        }

        return ret;
        //*/
    }

    PosCodec* CodecRegister::get_type(const SfenOutputType& type)
    {
        return codec_types[type];
        /*
        PosCodec* ret = nullptr;

        for (int i = 0; i < m_codecs.size(); i++)
        {
            if (m_codecs[i]->type() == type)
            {
                return m_codecs[i];
            }
        }

        return ret;
        //*/
    }

    PosCodec* CodecRegister::get_path(const string& path)
    {
        //return codec_exts[get_extension(path)];

        //*
        PosCodec* ret = nullptr;

        for (int i = 0; i < m_codecs.size(); i++)
        {
            if (ends_with(path, m_codecs[i]->ext()))
            {
                return m_codecs[i];
            }
        }

        return ret;
        //*/
    }
}
