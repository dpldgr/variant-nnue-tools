#include "convert.h"

#include "uci.h"
#include "misc.h"
#include "thread.h"
#include "position.h"
#include "variant.h"
#include "tt.h"

#include "extra/nnue_data_binpack_format.h"

#include "nnue/evaluate_nnue.h"

#include "syzygy/tbprobe.h"

#include "bitstream.h"
#include "piececode.h"

#include <sstream>
#include <fstream>
#include <unordered_set>
#include <iomanip>
#include <list>
#include <cmath>    // std::exp(),std::pow(),std::log()
#include <cstring>  // memcpy()
#include <memory>
#include <limits>
#include <optional>
#include <chrono>
#include <random>
#include <regex>
#include <filesystem>

using namespace std;
namespace sys = std::filesystem;

namespace Stockfish::Tools
{
    bool fen_is_ok(Position& pos, std::string input_fen) {
        std::string pos_fen = pos.fen();
        std::istringstream ss_input(input_fen);
        std::istringstream ss_pos(pos_fen);

        // example : "2r4r/4kpp1/nb1np3/p2p3p/B2P1BP1/PP6/4NPKP/2R1R3 w - h6 0 24"
        //       --> "2r4r/4kpp1/nb1np3/p2p3p/B2P1BP1/PP6/4NPKP/2R1R3"
        std::string str_input, str_pos;
        ss_input >> str_input;
        ss_pos >> str_pos;

        // Only compare "Piece placement field" between input_fen and pos.fen().
        return str_input == str_pos;
    }

    void convert_bin(
        const vector<string>& filenames,
        const string& output_file_name,
        const int ply_minimum,
        const int ply_maximum,
        const int interpolate_eval,
        const int src_score_min_value,
        const int src_score_max_value,
        const int dest_score_min_value,
        const int dest_score_max_value,
        const bool check_invalid_fen,
        const bool check_illegal_move)
    {
        std::cout << "check_invalid_fen=" << check_invalid_fen << std::endl;
        std::cout << "check_illegal_move=" << check_illegal_move << std::endl;

        std::fstream fs;
        uint64_t data_size = 0;
        uint64_t filtered_size = 0;
        uint64_t filtered_size_fen = 0;
        uint64_t filtered_size_move = 0;
        uint64_t filtered_size_ply = 0;
        auto th = Threads.main();
        auto& tpos = th->rootPos;
        // convert plain rag to packed sfenvalue for Yaneura king
        fs.open(output_file_name, ios::app | ios::binary);
        StateListPtr states;
        for (auto filename : filenames) {
            std::cout << "convert " << filename << " ... ";
            std::string line;
            ifstream ifs;
            ifs.open(filename);
            BinPackedPosValue p;
            data_size = 0;
            filtered_size = 0;
            filtered_size_fen = 0;
            filtered_size_move = 0;
            filtered_size_ply = 0;
            p.gamePly = 1; // Not included in apery format. Should be initialized
            bool ignore_flag_fen = false;
            bool ignore_flag_move = false;
            bool ignore_flag_ply = false;
            const Variant* v = variants.find(Options["UCI_Variant"])->second;
            while (std::getline(ifs, line)) {
                std::stringstream ss(line);
                std::string token;
                std::string value;
                ss >> token;
                if (token == "fen") {
                    states = StateListPtr(new std::deque<StateInfo>(1)); // Drop old and create a new one
                    std::string input_fen = line.substr(4);
                    tpos.set(v, input_fen, false, &states->back(), Threads.main());
                    if (check_invalid_fen && !fen_is_ok(tpos, input_fen)) {
                        ignore_flag_fen = true;
                        filtered_size_fen++;
                    }
                    else {
                        tpos.sfen_pack(p.sfen);
                    }
                }
                else if (token == "move") {
                    ss >> value;
                    Move move = UCI::to_move(tpos, value);
                    if (check_illegal_move && move == MOVE_NONE) {
                        ignore_flag_move = true;
                        filtered_size_move++;
                    }
                    else {
                        p.move = move;
                    }
                }
                else if (token == "score") {
                    double score;
                    ss >> score;
                    // Training Formula ?Issue #71 ?nodchip/Stockfish https://github.com/nodchip/Stockfish/issues/71
                    // Normalize to [0.0, 1.0].
                    score = (score - src_score_min_value) / (src_score_max_value - src_score_min_value);
                    // Scale to [dest_score_min_value, dest_score_max_value].
                    score = score * (dest_score_max_value - dest_score_min_value) + dest_score_min_value;
                    p.score = std::clamp((int32_t)std::round(score), -(int32_t)VALUE_MATE, (int32_t)VALUE_MATE);
                }
                else if (token == "ply") {
                    int temp;
                    ss >> temp;
                    if (temp < ply_minimum || temp > ply_maximum) {
                        ignore_flag_ply = true;
                        filtered_size_ply++;
                    }
                    p.gamePly = uint16_t(temp); // No cast here?
                    if (interpolate_eval != 0) {
                        p.score = min(3000, interpolate_eval * temp);
                    }
                }
                else if (token == "result") {
                    int temp;
                    ss >> temp;
                    p.game_result = int8_t(temp); // Do you need a cast here?
                    if (interpolate_eval) {
                        p.score = p.score * p.game_result;
                    }
                }
                else if (token == "e") {
                    if (!(ignore_flag_fen || ignore_flag_move || ignore_flag_ply)) {
                        fs.write((char*)&p, sizeof(BinPackedPosValue));
                        data_size += 1;
                        // debug
                        // std::cout<<tpos<<std::endl;
                        // std::cout<<p.score<<","<<int(p.gamePly)<<","<<int(p.game_result)<<std::endl;
                    }
                    else {
                        filtered_size++;
                    }
                    ignore_flag_fen = false;
                    ignore_flag_move = false;
                    ignore_flag_ply = false;
                }
            }
            std::cout << "done " << data_size << " parsed " << filtered_size << " is filtered"
                << " (invalid fen:" << filtered_size_fen << ", illegal move:" << filtered_size_move << ", invalid ply:" << filtered_size_ply << ")" << std::endl;
            ifs.close();
        }
        std::cout << "all done" << std::endl;
        fs.close();
    }

    static inline void ltrim(std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
            return !std::isspace(ch);
            }));
    }

    static inline void rtrim(std::string& s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
            return !std::isspace(ch);
            }).base(), s.end());
    }

    static inline void trim(std::string& s) {
        ltrim(s);
        rtrim(s);
    }

    int parse_game_result_from_pgn_extract(std::string result) {
        // White Win
        if (result == "\"1-0\"") {
            return 1;
        }
        // Black Win
        else if (result == "\"0-1\"") {
            return -1;
        }
        // Draw
        else {
            return 0;
        }
    }

    // 0.25 -->  0.25 * PawnValueEg
    // #-4  --> -mate_in(4)
    // #3   -->  mate_in(3)
    // -M4  --> -mate_in(4)
    // +M3  -->  mate_in(3)
    Value parse_score_from_pgn_extract(std::string eval, bool& success) {
        success = true;

        if (eval.substr(0, 1) == "#") {
            if (eval.substr(1, 1) == "-") {
                return -mate_in(stoi(eval.substr(2, eval.length() - 2)));
            }
            else {
                return mate_in(stoi(eval.substr(1, eval.length() - 1)));
            }
        }
        else if (eval.substr(0, 2) == "-M") {
            //std::cout << "eval=" << eval << std::endl;
            return -mate_in(stoi(eval.substr(2, eval.length() - 2)));
        }
        else if (eval.substr(0, 2) == "+M") {
            //std::cout << "eval=" << eval << std::endl;
            return mate_in(stoi(eval.substr(2, eval.length() - 2)));
        }
        else {
            char* endptr;
            double value = strtod(eval.c_str(), &endptr);

            if (*endptr != '\0') {
                success = false;
                return VALUE_ZERO;
            }
            else {
                return Value(value * static_cast<double>(PawnValueEg));
            }
        }
    }

    // for Debug
    //#define DEBUG_CONVERT_BIN_FROM_PGN_EXTRACT

    bool is_like_fen(std::string fen) {
        int count_space = std::count(fen.cbegin(), fen.cend(), ' ');
        int count_slash = std::count(fen.cbegin(), fen.cend(), '/');

#if defined(DEBUG_CONVERT_BIN_FROM_PGN_EXTRACT)
        //std::cout << "count_space=" << count_space << std::endl;
        //std::cout << "count_slash=" << count_slash << std::endl;
#endif

        return count_space == 5 && count_slash == 7;
    }

    void convert_bin_from_pgn_extract(
        const vector<string>& filenames,
        const string& output_file_name,
        const bool pgn_eval_side_to_move,
        const bool convert_no_eval_fens_as_score_zero)
    {
        std::cout << "pgn_eval_side_to_move=" << pgn_eval_side_to_move << std::endl;
        std::cout << "convert_no_eval_fens_as_score_zero=" << convert_no_eval_fens_as_score_zero << std::endl;

        auto th = Threads.main();
        auto& pos = th->rootPos;

        std::fstream ofs;
        ofs.open(output_file_name, ios::out | ios::binary);

        int game_count = 0;
        int fen_count = 0;

        for (auto filename : filenames) {
            std::cout << now_string() << " convert " << filename << std::endl;
            ifstream ifs;
            ifs.open(filename);

            int game_result = 0;

            std::string line;
            while (std::getline(ifs, line)) {

                if (line.empty()) {
                    continue;
                }

                else if (line.substr(0, 1) == "[") {
                    std::regex pattern_result(R"(\[Result (.+?)\])");
                    std::smatch match;

                    // example: [Result "1-0"]
                    if (std::regex_search(line, match, pattern_result)) {
                        game_result = parse_game_result_from_pgn_extract(match.str(1));
#if defined(DEBUG_CONVERT_BIN_FROM_PGN_EXTRACT)
                        std::cout << "game_result=" << game_result << std::endl;
#endif
                        game_count++;
                        if (game_count % 10000 == 0) {
                            std::cout << now_string() << " game_count=" << game_count << ", fen_count=" << fen_count << std::endl;
                        }
                    }

                    continue;
                }

                else {
                    int gamePly = 1;
                    auto itr = line.cbegin();

                    while (true) {
                        gamePly++;

                        BinPackedPosValue psv;
                        memset((char*)&psv, 0, sizeof(BinPackedPosValue));

                        // fen
                        {
                            bool fen_found = false;

                            while (!fen_found) {
                                std::regex pattern_bracket(R"(\{(.+?)\})");
                                std::smatch match;
                                if (!std::regex_search(itr, line.cend(), match, pattern_bracket)) {
                                    break;
                                }

                                itr += match.position(0) + match.length(0) - 1;
                                std::string str_fen = match.str(1);
                                trim(str_fen);

                                if (is_like_fen(str_fen)) {
                                    fen_found = true;

                                    StateInfo si;
                                    pos.set(pos.variant(), str_fen, false, &si, th);
                                    pos.sfen_pack(psv.sfen);
                                }

#if defined(DEBUG_CONVERT_BIN_FROM_PGN_EXTRACT)
                                std::cout << "str_fen=" << str_fen << std::endl;
                                std::cout << "fen_found=" << fen_found << std::endl;
#endif
                            }

                            if (!fen_found) {
                                break;
                            }
                        }

                        // move
                        {
                            std::regex pattern_move(R"(\}(.+?)\{)");
                            std::smatch match;
                            if (!std::regex_search(itr, line.cend(), match, pattern_move)) {
                                break;
                            }

                            itr += match.position(0) + match.length(0) - 1;
                            std::string str_move = match.str(1);
                            trim(str_move);
#if defined(DEBUG_CONVERT_BIN_FROM_PGN_EXTRACT)
                            std::cout << "str_move=" << str_move << std::endl;
#endif
                            psv.move = UCI::to_move(pos, str_move);
                        }

                        // eval
                        bool eval_found = false;
                        {
                            std::regex pattern_bracket(R"(\{(.+?)\})");
                            std::smatch match;
                            if (!std::regex_search(itr, line.cend(), match, pattern_bracket)) {
                                break;
                            }

                            std::string str_eval_clk = match.str(1);
                            trim(str_eval_clk);
#if defined(DEBUG_CONVERT_BIN_FROM_PGN_EXTRACT)
                            std::cout << "str_eval_clk=" << str_eval_clk << std::endl;
#endif

                            // example: { [%eval 0.25] [%clk 0:10:00] }
                            // example: { [%eval #-4] [%clk 0:10:00] }
                            // example: { [%eval #3] [%clk 0:10:00] }
                            // example: { +0.71/22 1.2s }
                            // example: { -M4/7 0.003s }
                            // example: { M3/245 0.017s }
                            // example: { +M1/245 0.010s, White mates }
                            // example: { 0.60 }
                            // example: { book }
                            // example: { rnbqkb1r/pp3ppp/2p1pn2/3p4/2PP4/2N2N2/PP2PPPP/R1BQKB1R w KQkq - 0 5 }

                            // Considering the absence of eval
                            if (!is_like_fen(str_eval_clk)) {
                                itr += match.position(0) + match.length(0) - 1;

                                if (str_eval_clk != "book") {
                                    std::regex pattern_eval1(R"(\[\%eval (.+?)\])");
                                    std::regex pattern_eval2(R"((.+?)\/)");

                                    std::string str_eval;
                                    if (std::regex_search(str_eval_clk, match, pattern_eval1) ||
                                        std::regex_search(str_eval_clk, match, pattern_eval2)) {
                                        str_eval = match.str(1);
                                        trim(str_eval);
                                    }
                                    else {
                                        str_eval = str_eval_clk;
                                    }

                                    bool success = false;
                                    Value value = parse_score_from_pgn_extract(str_eval, success);
                                    if (success) {
                                        eval_found = true;
                                        psv.score = std::clamp(value, -VALUE_MATE, VALUE_MATE);
                                    }

#if defined(DEBUG_CONVERT_BIN_FROM_PGN_EXTRACT)
                                    std::cout << "str_eval=" << str_eval << std::endl;
                                    std::cout << "success=" << success << ", psv.score=" << psv.score << std::endl;
#endif
                                }
                            }
                        }

                        // write
                        if (eval_found || convert_no_eval_fens_as_score_zero) {
                            if (!eval_found && convert_no_eval_fens_as_score_zero) {
                                psv.score = 0;
                            }

                            psv.gamePly = gamePly;
                            psv.game_result = game_result;

                            if (pos.side_to_move() == BLACK) {
                                if (!pgn_eval_side_to_move) {
                                    psv.score *= -1;
                                }
                                psv.game_result *= -1;
                            }

                            ofs.write((char*)&psv, sizeof(BinPackedPosValue));

                            fen_count++;
                        }
                    }

                    game_result = 0;
                }
            }
        }

        std::cout << now_string() << " game_count=" << game_count << ", fen_count=" << fen_count << std::endl;
        std::cout << now_string() << " all done" << std::endl;
        ofs.close();
    }

    void convert_plain(
        const vector<string>& filenames,
        const string& output_file_name)
    {
        Position tpos;
        std::ofstream ofs;
        ofs.open(output_file_name, ios::app);
        auto th = Threads.main();
        for (auto filename : filenames) {
            std::cout << "convert " << filename << " ... ";

            // Just convert packedsfenvalue to text
            std::fstream fs;
            fs.open(filename, ios::in | ios::binary);
            BinPackedPosValue p;
            while (true)
            {
                if (fs.read((char*)&p, sizeof(BinPackedPosValue))) {
                    StateInfo si;
                    Tools::set_from_packed_sfen(tpos, p.sfen, &si, th);

                    // write as plain text
                    ofs << "fen " << tpos.fen() << std::endl;
                    ofs << "move " << UCI::move(tpos, Move(p.move)) << std::endl;
                    ofs << "score " << p.score << std::endl;
                    ofs << "ply " << int(p.gamePly) << std::endl;
                    ofs << "result " << int(p.game_result) << std::endl;
                    ofs << "e" << std::endl;
                }
                else {
                    break;
                }
            }
            fs.close();
            std::cout << "done" << std::endl;
        }
        ofs.close();
        std::cout << "all done" << std::endl;
    }

    static inline const std::string plain_extension = ".plain";
    static inline const std::string bin_extension = ".bin";
    static inline const std::string binpack_extension = ".binpack";
    static inline const std::string bin2_extension = ".bin2";

    static bool file_exists(const std::string& name)
    {
        std::ifstream f(name);
        return f.good();
    }

    static bool is_convert_of_type(
        const std::string& input_path,
        const std::string& output_path,
        const std::string& expected_input_extension,
        const std::string& expected_output_extension)
    {
        return ends_with(input_path, expected_input_extension)
            && ends_with(output_path, expected_output_extension);
    }

    class PosCodec
    {
    public:
        virtual bool is_decoder() { return false; };
        virtual bool is_encoder() { return false; };

        virtual void operator=(const vector<uint8_t>& data) = 0;
        virtual operator vector<uint8_t>() const = 0;
        virtual void encode(const Position& pos) = 0;
        virtual void decode(Position& pos) = 0;
        virtual std::string name() const = 0;
        virtual std::string ext() const = 0;

        virtual void writeHeader() {};
        virtual void readHeader() {};
        virtual void writePosition() {};
        virtual void readPosition() {};
    };

    class PlainCodec : public PosCodec
    {
    private:
    public:
        virtual bool is_decoder() { return true; };
        virtual bool is_encoder() { return true; };
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
        uint8_t data[BIN2_DATA_SIZE];
    public:
        virtual bool is_decoder() { return true; };
        virtual bool is_encoder() { return true; };
        virtual std::string name() const { return "BIN2"; }
        virtual std::string ext() const { return ".bin2"; }

        virtual void operator=(const vector<uint8_t>& d)
        {
            std::copy(d.begin(), d.end(), data);
        }

        virtual operator vector<uint8_t>() const
        {
            // TODO.

            return vector<uint8_t>();
        }

        virtual void encode(const Position& pos)
        {
            const Square max_sq = pos.to_variant_square(pos.max_square());
            PieceCode::calc_code_size(pos.piece_types_count());

            // TODO: change to std::vector.
            memset(data, 0, BIN2_DATA_SIZE / 8 /* 2048 bits */);
            stream.set_data(data);

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
            PieceCode board[max_sq+1];
            //Piece board_pc[max_sq+1];
            PosCodecHelper hlp(&pos, &si, v);

            stream.set_data(data);
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

    class BinCodec: public PosCodec
    {
    private:
        BitStream stream;
        std::array<uint8_t, DATA_SIZE / 8 + 8> data;
        //uint8_t data[DATA_SIZE/8+8];
    public:
        virtual bool is_decoder() { return true; };
        virtual bool is_encoder() { return true; };
        virtual std::string name() const { return "BIN"; }
        virtual std::string ext() const { return ".bin"; }

        virtual void operator=(const vector<uint8_t>& data)
        {
            // TODO.
        }

        virtual operator vector<uint8_t>() const
        {
            vector<uint8_t> ret;

            ret.insert(ret.end(), data.begin(), data.end());

            return ret;
        }

        virtual void encode(const Position& pos)
        {
            //std::memset(data, 0, DATA_SIZE / 8 /* 512bit */);
            stream.set_data(data.data());

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

    inline void convert(PosCodec& format1, vector<uint8_t>& data1, PosCodec& format2, vector<uint8_t>& data2)
    {
        Position pos;

        format1 = data1;
        format1.decode(pos);
        format2.encode(pos);
        data2 = format2;
    }

    inline void convert(PosCodec& format_in, PosCodec& format_out, std::string inputPath, std::string outputPath, std::ios_base::openmode om, bool validate)
    {
        std::array<uint8_t, 256> buf;
        constexpr std::size_t bufferSize = 1024 * 1024;

        std::cout << "Converting from " << format_in.name() << " to " << format_out.name() << ".\n";
        std::cout << " Input file: " << inputPath << "\n";
        std::cout << "Output file: " << outputPath << "\n";
        std::cout << "WARNING: not fully implemented yet.\n";

        std::basic_ifstream<uint8_t> inputFile(inputPath, std::ios_base::binary);
        const auto base = inputFile.tellg();
        std::size_t numProcessedPositions = 0;

        std::basic_ofstream<uint8_t> outputFile(outputPath, om);
        std::string buffer;
        buffer.reserve(bufferSize * 2);

        Position pos;
        vector<uint8_t> data_in;
        vector<uint8_t> data_out;

        array<uint8_t,5> magic_ver = { 0xC2, 0x34, 0x56, 0x78, 0x20 };
        array<uint8_t,5> header{};

        struct variant_info
        {
            uint8_t ranks;
            uint8_t files;
            uint16_t squares;
            uint16_t piece_types;   
        } vi;

        if (!inputFile.read(header.data(), header.size()))
        {
            cout << "ERROR: couldn't open file.\n";
            return;
        }

        if ( equal( header.begin(), header.end(), magic_ver.begin() ) )
        {
            cout << "Matched file magic and version.\n";
        }
        else
        {
            cerr << "ERROR: Didn't match file magic and version.\n";
            return;
        }

        if (!inputFile.read(reinterpret_cast<uint8_t*>(&vi), sizeof(vi)))
        {
            cout << "ERROR: couldn't open file.\n";
            return;
        }

        for (;;)
        {
            uint16_t size = 0;

            if (!inputFile.read(reinterpret_cast<uint8_t*>(&size), sizeof(size)))
            {
                break;
            }

            if (!inputFile.read(buf.data(), size))
            {
                break;
            }
            else
            {
                const uint16_t POSITION_MAGIC = 0;
                const uint16_t magic = (size & 0xC000) >> 14;

                // Check magic bits match.
                if (magic == POSITION_MAGIC)
                {
                    //std::cout << "Matched position magic.\n";
                }
                else
                {
                    std::cout << "ERROR: POSITON_MAGIC should be 0 but it was " << magic << "\n";
                    break;
                }

                const int copy_size = size - 7;
                const Square max_sq = SQ_MAX; // TODO: get this from the file header.
                const int piece_type_count = 6; // TODO: get this from the file header.
                PieceCode board[SQ_MAX];
                PieceCode::calc_code_size(piece_type_count);

                data_in.clear();
                data_in.insert(data_in.end(), buf.begin() + 7, buf.begin() + size);

                if (copy_size <= (DATA_SIZE / 8))
                {
                    convert(format_in, data_in, format_out, data_out);
                }
                else
                {
                    std::cout << "BIN2 position is too large to convert. " << copy_size << " > " << (DATA_SIZE / 8) << '\n';
                }

                //std::cout << "BIN size: " << data_out.size() << '\n' << "BEFORE pos: " << outputFile.tellp() << '\n';
                outputFile.seekp(72*numProcessedPositions);
                //outputFile.write(reinterpret_cast<const char*>(data_out.data()), data_out.size());
                outputFile.write(data_out.data(), data_out.size());
                //std::cout << "AFTER pos: " << outputFile.tellp() << '\n';

                ++numProcessedPositions;
            }
        }

        outputFile.flush();
        outputFile.close();
        inputFile.close();

        std::cout << "Finished. Converted " << numProcessedPositions << " positions.\n";
    }

    using ConvertFunctionType = void(std::string inputPath, std::string outputPath, std::ios_base::openmode om, bool validate);

    static ConvertFunctionType* get_convert_function(const std::string& input_path, const std::string& output_path)
    {
        if (is_convert_of_type(input_path, output_path, plain_extension, bin_extension))
            return binpack::convertPlainToBin;
        if (is_convert_of_type(input_path, output_path, plain_extension, binpack_extension))
            return binpack::convertPlainToBinpack;

        if (is_convert_of_type(input_path, output_path, bin_extension, plain_extension))
            return binpack::convertBinToPlain;
        if (is_convert_of_type(input_path, output_path, bin_extension, binpack_extension))
            return binpack::convertBinToBinpack;

        if (is_convert_of_type(input_path, output_path, binpack_extension, plain_extension))
            return binpack::convertBinpackToPlain;
        if (is_convert_of_type(input_path, output_path, binpack_extension, bin_extension))
            return binpack::convertBinpackToBin;

        return nullptr;
    }

    static void convert(const std::string& input_path, const std::string& output_path, std::ios_base::openmode om, bool validate)
    {
        if (!file_exists(input_path))
        {
            std::cerr << "Input file does not exist.\n";
            return;
        }

        PosCodec* format_in = get_codec(input_path);
        PosCodec* format_out = get_codec(output_path);
        bool can_convert = true;

        // Each format has to be able to convert to/from a Position object, and input/output to their respective file format.
        if (format_in == nullptr)
        {
            std::cerr << "No matching format found for file: " << input_path << ".\n";
            can_convert = false;
        }
        else if (!format_in->is_decoder())
        {
            std::cerr << "Format " << format_in->name() << " cannot be used as an input format.\n";
            can_convert = false;
        }

        if (format_out == nullptr)
        {
            std::cerr << "No matching format found for file: " << output_path << ".\n"; 
            can_convert = false;
        }
        else if (!format_out->is_encoder())
        {
            std::cerr << "Format " << format_out->name() << " cannot be used as an output format.\n";
            can_convert = false;
        }

        if ( can_convert )
        {
            convert(*format_in, *format_out, input_path, output_path, om, validate);
        }
        else
        {
            std::cerr << "Conversion between these file formats is not supported.\n";
        }

        /* TODO: replace this.
        auto func = get_convert_function(input_path, output_path);
        if (func != nullptr)
        {
            func(input_path, output_path, om, validate);
        }
        else
        {
            std::cerr << "Conversion between files of these types is not supported.\n";
        }
        //*/
    }

    static void convert(const std::vector<std::string>& args)
    {
        if (args.size() < 2 || args.size() > 4)
        {
            std::cerr << "Invalid arguments.\n";
            std::cerr << "Usage: convert from_path to_path [append] [validate]\n";
            return;
        }

        const bool append = std::find(args.begin() + 2, args.end(), "append") != args.end();
        const bool validate = std::find(args.begin() + 2, args.end(), "validate") != args.end();

        const std::ios_base::openmode openmode =
            append
            ? std::ios_base::app
            : std::ios_base::trunc;

        convert(args[0], args[1], openmode, validate);
    }

    void convert(istringstream& is)
    {
        std::vector<std::string> args;

        while (true)
        {
            std::string token = "";
            is >> token;
            if (token == "")
                break;

            args.push_back(token);
        }

        convert(args);
    }

    static void append_files_from_dir(
        std::vector<std::string>& filenames,
        const std::string& base_dir,
        const std::string& target_dir)
    {
        string kif_base_dir = Path::combine(base_dir, target_dir);

        sys::path p(kif_base_dir); // Origin of enumeration
        std::for_each(sys::directory_iterator(p), sys::directory_iterator(),
            [&](const sys::path& path) {
                if (sys::is_regular_file(path))
                    filenames.push_back(Path::combine(target_dir, path.filename().generic_string()));
            });
    }

    static void rebase_files(
        std::vector<std::string>& filenames,
        const std::string& base_dir)
    {
        for (auto& file : filenames)
        {
            file = Path::combine(base_dir, file);
        }
    }

    void convert_bin_from_pgn_extract(std::istringstream& is)
    {
        std::vector<std::string> filenames;

        string base_dir;
        string target_dir;

        bool pgn_eval_side_to_move = false;
        bool convert_no_eval_fens_as_score_zero = false;

        string output_file_name = "shuffled_sfen.bin";

        while (true)
        {
            string option;
            is >> option;

            if (option == "")
                break;

            if (option == "targetdir") is >> target_dir;
            else if (option == "targetfile")
            {
                std::string filename;
                is >> filename;
                filenames.push_back(filename);
            }

            else if (option == "basedir")   is >> base_dir;

            else if (option == "pgn_eval_side_to_move") is >> pgn_eval_side_to_move;
            else if (option == "convert_no_eval_fens_as_score_zero") is >> convert_no_eval_fens_as_score_zero;
            else if (option == "output_file_name") is >> output_file_name;
            else
            {
                cout << "Unknown option: " << option << ". Ignoring.\n";
            }
        }

        if (!target_dir.empty())
        {
            append_files_from_dir(filenames, base_dir, target_dir);
        }
        rebase_files(filenames, base_dir);

        Eval::NNUE::init();

        cout << "convert_bin_from_pgn-extract.." << endl;
        convert_bin_from_pgn_extract(
            filenames,
            output_file_name,
            pgn_eval_side_to_move,
            convert_no_eval_fens_as_score_zero);
    }

    void convert_bin(std::istringstream& is)
    {
        std::vector<std::string> filenames;

        string base_dir;
        string target_dir;

        int ply_minimum = 0;
        int ply_maximum = 114514;
        bool interpolate_eval = 0;
        bool check_invalid_fen = false;
        bool check_illegal_move = false;

        bool pgn_eval_side_to_move = false;
        bool convert_no_eval_fens_as_score_zero = false;

        double src_score_min_value = 0.0;
        double src_score_max_value = 1.0;
        double dest_score_min_value = 0.0;
        double dest_score_max_value = 1.0;

        string output_file_name = "shuffled_sfen.bin";

        while (true)
        {
            string option;
            is >> option;

            if (option == "")
                break;

            if (option == "targetdir") is >> target_dir;
            else if (option == "targetfile")
            {
                std::string filename;
                is >> filename;
                filenames.push_back(filename);
            }

            else if (option == "basedir")   is >> base_dir;

            else if (option == "ply_minimum") is >> ply_minimum;
            else if (option == "ply_maximum") is >> ply_maximum;
            else if (option == "interpolate_eval") is >> interpolate_eval;
            else if (option == "check_invalid_fen") is >> check_invalid_fen;
            else if (option == "check_illegal_move") is >> check_illegal_move;
            else if (option == "pgn_eval_side_to_move") is >> pgn_eval_side_to_move;
            else if (option == "convert_no_eval_fens_as_score_zero") is >> convert_no_eval_fens_as_score_zero;
            else if (option == "src_score_min_value") is >> src_score_min_value;
            else if (option == "src_score_max_value") is >> src_score_max_value;
            else if (option == "dest_score_min_value") is >> dest_score_min_value;
            else if (option == "dest_score_max_value") is >> dest_score_max_value;
            else if (option == "output_file_name") is >> output_file_name;
            else
            {
                cout << "Unknown option: " << option << ". Ignoring.\n";
            }
        }

        if (!target_dir.empty())
        {
            append_files_from_dir(filenames, base_dir, target_dir);
        }
        rebase_files(filenames, base_dir);

        Eval::NNUE::init();

        cout << "convert_bin.." << endl;
            convert_bin(
                filenames,
                output_file_name,
                ply_minimum,
                ply_maximum,
                interpolate_eval,
                src_score_min_value,
                src_score_max_value,
                dest_score_min_value,
                dest_score_max_value,
                check_invalid_fen,
                check_illegal_move
            );
    }

    void convert_plain(std::istringstream& is)
    {
        std::vector<std::string> filenames;

        string base_dir;
        string target_dir;

        string output_file_name = "shuffled_sfen.bin";

        while (true)
        {
            string option;
            is >> option;

            if (option == "")
                break;

            if (option == "targetdir") is >> target_dir;
            else if (option == "targetfile")
            {
                std::string filename;
                is >> filename;
                filenames.push_back(filename);
            }

            else if (option == "basedir")   is >> base_dir;

            else if (option == "output_file_name") is >> output_file_name;
            else
            {
                cout << "Unknown option: " << option << ". Ignoring.\n";
            }
        }

        if (!target_dir.empty())
        {
            append_files_from_dir(filenames, base_dir, target_dir);
        }
        rebase_files(filenames, base_dir);

        Eval::NNUE::init();

        cout << "convert_plain.." << endl;
        convert_plain(filenames, output_file_name);
    }
}
