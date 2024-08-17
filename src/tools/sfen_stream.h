#ifndef _SFEN_STREAM_H_
#define _SFEN_STREAM_H_

#include "packed_sfen.h"

#include "extra/nnue_data_binpack_format.h"

#include <optional>
#include <fstream>
#include <string>
#include <memory>

namespace Stockfish::Tools {

    enum struct SfenOutputType
    {
        Bin,
        Binpack,
        Bin2,
    };

    static bool ends_with(const std::string& lhs, const std::string& end)
    {
        if (end.size() > lhs.size()) return false;

        return std::equal(end.rbegin(), end.rend(), lhs.rbegin());
    }

    static bool has_extension(const std::string& filename, const std::string& extension)
    {
        return ends_with(filename, "." + extension);
    }

    static std::string filename_with_extension(const std::string& filename, const std::string& ext)
    {
        if (ends_with(filename, ext))
        {
            return filename;
        }
        else
        {
            return filename + "." + ext;
        }
    }

    struct BasicSfenInputStream
    {
        virtual std::optional<PackedPos> next() = 0;
        virtual bool eof() const = 0;
        virtual ~BasicSfenInputStream() {}
    };

    struct BinSfenInputStream : BasicSfenInputStream
    {
        static constexpr auto openmode = std::ios::in | std::ios::binary;
        static inline const std::string extension = "bin";

        BinSfenInputStream(std::string filename) :
            m_stream(filename, openmode),
            m_eof(!m_stream)
        {
        }

        std::optional<PackedPos> next() override
        {
            BinPackedPos e;
            BinPosFileData f_data;

            if (m_stream.read(reinterpret_cast<char*>(&f_data), sizeof(f_data)))
            {
                // TODO: covert into PackedPos.
                return e;
            }
            else
            {
                m_eof = true;
                return std::nullopt;
            }
        }

        bool eof() const override
        {
            return m_eof;
        }

        ~BinSfenInputStream() override {}

    private:
        std::fstream m_stream;
        bool m_eof;
    };

    struct Bin2InputStream : BasicSfenInputStream
    {
        static constexpr auto openmode = std::ios::in | std::ios::binary;
        static inline const std::string extension = "bin2";
        static constexpr std::array<uint8_t, 5> file_header = { 0xC2, 0x34, 0x56, 0x78, 0x20 };
        bool header_read = false;
        bool header_match = false;

        Bin2InputStream(std::string filename) :
            m_stream(filename, openmode),
            m_eof(!m_stream)
        {
        }

        std::optional<PackedPos> next() override
        {
            Bin2PackedPos e;
            Bin2PosFileData f_data;

            if (!header_read)
            {
                std::array<uint8_t, 5> header_data;
                m_stream.read(header_data.data(), header_data.size());
                if ( std::equal(header_data.begin(), header_data.end(), file_header.begin()) )
                    header_match = true;
                header_read = true;
            }

            if (m_stream.read(reinterpret_cast<uint8_t*>(&f_data.size), sizeof(f_data.size)) && m_stream.read(reinterpret_cast<uint8_t*>(&f_data.data), f_data.size))
            {
                // TODO: covert to Bin2PackedPos.

                return e;
            }
            else
            {
                m_eof = true;
                return std::nullopt;
            }
        }

        bool eof() const override
        {
            return m_eof;
        }

        ~Bin2InputStream() override {}

    private:
        std::basic_fstream<uint8_t> m_stream;
        bool m_eof;
    };

    /* TODO: Delete all this binpack stuff?
    struct BinpackSfenInputStream : BasicSfenInputStream
    {
        static constexpr auto openmode = std::ios::in | std::ios::binary;
        static inline const std::string extension = "binpack";

        BinpackSfenInputStream(std::string filename) :
            m_stream(filename, openmode),
            m_eof(!m_stream.hasNext())
        {
        }

        std::optional<PackedSfenValue> next() override
        {
            static_assert(sizeof(binpack::nodchip::PackedSfenValue) == sizeof(PackedSfenValue));

            if (!m_stream.hasNext())
            {
                m_eof = true;
                return std::nullopt;
            }

            auto training_data_entry = m_stream.next();
            auto v = binpack::trainingDataEntryToPackedSfenValue(training_data_entry);
            PackedSfenValue psv;
            // same layout, different types. One is from generic library.
            std::memcpy(&psv, &v, sizeof(PackedSfenValue));

            return psv;
        }

        bool eof() const override
        {
            return m_eof;
        }

        ~BinpackSfenInputStream() override {}

    private:
        binpack::CompressedTrainingDataEntryReader m_stream;
        bool m_eof;
    };
    //*/

    struct BasicSfenOutputStream
    {
        virtual void write(const PSVector& sfens) = 0;
        virtual ~BasicSfenOutputStream() {}
    };

    struct BinSfenOutputStream : BasicSfenOutputStream
    {
        static constexpr auto openmode = std::ios::out | std::ios::binary | std::ios::app;
        static inline const std::string extension = "bin";

        BinSfenOutputStream(std::string filename) :
            m_stream(filename_with_extension(filename, extension), openmode)
        {
        }

        void write(const PSVector& sfens) override
        {
            m_stream.write(reinterpret_cast<const char*>(sfens.data()), sizeof(BinPosFileData) * sfens.size());
        }

        ~BinSfenOutputStream() override {}

    private:
        std::fstream m_stream;
    };

    struct Bin2OutputStream : BasicSfenOutputStream
    {
        static constexpr auto openmode = std::ios::out | std::ios::binary | std::ios::app;
        static inline const std::string extension = "bin2";
        static constexpr std::array<uint8_t, 5> file_header = { 0xC2, 0x34, 0x56, 0x78, 0x20 };
        bool header_written = false;

        Bin2OutputStream(std::string filename) :
            m_stream(filename_with_extension(filename, extension), openmode)
        {
        }

        void write(const PSVector& sfens) override
        {
            if (!header_written)
            {
                m_stream.write(file_header.data(), file_header.size());
                header_written = true;
            }

            for (auto& sfen : sfens)
            {
                /* FIXME
                int i = (DATA_SIZE / 8) - 1;
                //int i = (BIN2_DATA_SIZE / 8) - 1;

                for (; i >= 0; i--)
                {
                    if (sfen.stream_data.data[i] != 0)
                        break;
                }

                int write_size = (7 + i + 1) & 0x3FFF;
                //*/

                m_stream.write(reinterpret_cast<const uint8_t*>(&sfen.data_size), 2);
                m_stream.write(reinterpret_cast<const uint8_t*>(&sfen.buffer), sfen.data_size);
            }
        }

        ~Bin2OutputStream() override {}

    private:
        std::basic_fstream<uint8_t> m_stream;
    };

    /* TODO: Delete all this binpack stuff?
    struct BinpackSfenOutputStream : BasicSfenOutputStream
    {
        static constexpr auto openmode = std::ios::out | std::ios::binary | std::ios::app;
        static inline const std::string extension = "binpack";

        BinpackSfenOutputStream(std::string filename) :
            m_stream(filename_with_extension(filename, extension), openmode)
        {
        }

        void write(const PSVector& sfens) override
        {
            static_assert(sizeof(binpack::nodchip::PackedSfenValue) == sizeof(PackedSfenValue));

            for(auto& sfen : sfens)
            {
                // The library uses a type that's different but layout-compatible.
                binpack::nodchip::PackedSfenValue e;
                std::memcpy(&e, &sfen, sizeof(binpack::nodchip::PackedSfenValue));
                m_stream.addTrainingDataEntry(binpack::packedSfenValueToTrainingDataEntry(e));
            }
        }

        ~BinpackSfenOutputStream() override {}

    private:
        binpack::CompressedTrainingDataEntryWriter m_stream;
    };
    //*/

    inline std::unique_ptr<BasicSfenInputStream> open_sfen_input_file(const std::string& filename)
    {
        if (has_extension(filename, BinSfenInputStream::extension))
            return std::make_unique<BinSfenInputStream>(filename);
        /* TODO: Delete all this binpack stuff?
        else if (has_extension(filename, BinpackSfenInputStream::extension))
            return std::make_unique<BinpackSfenInputStream>(filename);
        //*/
        else if (has_extension(filename, Bin2InputStream::extension))
            return std::make_unique<Bin2InputStream>(filename);

        return nullptr;
    }

    inline std::unique_ptr<BasicSfenOutputStream> create_new_sfen_output(const std::string& filename, SfenOutputType sfen_output_type)
    {
        switch(sfen_output_type)
        {
            case SfenOutputType::Bin:
                return std::make_unique<BinSfenOutputStream>(filename);
            /* TODO: Delete all this binpack stuff?
            case SfenOutputType::Binpack:
                return std::make_unique<BinpackSfenOutputStream>(filename);
            //*/
            case SfenOutputType::Bin2:
                return std::make_unique<Bin2OutputStream>(filename);
        }

        assert(false);
        return nullptr;
    }

    inline std::unique_ptr<BasicSfenOutputStream> create_new_sfen_output(const std::string& filename)
    {
        if (has_extension(filename, BinSfenOutputStream::extension))
            return std::make_unique<BinSfenOutputStream>(filename);
        /* TODO: Delete all this binpack stuff?
        else if (has_extension(filename, BinpackSfenOutputStream::extension))
            return std::make_unique<BinpackSfenOutputStream>(filename);
        //*/
        else if (has_extension(filename, Bin2OutputStream::extension))
            return std::make_unique<Bin2OutputStream>(filename);

        return nullptr;
    }
}

#endif