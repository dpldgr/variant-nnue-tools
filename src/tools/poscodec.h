#pragma once

#include "bitstream.h"
#include "piececode.h"
#include "posbuffer.h"
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

    struct PosData
    {
        PosData(Position& p) : pos(p) {};
        Position& pos;
        int16_t score;
        uint16_t move;
        uint16_t game_ply;
        int8_t game_result;
    };

    class PosCodec
    {
    protected:
        //PosData m_info;
        SfenOutputType m_type;
    public:
        //PosData& get_pos_info() { return m_info; }
        //void set_pos_info( const PosData& info) { m_info = info; }

        virtual bool is_decoder() { return false; }
        virtual bool is_encoder() { return false; }

        virtual uint8_t* data() = 0;
        virtual size_t size() = 0;
        virtual size_t max_size() = 0;

        virtual void buffer(PosBuffer& pb) = 0;
        virtual PosBuffer& buffer() = 0;
        virtual PosBuffer* copy() = 0;

        virtual void encode(const PosData& pos) = 0;
        virtual void decode(PosData& pos) = 0;
        
        virtual std::string name() const = 0;
        virtual std::string ext() const = 0;
        virtual SfenOutputType type() const = 0;

    public:
        thread file_worker_thread;
        atomic<bool> finished;
        string filename;
        vector<unique_ptr<PosBuffer>> thread_buffers;
        vector<unique_ptr<PosBuffer>> file_buffer;
        mutex file_mutex;
        basic_fstream<uint8_t> file_stream;
        bool m_eof;
        uint64_t sfen_write_count = 0;
        uint64_t sfen_write_count_current_file = 0;

    public:
        void flush();
        void flush(size_t thread_id);
        void file_write_worker();
    };

    class PlainCodec : public PosCodec
    {
    public:
        uint8_t* data() override { return nullptr; }
        size_t size() override { return 0; }
        size_t max_size() override { return 0; }
        bool is_decoder() override { return true; }
        bool is_encoder() override { return true; }
        std::string name() const override { return "PLAIN"; }
        std::string ext() const override { return ".plain"; }
        SfenOutputType type() const override { return SfenOutputType::Plain; }
        void buffer(PosBuffer& pb) override;
        PosBuffer& buffer() override;
        PosBuffer* copy() override;
        void encode(const PosData& pos) override;
        void decode(PosData& pos) override;
    };

    class EpdCodec : public PosCodec
    {
    public:
        uint8_t* data() override { return nullptr; }
        size_t size() override { return 0; }
        size_t max_size() override { return 0; }
        bool is_decoder() override { return true; }
        bool is_encoder() override { return true; }
        std::string name() const override { return "EPD"; }
        std::string ext() const override { return ".epd"; }
        SfenOutputType type() const override { return SfenOutputType::Epd; }
        void buffer(PosBuffer& pb) override;
        PosBuffer& buffer() override;
        PosBuffer* copy() override;
        void encode(const PosData& pos) override;
        void decode(PosData& pos) override;
    };

    class JpnCodec : public PosCodec
    {
    private:
        JpnPosBuffer m_data;
        ostringstream m_stream;
        string m_str;
    public:
        JpnCodec() : m_data(*new std::string("")) {}
        uint8_t* data() override { return reinterpret_cast<uint8_t*>(m_str.data()); }
        size_t size() override { return m_str.size(); }
        size_t max_size() override { return -1; }
        bool is_decoder() override { return true; }
        bool is_encoder() override { return true; }
        std::string name() const override { return "JPN"; }
        std::string ext() const override { return ".jpn"; }
        SfenOutputType type() const override { return SfenOutputType::Jpn; }
        void buffer(PosBuffer& pb) override;
        PosBuffer& buffer() override;
        PosBuffer* copy() override;
        void encode(const PosData& pos) override;
        void decode(PosData& pos) override;
    };

    class FenCodec : public PosCodec
    {
    public:
        uint8_t* data() override { return nullptr; }
        size_t size() override { return 0; }
        size_t max_size() override { return 0; }
        bool is_decoder() override { return true; }
        bool is_encoder() override { return true; }
        std::string name() const override { return "FEN"; }
        std::string ext() const override { return ".fen"; }
        SfenOutputType type() const override { return SfenOutputType::Fen; }
        void buffer(PosBuffer& pb) override;
        PosBuffer& buffer() override;
        PosBuffer* copy() override;
        void encode(const PosData& pos) override;
        void decode(PosData& pos) override;
    };

    class Bin2Codec : public PosCodec
    {
    private:
        BitStream m_stream;
        Bin2PosBuffer m_data;
    public:
        Bin2Codec() { m_stream.set_data(m_data.data()); }
        uint8_t* data() override { return m_data.data(); }
        size_t size() override { return m_stream.size_bytes(); }
        size_t max_size() override { return m_data.max_size(); }
        bool is_decoder() override { return true; }
        bool is_encoder() override { return true; }
        std::string name() const override { return "BIN2"; }
        std::string ext() const override { return ".bin2"; }
        SfenOutputType type() const override { return SfenOutputType::Bin2; }
        void buffer(PosBuffer& pb) override;
        PosBuffer& buffer() override;
        PosBuffer* copy() override;
        void encode(const PosData& pos) override;
        void decode(PosData& pos) override;
    };

    class BinCodec : public PosCodec
    {
    private:
        BitStream m_stream;
        BinPosBuffer m_data;
    public:
        BinCodec() { m_stream.set_data(m_data.data()); }
        uint8_t* data() override { return m_data.data(); }
        size_t size() override { return m_data.size(); }
        size_t max_size() override { return (DATA_SIZE / 8 + 8); }
        bool is_decoder() override { return true; }
        bool is_encoder() override { return true; }
        std::string name() const override { return "BIN"; }
        std::string ext() const override { return ".bin"; }
        SfenOutputType type() const override { return SfenOutputType::Bin; }
        void buffer(PosBuffer& pb) override;
        PosBuffer& buffer() override;
        PosBuffer* copy() override;
        void encode(const PosData& pos) override;
        void decode(PosData& pos) override;

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
        void write_board_piece_to_stream(const Position& pos, Piece pc);

        // Read one board piece from stream
        Piece read_board_piece_from_stream(const Position& pos);
    };

    class CodecRegister
    {
    private:
        unordered_map<string, PosCodec*> codec_names;
        unordered_map<string, PosCodec*> codec_exts;
        unordered_map<SfenOutputType, PosCodec*> codec_types;
        vector<PosCodec*> m_codecs;
    public:
        CodecRegister();
        void register_codec(PosCodec* codec);
        PosCodec* get_name(const string& name );
        PosCodec* get_ext(const string& ext);
        PosCodec* get_path(const string& path);
        PosCodec* get_type(const SfenOutputType& type);
    };

    extern CodecRegister codecs;

    extern const std::string plain_extension;
    extern const std::string bin_extension;
    extern const std::string binpack_extension;
    extern const std::string bin2_extension;
    extern const std::string jpn_extension;
    extern const std::string fen_extension;
    extern const std::string epd_extension;

    extern SfenOutputType data_format;
    extern PosCodec* pos_codec;

    PosCodec* get_codec(const std::string& path);
    PosCodec* get_codec_ext(const std::string& ext);
    PosCodec* get_codec_type(const SfenOutputType type);
}
