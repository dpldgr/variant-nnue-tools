#ifndef _PACKED_SFEN_H_
#define _PACKED_SFEN_H_

#include <vector>
#include <cstdint>

#include "posbuffer.h"

namespace Stockfish::Tools {

    struct PackedPosFileData
    {
        virtual ~PackedPosFileData() {}
        virtual uint8_t* begin() { return nullptr; }
        virtual uint8_t* end() { return nullptr; }
        //virtual PackedPosBuffer* buffer() {return nullptr;}
        virtual uint8_t* data() { return nullptr; }
        virtual uint8_t* data_begin() = 0;
        virtual uint8_t* data_end() = 0;
        virtual size_t size() { return 0; }
    };

    // Structure in which BinPackedPosBuffer and evaluation value are integrated
    // If you write different contents for each option, it will be a problem when reusing the teacher game
    // For the time being, write all the following members regardless of the options.
    struct BinPackedPosFileData : PackedPosFileData
    {
        // phase
        //BinPackedPosBuffer sfen;
        std::uint8_t sfen[DATA_SIZE/8];

        // Evaluation value returned from Tools::search()
        std::int16_t score;

        // PV first move
        // Used when finding the match rate with the teacher
        std::uint16_t move;

        // Trouble of the phase from the initial phase.
        std::uint16_t gamePly;

        // 1 if the player on this side ultimately wins the game. -1 if you are losing.
        // 0 if a draw is reached.
        // The draw is in the teacher position generation command gensfen,
        // Only write if LEARN_GENSFEN_DRAW_RESULT is enabled.
        std::int8_t game_result;

        // When exchanging the file that wrote the teacher aspect with other people
        //Because this structure size is not fixed, pad it so that it is 72 bytes in any environment.
        std::uint8_t padding;
        // 64 + 2 + 2 + 2 + 1 + 1 = 72bytes

        uint8_t* begin() override { return &sfen[0]; };
        uint8_t* end() override { return &sfen[0]+71; };
        //PackedPosBuffer* buffer() override { return &sfen; };
        uint8_t* data() override { return &sfen[0]; }
        uint8_t* data_begin() { return &sfen[0]; }
        uint8_t* data_end() { return &sfen[0] +71; }
        size_t size() override { return 72; }
    };

    struct Bin2PackedPosFileData : PackedPosFileData
    {
        std::int16_t _size;

        // Evaluation value returned from Tools::search()
        std::int16_t score;

        // PV first move
        // Used when finding the match rate with the teacher
        std::uint16_t move;

        // Trouble of the phase from the initial phase.
        std::uint16_t gamePly;

        // 1 if the player on this side ultimately wins the game. -1 if you are losing.
        // 0 if a draw is reached.
        // The draw is in the teacher position generation command gensfen,
        // Only write if LEARN_GENSFEN_DRAW_RESULT is enabled.
        std::int8_t game_result;

        // phase
        //Bin2PackedPosBuffer sfen;
        std::uint8_t sfen[BIN2_DATA_SIZE/8];

        uint8_t* begin() override { return reinterpret_cast<uint8_t*>(&score); };
        uint8_t* end() override { return reinterpret_cast<uint8_t*>(&score+_size); };
        //PackedPosBuffer* buffer() override { return &sfen; };
        uint8_t* data() override { return &sfen[0]; }
        uint8_t* data_begin() { return &sfen[0]; }
        uint8_t* data_end() { return &sfen[0]+_size; }
        size_t size() override { return _size+sizeof(_size); }
    };

    class PackedPos
    {
    protected:
        PackedPosFileData* _data = nullptr;
        size_t _size = 0;

    public:
        PackedPos(){}

        PackedPos( const PackedPos& pp )
        {
            _data = pp._data;
            _size = pp._size;
        }

        ~PackedPos()
        {
            if (_data)
                delete _data;
        }

        PackedPosFileData* data() const { return _data; }
        size_t size() const { return _size; }

        virtual void result(int8_t r) {}
        virtual void ply(uint16_t p) {}
        virtual void move(uint16_t m) {}
        virtual void score(int16_t s) {}
    };

    class BinPackedPos : public PackedPos {

        BinPackedPos()
        {
            this->_data = new BinPackedPosFileData();
            this->_size = 72;
        }

        virtual void result(int8_t r)
        {
            BinPackedPosFileData* data = dynamic_cast<BinPackedPosFileData*>(this->_data);
            data->game_result = r;
        }

        virtual void ply(uint16_t p)
        {
            BinPackedPosFileData* data = dynamic_cast<BinPackedPosFileData*>(this->_data);
            data->gamePly = p;
        }

        virtual void move(uint16_t m)
        {
            BinPackedPosFileData* data = dynamic_cast<BinPackedPosFileData*>(this->_data);
            data->move = m;
        }

        virtual void score(int16_t s)
        {
            BinPackedPosFileData* data = dynamic_cast<BinPackedPosFileData*>(this->_data);
            data->score = s;
        }
    };

    class Bin2PackedPos : public PackedPos {

        Bin2PackedPos()
        {
            this->_data = new Bin2PackedPosFileData();
        }

        virtual void result(int8_t r)
        {
            Bin2PackedPosFileData* data = dynamic_cast<Bin2PackedPosFileData*>(this->_data);
            data->game_result = r;
        }

        virtual void ply(uint16_t p)
        {
            Bin2PackedPosFileData* data = dynamic_cast<Bin2PackedPosFileData*>(this->_data);
            data->gamePly = p;
        }

        virtual void move(uint16_t m)
        {
            Bin2PackedPosFileData* data = dynamic_cast<Bin2PackedPosFileData*>(this->_data);
            data->move = m;
        }

        virtual void score(int16_t s)
        {
            Bin2PackedPosFileData* data = dynamic_cast<Bin2PackedPosFileData*>(this->_data);
            data->score = s;
        }
    };

    // Phase array: PPVector stands for packed position vector.
    using PPVector = std::vector<PackedPos>;
    using PBVector = std::vector<PosBuffer*>;
}
#endif
