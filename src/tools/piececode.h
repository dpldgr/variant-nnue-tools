#pragma once
#include <cmath>
#include "types.h"

namespace Stockfish::Tools
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

        void PieceCode::calc_code_size(int type_count)
        {
            code_size = ceil(log2(type_count));
        }

        PieceCode::PieceCode()
        {}

        PieceCode::PieceCode(bool is_piece)
            :_is_piece(is_piece), _bits(code_size)
        {}

        PieceCode::PieceCode(int code)
            : _code(code), _bits(code_size)
        {}

        PieceCode::PieceCode(Piece pc)
            : _bits(code_size)
        {
            PieceType pt = type_of(pc);
            Color c = color_of(pc);

            if (pt == NO_PIECE_TYPE)
            {
                _is_piece = false;
                _code = 0;
            }
            else if (pt == KING)
            {
                _is_king = true;
                _is_piece = true;
                _code = (c << (_bits - 1));
            }
            else
            {
                _is_piece = true;
                _code = (c << (_bits - 1)) | pt;
            }
        }

        PieceCode::PieceCode(Color c, PieceType pt)
            :_bits(code_size)
        {
            if (pt == NO_PIECE_TYPE)
            {
                _is_piece = false;
                _code = 0;
            }
            else if (pt == KING)
            {
                _is_king = true;
                _is_piece = true;
                _code = (c << (_bits - 1));
            }
            else
            {
                _is_piece = true;
                _code = (c << (_bits - 1)) | pt;
            }
        }

        PieceCode& PieceCode::operator=(Piece pc)
        {
            PieceType pt = type_of(pc);
            Color c = color_of(pc);

            if (pt == NO_PIECE_TYPE)
            {
                _is_piece = false;
                _code = 0;
            }
            else if (pt == KING)
            {
                _is_king = true;
                _is_piece = true;
                _code = (c << (_bits - 1));
            }
            else
            {
                _is_piece = true;
                _code = (c << (_bits - 1)) | pt;
            }

            return *this;
        }

        PieceCode& PieceCode::operator=(Color c)
        {
            if (_code != 0)
            {
                _code = (_code & ~(1 << _bits)) | (c << _bits);
            }

            return *this;
        }

        PieceCode& PieceCode::operator=(PieceType pt)
        {
            if (pt == NO_PIECE_TYPE)
            {
                _is_piece = false;
                _code = 0;
            }
            else if (pt == KING)
            {
                _is_king = true;
                _is_piece = true;
                _code = (c() << (_bits - 1));
            }
            else
            {
                _is_piece = true;
                _code = (c() << (_bits - 1)) | pt;
            }

            return *this;
        }

        inline Piece PieceCode::pc() const
        {
            Piece ret = make_piece(c(), pt());

            return ret;
        }

        inline PieceType PieceCode::pt() const
        {
            if (!_is_piece)
            {
                return NO_PIECE_TYPE;
            }
            else if (_is_king)
            {
                return KING;
            }
            else
            {
                return PieceType(_code & ((1 << (_bits - 1)) - 1));
            }
        }

        inline Color PieceCode::c() const
        {
            Color ret = Color(_code >> (_bits - 1));

            return ret;
        }

        inline PieceCode::operator Piece() const
        {
            return pc();
        }

        inline PieceCode::operator PieceType() const
        {
            return pt();
        }

        inline PieceCode::operator Color() const
        {
            return c();
        }

        inline int PieceCode::code() const
        {
            return _code;
        }

        inline void PieceCode::code(int c)
        {
            _code = c;
        }

        inline int PieceCode::bits() const
        {
            return _bits;
        }

        inline bool PieceCode::is_empty() const
        {
            return (_is_piece == false);
        }

        inline bool PieceCode::is_piece() const
        {
            return (_is_piece == true);
        }

        inline bool PieceCode::is_king() const
        {
            return (_is_king == true);
        }
    };
}
