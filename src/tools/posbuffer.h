#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <sstream>
#include <sstream>
#include <ostream>
#include <assert.h>

namespace Stockfish::Tools
{
    class PosBuffer
    {
    public:
        virtual void clear() = 0;
        virtual uint8_t* data() = 0;
        virtual size_t size() = 0;
        virtual void size( size_t s ) = 0;
        virtual size_t max_size() = 0;
        virtual PosBuffer* copy() = 0;
        virtual void write(std::ostream& out) = 0;
    };

    class JpnPosBuffer : public PosBuffer
    {
        std::string m_data;
    public:
        JpnPosBuffer(std::string& str) :m_data(str) {}
        JpnPosBuffer(const JpnPosBuffer& buf) :m_data(buf.m_data) {}
        void clear() override { m_data=""; }
        uint8_t* data() override { return reinterpret_cast<uint8_t*>(m_data.data() ); }
        size_t size() override { return m_data.size(); }
        void size(size_t s) override {}
        size_t max_size() override { return -1; }
        PosBuffer* copy() override { return new JpnPosBuffer(*this); };
        void write(std::ostream& out) override { out.write(m_data.data(), m_data.size()); };
    };

    class BinPosBuffer : public PosBuffer
    {
        std::array<uint8_t, 72> _data {};
    public:
        BinPosBuffer() {}
        BinPosBuffer(const BinPosBuffer& buf) { std::copy(buf._data.begin(), buf._data.end(), _data.begin()); }
        void clear() override { std::memset( _data.data(), 0, _data.size() ); }
        uint8_t* data() override { return _data.data(); }
        size_t size() override { return _data.size(); }
        void size(size_t s) override {}
        size_t max_size() override { return _data.size(); }
        PosBuffer* copy() override { return new BinPosBuffer(*this); };
        void write(std::ostream& out) override { out.write(reinterpret_cast<char*>(_data.data()), _data.size()); };
    };

    class Bin2PosBuffer : public PosBuffer
    {
        size_t m_size = 0;
        std::array<uint8_t, 256> m_data {};
    public:
        Bin2PosBuffer() {}
        Bin2PosBuffer(const Bin2PosBuffer& buf) { std::copy(buf.m_data.begin(), buf.m_data.end(), m_data.begin()); size(buf.m_size); }
        void clear() override { std::memset( m_data.data(), 0, m_data.size() ); m_size = 0; }
        uint8_t* data() override { return m_data.data(); }
        size_t size() override { return m_size; }
        void size(size_t s) override { m_size = s; assert(m_size <= 256); }
        size_t max_size() override { return m_data.size(); }
        PosBuffer* copy() override{ return new Bin2PosBuffer(*this); };
        void write(std::ostream& out) override
        { 
            out.write(reinterpret_cast<char*>(m_data.data()), m_size); 
        };
    };
}
