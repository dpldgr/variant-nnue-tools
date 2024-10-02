#ifndef _SFEN_STREAM_H_
#define _SFEN_STREAM_H_

#include "packed_sfen.h"
#include "posbuffer.h"

#include "extra/nnue_data_binpack_format.h"

#include <optional>
#include <fstream>
#include <string>
#include <memory>
#include <vector>

using namespace std;

namespace Stockfish::Tools {

    enum struct SfenOutputType
    {
        Bin,
        Binpack,
        Bin2,
        Plain,
        Fen,
        Epd,
        Jpn,
    };

    enum struct PosCodecType
    {
        Bin,
        Binpack,
        Bin2,
        Plain,
        FEN,
        EPD,
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

    struct PosInputStream
    {
        virtual std::optional<PosBuffer*> read() = 0;
        virtual bool eof() const = 0;
        virtual bool is_open() const = 0;
        virtual ~PosInputStream() {}
    };

    struct BinPosInputStream : PosInputStream
    {
        static constexpr auto openmode = std::ios::in | std::ios::binary;
        static inline const std::string extension = "bin";

        BinPosInputStream(std::string filename) :
            m_stream(filename, openmode),
            m_eof(!m_stream)
        {
        }

        std::optional<PosBuffer*> read() override
        {
            BinPosBuffer* e = new BinPosBuffer();

            if (m_stream.read(reinterpret_cast<char*>(e->data()), e->size()))
            {
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

        bool is_open() const override
        {
            return m_stream.is_open();
        }

        ~BinPosInputStream() override {}

    private:
        std::fstream m_stream;
        bool m_eof;
    };

    struct Bin2PosInputStream : PosInputStream
    {
        static constexpr auto openmode = std::ios::in | std::ios::binary;
        static inline const std::string extension = "bin2";
        //static constexpr std::array<uint8_t, 4> file_header = { 0xFE, 0xB7, 0xD2, 0x2F }; // Magic is the first four bytes of SHA256('{type:"bin2",version:"1.0"}').
        static constexpr std::array<uint8_t, 5> file_header = { 0xC2, 0x34, 0x56, 0x78, 0x20 };
        bool header_read = false;
        bool header_match = false;

        Bin2PosInputStream(std::string filename) :
            m_stream(filename, openmode),
            m_eof(!m_stream)
        {
        }

        std::optional<PosBuffer*> read()
        {
            Bin2PosBuffer* pb = new Bin2PosBuffer();
            size_t loc = 0;
            uint16_t pos_size = 0;

            if (!header_read)
            {
                std::array<uint8_t, 5> header_data;
                m_stream.read(header_data.data(), header_data.size());
                if (std::equal(header_data.begin(), header_data.end(), file_header.begin()))
                    header_match = true;
                header_read = true;
            }

            if (m_stream.read(reinterpret_cast<uint8_t*>(&pos_size), sizeof(pos_size)))
            {
                pb->size(pos_size);

                if (m_stream.read(pb->data(), pos_size))
                {
                    return pb;
                }
                else
                {
                    m_eof = true;
                    return std::nullopt;
                }
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

        bool is_open() const override
        {
            return m_stream.is_open();
        }

        ~Bin2PosInputStream() override {}

    private:
        std::basic_fstream<uint8_t> m_stream;
        bool m_eof;
    };

    struct BinpackPosInputStream : PosInputStream
    {
        static constexpr auto openmode = std::ios::in | std::ios::binary;
        static inline const std::string extension = "binpack";

        BinpackPosInputStream(std::string filename) :
            m_stream(filename, openmode),
            m_eof(!m_stream.hasNext())
        {
        }

        std::optional<PosBuffer*> read() override
        {
            //static_assert(sizeof(binpack::nodchip::PackedSfenValue) == sizeof(BinPackedPosFileData)); // FIXME.

            if (!m_stream.hasNext())
            {
                m_eof = true;
                return std::nullopt;
            }

            auto training_data_entry = m_stream.next();
            auto v = binpack::trainingDataEntryToPackedSfenValue(training_data_entry);
            BinPackedPosFileData* psv = new BinPackedPosFileData();
            // same layout, different types. One is from generic library.
            std::memcpy(psv, &v, sizeof(BinPackedPosFileData));

            //return psv;
            return std::nullopt; // FIXME.
        }

        bool eof() const override
        {
            return m_eof;
        }

        bool is_open() const override
        {
            // TODO.

            return false;
        }

        ~BinpackPosInputStream() override {}

    private:
        binpack::CompressedTrainingDataEntryReader m_stream;
        bool m_eof;
    };

    struct PosOutputStream
    {
        virtual void write_header() {};
        virtual void write_footer() {};
        virtual void write(const vector<string*>& strs) {};
        virtual void write(PosBuffer* pb) = 0;
        virtual void write(const std::vector<PosBuffer*>& pb) = 0;
        virtual void write(const std::vector<PackedPosFileData*>& ppfd) = 0;
        virtual void write(const PPVector& sfens) = 0;
        //virtual void last_pos( bool v ) = 0;
        virtual bool is_open() const = 0;
        virtual ~PosOutputStream() {}
    };

    struct AbstractPosOutputStream
        : public PosOutputStream
    {
    protected:
        bool header_written = false;
        bool footer_written = false;
        bool first_position = true;
    public:
        void write(PosBuffer* pb) override { assert(false); };
        void write(const std::vector<PosBuffer*>& pb) override { assert(false); };
        void write(const std::vector<PackedPosFileData*>& ppfd) override { assert(false); };
        void write(const PPVector& sfens) override { assert(false); };
        //void last_pos(bool v) override { last_position=v; };
        bool is_open() const override { assert(false); return false; };
    };

    struct JpnPosOutputStream : AbstractPosOutputStream
    {
        static constexpr auto openmode = std::ios::out | std::ios::app;
        static inline const std::string extension = "jpn";
        std::string file_header = "{\"header\":{\"type\":\"jpn\",\"version\":\"1.0\",\"magic\":\"1c36f8e2\"},\n\"variant\":{\"name\":\"chess\",\"files\":8,\"ranks\":8,\"squares\":64,\"types\":12,\"magic\":\"b2d69903\"},\n\"positions\":[";
        std::string file_footer = "]}\n";
        std::string pos_separator = ",";

        JpnPosOutputStream(std::string filename) :  
            m_stream(filename_with_extension(filename, extension), openmode)
        {
        }

        void write_header() override
        {
            if (!header_written)
            {
                m_stream.write(file_header.data(), file_header.size());
                header_written = true;
            }
        };

        void write_footer() override
        {
            if (!footer_written)
            {
                m_stream.write(file_footer.data(), file_footer.size());
                footer_written = true;
            }
        };

        void write(const vector<string*>& strs) override
        {
            for (string* str : strs)
            {
                m_stream.write(str->data(), str->size());
            }
        }

        void write(PosBuffer* pb) override
        {
            write_header();

            if (first_position)
                first_position = false;
            else
                m_stream.write(pos_separator.data(), pos_separator.size());

            pb->write(m_stream);
        }

        void write(const vector<PosBuffer*>& pbs) override
        {
            int size = pbs.size();

            write_header();

            for (int i = 0 ; i < size; i++)
            {
                pbs[i]->write(m_stream);
                if ( i < (size - 1) )
                    m_stream.write(pos_separator.data(), pos_separator.size());
            }

            write_footer();
        }

        void write(const vector<PackedPosFileData*>& ppfds) override
        {
            for (PackedPosFileData* ppfd : ppfds)
            {
                m_stream.write(reinterpret_cast<const char*>(ppfd->data()), ppfd->size());
            }
        }

        void write(const PPVector& sfens) override
        {
            m_stream.write(reinterpret_cast<const char*>(sfens.data()), sizeof(BinPackedPosFileData) * sfens.size());
        }

        bool is_open() const override
        {
            return m_stream.is_open();
        }

        ~JpnPosOutputStream() override {}

    private:
        std::basic_fstream<char> m_stream;
    };


    struct BinPosOutputStream : AbstractPosOutputStream
    {
        static constexpr auto openmode = std::ios::out | std::ios::binary | std::ios::app;
        static inline const std::string extension = "bin";

        BinPosOutputStream(std::string filename) :
            m_stream(filename_with_extension(filename, extension), openmode)
        {
        }

        void write(PosBuffer* pb) override
        {
            pb->write(m_stream);
        }

        void write(const vector<PosBuffer*>& pbs) override
        {
            for (PosBuffer* pb : pbs)
            {
                pb->write(m_stream);
            }
        }

        void write(const vector<PackedPosFileData*>& ppfds) override
        {
            for (PackedPosFileData* ppfd : ppfds)
            {
                m_stream.write(reinterpret_cast<const char*>(ppfd->data()), ppfd->size());
            }
        }

        void write(const PPVector& sfens) override
        {
            m_stream.write(reinterpret_cast<const char*>(sfens.data()), sizeof(BinPackedPosFileData) * sfens.size());
        }

        bool is_open() const override
        {
            return m_stream.is_open();
        }

        ~BinPosOutputStream() override {}

    private:
        std::basic_fstream<char> m_stream;
    };

    struct Bin2PosOutputStream : AbstractPosOutputStream
    {
        static constexpr auto openmode = std::ios::out | std::ios::binary | std::ios::app;
        static inline const std::string extension = "bin2";
        //static constexpr std::array<uint8_t, 4> file_header = { 0xFE, 0xB7, 0xD2, 0x2F }; // Magic is the first four bytes of SHA256('{type:"bin2",version:"1.0"}').
        static constexpr std::array<uint8_t, 5> file_header = { 0xC2, 0x34, 0x56, 0x78, 0x20 };
        bool header_written = false;

        Bin2PosOutputStream(std::string filename) :
            m_stream(filename_with_extension(filename, extension), openmode)
        {
        }

        void write(PosBuffer* pb) override
        {
            pb->write(m_stream);
        }

        void write(const vector<PosBuffer*>& pbs) override
        {
            if (!header_written)
            {
                m_stream.write(reinterpret_cast<const char*>(file_header.data()), file_header.size());
                header_written = true;
            }

            for (PosBuffer* pb : pbs)
            {
                pb->write(m_stream);
            }
        }

        void write(const vector<PackedPosFileData*>& ppfds) override
        {
            if (!header_written)
            {
                m_stream.write(reinterpret_cast<const char*>(file_header.data()), file_header.size());
                header_written = true;
            }

            for (PackedPosFileData* ppfd : ppfds)
            {
                m_stream.write(reinterpret_cast<const char*>(ppfd->data()), ppfd->size());
            }
        }

        void write(const PPVector& sfens) override
        {
            if (!header_written)
            {
                m_stream.write(reinterpret_cast<const char*>(file_header.data()), file_header.size());
                header_written = true;
            }

            for (const PackedPos& sfen : sfens)
            {
                int write_size = sfen.size();
                m_stream.write(reinterpret_cast<const char*>(&write_size), 2);
                m_stream.write(reinterpret_cast<const char*>(sfen.data()), write_size);
            }
        }

        bool is_open() const override
        {
            return m_stream.is_open();
        }

        ~Bin2PosOutputStream() override {}

    private:
        std::basic_fstream<char> m_stream;
    };

    struct BinpackPosOutputStream : AbstractPosOutputStream
    {
        static constexpr auto openmode = std::ios::out | std::ios::binary | std::ios::app;
        static inline const std::string extension = "binpack";

        BinpackPosOutputStream(std::string filename) :
            m_stream(filename_with_extension(filename, extension), openmode)
        {
        }

        void write(PosBuffer* pb) override
        {
            // TODO.
        }

        void write(const vector<PosBuffer*>& pbs) override
        {
            // TODO.
        }

        void write(const vector<PackedPosFileData*>& ppfds) override
        {
            // TODO.
        }

        void write(const PPVector& sfens) override
        {
            //static_assert(sizeof(binpack::nodchip::PackedSfenValue) == sizeof(BinPackedPosFileData // FIXME.

            for(auto& sfen : sfens)
            {
                // The library uses a type that's different but layout-compatible.
                binpack::nodchip::PackedSfenValue e;
                std::memcpy(&e, &sfen, sizeof(binpack::nodchip::PackedSfenValue));
                m_stream.addTrainingDataEntry(binpack::packedSfenValueToTrainingDataEntry(e));
            }
        }

        bool is_open() const override
        {
            // TODO.

            return false;
        }

        ~BinpackPosOutputStream() override {}

    private:
        binpack::CompressedTrainingDataEntryWriter m_stream;
    };

    inline std::unique_ptr<PosInputStream> open_sfen_input_file(const std::string& filename)
    {
        if (has_extension(filename, BinPosInputStream::extension))
            return std::make_unique<BinPosInputStream>(filename);
        else if (has_extension(filename, BinpackPosInputStream::extension))
            return std::make_unique<BinpackPosInputStream>(filename);
        else if (has_extension(filename, Bin2PosInputStream::extension))
            return std::make_unique<Bin2PosInputStream>(filename);

        return nullptr;
    }

    inline std::unique_ptr<PosOutputStream> create_new_sfen_output(const std::string& filename, SfenOutputType sfen_output_type)
    {
        switch(sfen_output_type)
        {
            case SfenOutputType::Bin:
                return std::make_unique<BinPosOutputStream>(filename);
            case SfenOutputType::Binpack:
                return std::make_unique<BinpackPosOutputStream>(filename);
            case SfenOutputType::Bin2:
                return std::make_unique<Bin2PosOutputStream>(filename);
            case SfenOutputType::Jpn:
                return std::make_unique<JpnPosOutputStream>(filename);
        }

        assert(false);
        return nullptr;
    }

    inline std::unique_ptr<PosOutputStream> create_new_sfen_output(const std::string& filename)
    {
        if (has_extension(filename, BinPosOutputStream::extension))
            return std::make_unique<BinPosOutputStream>(filename);
        else if (has_extension(filename, BinpackPosOutputStream::extension))
            return std::make_unique<BinpackPosOutputStream>(filename);
        else if (has_extension(filename, Bin2PosOutputStream::extension))
            return std::make_unique<Bin2PosOutputStream>(filename);
        else if (has_extension(filename, JpnPosOutputStream::extension))
            return std::make_unique<JpnPosOutputStream>(filename);

        return nullptr;
    }
}

#endif