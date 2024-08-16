#ifndef _PACKED_SFEN_H_
#define _PACKED_SFEN_H_

#include <vector>
#include <cstdint>

namespace Stockfish::Tools {

    // packed sfen
    typedef std::array<uint8_t, DATA_SIZE/8+8> BinPosPacked;
    typedef std::array<uint8_t, BIN2_DATA_SIZE/8> Bin2PosPacked;

    struct PackedPos
    {
        uint8_t* data;
        uint32_t size;
    };

    struct PackedSfen { std::uint8_t data[DATA_SIZE / 8]; };
    struct Bin2PackedPos { std::uint8_t data[BIN2_DATA_SIZE / 8]; };

    struct BasePosValue {};

    // Structure in which PackedSfen and evaluation value are integrated
    // If you write different contents for each option, it will be a problem when reusing the teacher game
    // For the time being, write all the following members regardless of the options.
    struct PackedSfenValue : BasePosValue
    {
        // phase
        std::uint8_t sfen[DATA_SIZE / 8]{};
        //PackedSfen sfen;

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

    struct Bin2PackedPosValue : BasePosValue
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
        //PackedSfen sfen;
        std::uint8_t sfen[BIN2_DATA_SIZE / 8]{};
    };

    // Phase array: PSVector stands for packed sfen vector.
    using PSVector = std::vector<PackedSfenValue>;
    using PPVector = std::vector<Bin2PackedPosValue>;
}
#endif
