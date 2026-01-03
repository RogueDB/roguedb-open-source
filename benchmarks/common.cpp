#include "benchmarks/common.h"


rogue::services::Subscribe rogue::benchmarks::createSubscribe()
{
    rogue::services::Subscribe subscribe{};
    subscribe.set_api_key(API_KEY);
    subscribe.add_schemas("syntax = \"proto3\";\n\
\n\
package rogue.benchmarks;\n\
message Dummy\n\
{\n\
    uint64 id = 1; //index-1\n\
    string field1 = 2;\n\
    string field2 = 3;\n\
    string field3 = 4;\n\
    string field4 = 5;\n\
    string field5 = 6;\n\
    string field6 = 7;\n\
    string field7 = 8;\n\
    string field8 = 9;\n\
    string field9 = 10;\n\
    string field10 = 11;\n\
}");
    return subscribe;
}

void rogue::benchmarks::initialLog(const std::string& filename)
{
    std::ofstream output{ filename, std::ios::app };
    output << "| Benchmark | Execution Time | Read Ops | Write Ops | Throughput |\n";
    output << "| --- | --- | --- | --- | ---: |\n";
    output.close();
}

void rogue::benchmarks::logBenchmark(
    const std::string& filename,
    const std::string benchmark,
    const std::chrono::_V2::system_clock::time_point start, 
    const std::chrono::_V2::system_clock::time_point finish,
    uint64_t readOperations, 
    const uint64_t writeOperations)
{
    std::ofstream output{ filename, std::ios::app };
    const double seconds{ std::chrono::duration<double>(finish - start).count() };
    const double operationsPerSecond{ (readOperations + writeOperations) / seconds };
    output << std::format(
        std::locale(std::locale{}, new rogue::benchmarks::CommaPunctuation{}), "| {} | {:.3Lf} s | {:L} op | {:L} op | {:.2Lf} op/s |\n", 
        benchmark, seconds, readOperations, 
        writeOperations, operationsPerSecond);
    output.close();
}
