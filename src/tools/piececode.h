
#pragma once

#include "types.h"

namespace Stockfish
{
    class PieceCode
    {
    private:
        int _code = 0;
        int _bits = code_size;
        bool _is_piece = false;
        bool _is_king = false;

    public:
        static int code_size;
        static void calc_code_size(int type_count);
        PieceCode();
        PieceCode(bool is_piece);
        PieceCode(int code);
        PieceCode(Piece pc);
        PieceCode(Color c, PieceType pt);
        PieceCode& operator=(Piece pc);
        PieceCode& operator=(Color c);
        PieceCode& operator=(PieceType pt);
        Piece pc() const;
        PieceType pt() const;
        Color c() const;
        operator Piece() const;
        operator PieceType() const;
        operator Color() const;
        int code() const;
        void code(int c);
        int bits() const;
        bool is_empty() const;
        bool is_piece() const;
        bool is_king() const;
    };
}
