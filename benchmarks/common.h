#ifndef BENCHMARKS_COMMON_H
#define BENCHMARKS_COMMON_H

#include <fstream>
#include <format>

#include "protos/queries.pb.h"

#define BS_THREAD_POOL_NATIVE_EXTENSIONS
#include "BS_thread_pool.hpp"

namespace rogue
{
    namespace benchmarks
    {
        constexpr uint64_t TOTAL_OPERATIONS{ 5400000 };
        constexpr uint32_t STARTING_DATA{ 5000000 };
        const std::string BYTES_100{ "1aa_aa_aa_2bb_bb_bb_3cc_cc_cc_4dd_dd_dd_5ee_ee_ee_6ff_ff_ff_7gg_gg_gg_8hh_hh_hh_9ii_ii_ii_0jj_jj_jj_" };
        const std::string BYTES_50{ "1aa_aa_aa_2bb_bb_bb_3cc_cc_cc_4dd_dd_dd_5ee_ee_ee_" };
        const std::string API_KEY{ "api" };
        
        constexpr uint64_t TOTAL_WORKERS{ 50 };
        static BS::thread_pool<BS::tp::none> threadpool{ TOTAL_WORKERS };

        rogue::services::Subscribe createSubscribe();
        void initialLog(const std::string& filename);
        void logBenchmark(
            const std::string& filename,
            const std::string benchmark,
            const std::chrono::_V2::system_clock::time_point start, 
            const std::chrono::_V2::system_clock::time_point finish,
            uint64_t readOperations, 
            const uint64_t writeOperations);

        class CommaPunctuation : public std::numpunct<char>
        {
        protected:
            virtual char do_thousands_sep() const
            {
                return ',';
            }

            virtual std::string do_grouping() const
            {
                return "\03";
            }
        };
    }
}

#endif
