#include "extract.h"

#include "poscodec.h"
#include "search.h"
#include "thread.h"

#include <iostream>
#include <string>

using namespace std;

namespace Stockfish::Tools
{
    struct ExtractParams
    {
        std::string input_filename = "in.bin";
        std::string output_filename = "out.bin2";
        std::string input_codec = "bin";
        std::string output_codec = "bin2";
        int skip = 0;
        int count = -1;
        bool process_all = true;
        bool rescore = false;
        int depth = 4;
        int nodes = 0;

        void enforce_constraints()
        {
            if (skip < 0)
                skip = 0;
            if (count < -1)
                count = -1;
            if (count == 0)
                count = 1;
            if (!(skip == 0 && count == -1))
                process_all = false;
        }
    };

    void do_rescore(PosData& pd)
    {
        auto [search_value, search_pv] = Search::search(pd.pos, 10, 1, 0);

        if (search_pv.empty())
            return;

        pd.move = search_pv[0];
        pd.score = search_value;
    }

    void do_extract(ExtractParams& params)
    {
        unique_ptr<PosInputStream> file_in = open_sfen_input_file(params.input_filename);
        unique_ptr<PosOutputStream> file_out = create_new_sfen_output(params.output_filename);
        PosCodec* codec_in = codecs.get_path(params.input_filename);
        PosCodec* codec_out = codecs.get_path(params.output_filename);
        bool conversion_required = false, last_position = false;
        size_t num_processed_positions = 0, num_skipped_positions = 0;
        optional<PosBuffer*> pbuf;
        Position pos;
        PosData info(pos);
        StateListPtr states;
        const Variant* v = variants.find(Options["UCI_Variant"])->second;
        StateInfo si;
        PosCodecHelper hlp(&pos, &si, v);
        bool can_proceed = true;

        // { Check for any issues with codecs and files.
        if (codec_in == nullptr)
        {
            std::cerr << "No matching codec found for file: " << params.input_filename << ".\n";
            can_proceed = false;
        }
        else if (!codec_in->is_decoder())
        {
            std::cerr << "Codec " << codec_in->name() << " cannot be used for decoding.\n";
            can_proceed = false;
        }

        if (codec_out == nullptr)
        {
            std::cerr << "No matching codec found for file: " << params.output_filename << ".\n";
            can_proceed = false;
        }
        else if (!codec_out->is_encoder())
        {
            std::cerr << "Codec " << codec_out->name() << " cannot be used for encoding.\n";
            can_proceed = false;
        }

        if ( codec_in != codec_out )
        {
            conversion_required = true;
        }

        if (!file_in->is_open())
        {
            cout << "ERROR: couldn't open input file.\n";
            can_proceed = false;
        }

        if (!file_out->is_open())
        {
            cout << "ERROR: couldn't open output file.\n";
            can_proceed = false;
        }
        // } Check for any issues with codecs and files.

        if (!can_proceed)
            return;

        file_out->write_header();

        for ( int i = 0 ; ; i++ )
        {
            pbuf = file_in->read();

            if (pbuf.has_value())
            {
                hlp.new_states();

                // Skip the specified number of positions.
                if (i < params.skip)
                {
                    ++num_skipped_positions;
                    continue;
                }

                // Stop when the specified number of positions have been processed.
                if (params.count != -1 && i >= (params.skip + params.count))
                    break;

                if (conversion_required)
                {
                    codec_in->buffer(*pbuf.value());
                    codec_in->decode(info);
                }

                // NOTE: see ThreadPool::start_thinking, "position", and "bench" uci commands for how to properly setup position.

                // Perform an action on the decoded position.
                if (params.rescore)
                {
                    Search::LimitsType limits;

                    //pos.set(v, v->startFen, false, &states->back(), Threads.main());

                    limits.startTime = now();
                    limits.depth = params.depth;
                    Threads.start_thinking(pos, states, limits, false);
                    Threads.main()->wait_for_search_finished();
                    //do_rescore(info);
                }

                if (conversion_required)
                {
                    codec_out->encode(info);
                    pbuf = codec_out->copy();
                }

                file_out->write(pbuf.value());
            }
            // Stop when there are no more positions in the input file.
            else
            {
                break;
            }

            ++num_processed_positions;
        }

        file_out->write_footer();

        cout << "Finished. Skipped " << num_skipped_positions << " positions. Processed " << num_processed_positions << " positions.\n";
    }

    void extract(std::istringstream& is)
    {
        ExtractParams params{};

        while (true)
        {
            std::string token;
            is >> token;

            if (token == "")
                break;

            if (token == "-s" || token == "skip")
                is >> params.skip;
            else if (token == "-i" || token == "input_file")
                is >> params.input_filename;
            else if (token == "-o" || token == "output_file")
                is >> params.output_filename;
            else if (token == "-ic" || token == "input_codec")
                is >> params.input_codec;
            else if (token == "-oc" || token == "output_codec")
                is >> params.output_codec;
            else if (token == "-c" || token == "count")
                is >> params.count;
            else if (token == "-r" || token == "rescore")
                params.rescore = true;
            else if (token == "-d" || token == "depth")
                is >> params.depth;
            else if (token == "-n" || token == "nodes")
                is >> params.nodes;
            else
            {
                cout << "ERROR: Unknown option " << token << ". Exiting...\n";
                return;
            }
        }

        params.enforce_constraints();

        cout << "Performing extract with parameters:\n";
        cout << "input_file          : " << params.input_filename << '\n';
        cout << "output_file         : " << params.output_filename << '\n';
        if ( params.skip != 0 )
            cout << "skip                : " << params.skip << '\n';
        if ( params.count != -1 )
            cout << "count               : " << params.count << '\n';
        cout << '\n';

        do_extract(params);
    }
}
