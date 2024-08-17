#ifndef _PACKED_SFEN_H_
#define _PACKED_SFEN_H_

#include <vector>
#include <cstdint>

namespace Stockfish::Tools {

    // NOTE: Must be the same size as BinPackedPosValue for reinterpret_cast<BinPackedPosValue> to work.
    struct BinPosFileData
    {
        std::uint8_t data[DATA_SIZE / 8 + 8]; // A position can use up to 72 bytes.
    };

    // NOTE: Must be the same size as Bin2PackedPosValue for reinterpret_cast<Bin2PackedPosValue> to work.
    struct Bin2PosFileData
    {
        std::uint16_t size; // BIN2 is variable width rather than fixed width so needs a size field per postion.
        std::uint8_t data[BIN2_DATA_SIZE / 8 + 7]; // A position can use up to 263 bytes.
    };

    struct PackedPos
    {
        uint8_t* buffer = nullptr;
        uint32_t buffer_size = 0;
        uint32_t data_size = 0;

        ~PackedPos()
        { 
            if ( buffer )
                delete buffer;
        }

        virtual void set_result(int8_t result) {};
    };

    struct BinPackedPos : public PackedPos
    {
        BinPackedPos()
        {
            buffer = reinterpret_cast<uint8_t*>(new BinPosFileData());
            buffer_size = sizeof(BinPosFileData);
            data_size = sizeof(BinPosFileData); // BIN has a fixed width position with a maximum size of 72 bytes.
        }

        virtual void set_result(int8_t result)
        {
            // TODO.
        }
    };

    struct Bin2PackedPos : public PackedPos
    {
        Bin2PackedPos()
        {
            buffer = reinterpret_cast<uint8_t*>(new Bin2PosFileData());
            buffer_size = sizeof(Bin2PosFileData);
            data_size = 0; // BIN2 has a variable width position with maximum size of 2^14 bytes.
        }

        virtual void set_result(int8_t result)
        {
            // TODO.
        }
    };

    //struct PackedSfen { std::uint8_t data[DATA_SIZE / 8]; };
    struct BinStreamData { std::uint8_t data[DATA_SIZE / 8]; };
    struct Bin2StreamData { std::uint8_t data[BIN2_DATA_SIZE / 8]; };

    // Structure in which PackedSfen and evaluation value are integrated
    // If you write different contents for each option, it will be a problem when reusing the teacher game
    // For the time being, write all the following members regardless of the options.
    // NOTE: Must be the same size as BinPosBytes.
    //struct PackedSfenValue
    struct BinPackedPosValue
    {
        // phase
        BinStreamData stream_data;

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
    };

    struct Bin2PackedPosValue
    {
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
        Bin2StreamData stream_data;
    };

    // Phase array: PSVector stands for packed sfen vector.
    using PSVector = std::vector<PackedPos>;
}
#endif
