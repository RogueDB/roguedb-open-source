/*
 * Copyright (C) Rogue LLC - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Written by Jacob Bartholomew Blankenship [jacob.blankenship@roguedb.com] - January 2022
*/

#include <latch>
#include <random>
#include <grpcpp/grpcpp.h>

#include "benchmarks/common.h"
#include "benchmarks/zipfian_generator.h"

#include "protos/experiment.grpc.pb.h"
#include "protos/dummy.pb.h"

const std::string FORCED_CHANNEL_BENCHMARK_FILE{ "GRPC_FORCED_CHANNEL_BENCHMARKS.md" };
const std::string MULTI_PORT_BENCHMARK_FILE{ "GRPC_MULTI_PORT_BENCHMARKS.md" };
const std::string MULTI_SERVER_BENCHMARK_FILE{ "GRPC_MULTI_SERVER_BENCHMARKS.md" };
const std::string THREAD_BENCHMARK_FILE{ "GRPC_THREADING_BENCHMARKS.md" };
const std::string BENCHMARK_FILE{ "GRPC_BENCHMARKS.md" };

class SearchChatter : public grpc::ClientBidiReactor<rogue::services::Search, rogue::services::Response> 
{
public:
    explicit SearchChatter(
        std::unique_ptr<rogue::services::Experiment::Stub>& stub,
        const uint32_t operations)
        : m_totalOperations{ operations }
    {
        rogue::services::Query* query{ m_search.add_queries() };
        rogue::services::Basic& expression{ *query->mutable_basic() };
        expression.set_logical_operator(rogue::services::LogicalOperator::AND);
        expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
        expression.add_operands();

        rogue::benchmarks::Dummy dummy{};
        dummy.set_id(1);
        query->mutable_basic()->mutable_operands(0)->PackFrom(dummy);

        stub->async()->search(&m_context, this);
        nextWrite();
        StartRead(&m_response);
        StartCall();
    }
    void OnWriteDone(bool ok) override
    {
        if(ok) 
        {
            nextWrite();
        }
    }
    void OnReadDone(bool ok) override
    {
        if(ok)
        {
            StartRead(&m_response);
        }
    }
    void OnDone(const grpc::Status& status) override 
    {
        std::unique_lock<std::mutex> l(m_stateLock);
        m_status = status;
        m_done = true;
        m_conditional.notify_one();
    }

    grpc::Status Await()
    {
        std::unique_lock<std::mutex> l(m_stateLock);
        m_conditional.wait(l, [this] { return m_done; });
        return std::move(m_status);
    }

private:
    void nextWrite()
    {
        if(m_count++ < m_totalOperations)
        {
            StartWrite(&m_search);
        }
        else
        {   
            StartWritesDone();
        }
    }
    
    const uint32_t m_totalOperations;
    uint32_t m_count{ 0 };
    grpc::ClientContext m_context{};
    rogue::services::Search m_search{};
    rogue::services::Response m_response{};
    std::mutex m_stateLock{};
    std::condition_variable m_conditional{};
    grpc::Status m_status{};
    bool m_done{ false };
};

void readOnlyBulkAsync(const std::string& ipAddress, const uint64_t batchSize)
{
    std::latch latch{ 1 };
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS / rogue::benchmarks::TOTAL_WORKERS };
    
    for(uint64_t index{ 0 }; index < rogue::benchmarks::TOTAL_WORKERS; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&]()
            {
                std::unique_ptr<rogue::services::Experiment::Stub> readerStub{ rogue::services::Experiment::NewStub(
                    grpc::CreateChannel(std::format("{}:80", ipAddress), grpc::InsecureChannelCredentials())) };
                latch.wait();

                SearchChatter chatter{ readerStub, operationsPerThread };
                grpc::Status status = chatter.Await();
                if (!status.ok()) {
                    std::cout << "RouteChat rpc failed." << std::endl;
                }
                std::cout << "finished reads" << std::endl;
            });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto start{ std::chrono::high_resolution_clock::now() };
    latch.count_down();
    rogue::benchmarks::threadpool.wait();
    const auto finish{ std::chrono::high_resolution_clock::now() };

    rogue::benchmarks::logBenchmark(BENCHMARK_FILE, std::format("Read Only Bulk Asyc {}", batchSize), start, finish, operationsPerThread * rogue::benchmarks::TOTAL_WORKERS, 0);
}

void singleReadAllWriteAll(const std::string& ipAddress)
{
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS / 10 };
    std::unique_ptr<rogue::services::Experiment::Stub> readerStub{ rogue::services::Experiment::NewStub(
        grpc::CreateChannel(std::format("{}:80", ipAddress), grpc::InsecureChannelCredentials())) };
    grpc::ClientContext readerContext{};
    std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
        readerStub->singleReadAllWriteAll(&readerContext) };
    std::random_device randomizer{};
    std::mt19937 generator{ randomizer() };
    
    rogue::services::Search search{};
    search.set_api_key(rogue::benchmarks::API_KEY);
    
    rogue::services::Query* query{ search.add_queries() };
    rogue::services::Basic& expression{ *query->mutable_basic() };
    expression.set_logical_operator(rogue::services::LogicalOperator::AND);
    expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
    expression.add_operands();

    rogue::benchmarks::Dummy dummy{};
    rogue::utilities::ZipfianGenerator zipfian{rogue::benchmarks::STARTING_DATA, .9};
    rogue::concepts::Generator<uint64_t> zipfianGenerator{ zipfian.generate(generator) };
    rogue::services::Response readResponse{};
    dummy.set_id(zipfianGenerator());
    search.mutable_queries(0)->mutable_basic()->mutable_operands(0)->PackFrom(dummy);
    
    const auto start{ std::chrono::high_resolution_clock::now() };
    for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
    {
        stream->Write(search);
    }
    stream->WritesDone();

    for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
    {
        if(!stream->Read(&readResponse))
        {
            grpc::Status status{ stream->Finish() };
            std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
            throw std::runtime_error{"Could not recover."};
        }
    }

    const auto finish{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::logBenchmark(
        BENCHMARK_FILE, 
        "Send All Receive All - Batch 1", 
        start, finish, operationsPerThread, 0);
}

void singleReadWriteAlternate(const std::string& ipAddress)
{
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS / 10 };
    
    std::unique_ptr<rogue::services::Experiment::Stub> readerStub{ rogue::services::Experiment::NewStub(
        grpc::CreateChannel(std::format("{}:80", ipAddress), grpc::InsecureChannelCredentials())) };
    
    grpc::ClientContext readerContext{};
    std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
        readerStub->singleReadWriteAlternate(&readerContext) };
    std::random_device randomizer{};
    std::mt19937 generator{ randomizer() };
    
    rogue::services::Search search{};
    search.set_api_key(rogue::benchmarks::API_KEY);
    
    rogue::services::Query* query{ search.add_queries() };
    rogue::services::Basic& expression{ *query->mutable_basic() };
    expression.set_logical_operator(rogue::services::LogicalOperator::AND);
    expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
    expression.add_operands();

    rogue::benchmarks::Dummy dummy{};
    rogue::utilities::ZipfianGenerator zipfian{rogue::benchmarks::STARTING_DATA, .9};
    rogue::concepts::Generator<uint64_t> zipfianGenerator{ zipfian.generate(generator) };
    rogue::services::Response readResponse{};
    dummy.set_id(zipfianGenerator());
    search.mutable_queries(0)->mutable_basic()->mutable_operands(0)->PackFrom(dummy);
    
    const auto start{ std::chrono::high_resolution_clock::now() };
    for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
    {
        stream->Write(search);
        if(!stream->Read(&readResponse))
        {
            grpc::Status status{ stream->Finish() };
            std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
            throw std::runtime_error{"Could not recover."};
        }
    }
    stream->WritesDone();

    const auto finish{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::logBenchmark(
        BENCHMARK_FILE, 
        "Alternate Send Receive - Batch 1", start, finish, operationsPerThread, 0);
}

void singleReadAllNoResponse(const std::string& ipAddress)
{
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS / 10 };
    
    std::unique_ptr<rogue::services::Experiment::Stub> readerStub{ rogue::services::Experiment::NewStub(
        grpc::CreateChannel(std::format("{}:80", ipAddress), grpc::InsecureChannelCredentials())) };
    
    grpc::ClientContext readerContext{};
    std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
        readerStub->singleReadAllNoResponse(&readerContext) };
    std::random_device randomizer{};
    std::mt19937 generator{ randomizer() };
    
    rogue::services::Search search{};
    search.set_api_key(rogue::benchmarks::API_KEY);
    
    rogue::services::Query* query{ search.add_queries() };
    rogue::services::Basic& expression{ *query->mutable_basic() };
    expression.set_logical_operator(rogue::services::LogicalOperator::AND);
    expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
    expression.add_operands();

    rogue::benchmarks::Dummy dummy{};
    rogue::utilities::ZipfianGenerator zipfian{rogue::benchmarks::STARTING_DATA, .9};
    rogue::concepts::Generator<uint64_t> zipfianGenerator{ zipfian.generate(generator) };
    rogue::services::Response readResponse{};
    dummy.set_id(zipfianGenerator());
    search.mutable_queries(0)->mutable_basic()->mutable_operands(0)->PackFrom(dummy);
    
    const auto start{ std::chrono::high_resolution_clock::now() };
    for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
    {
        stream->Write(search);
    }
    stream->WritesDone();

    const auto finish{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::logBenchmark(
        BENCHMARK_FILE, 
        "Send All No Response - Batch 1", start, finish, operationsPerThread, 0);
}

void bulkReadAllWriteAll(const std::string& ipAddress, const uint64_t batchSize)
{
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS };
    
    std::unique_ptr<rogue::services::Experiment::Stub> readerStub{ rogue::services::Experiment::NewStub(
        grpc::CreateChannel(std::format("{}:80", ipAddress), grpc::InsecureChannelCredentials())) };
    
    grpc::ClientContext readerContext{};
    std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
        readerStub->bulkReadAllWriteAll(&readerContext) };
    std::random_device randomizer{};
    std::mt19937 generator{ randomizer() };
    
    rogue::services::Search search{};
    search.set_api_key(rogue::benchmarks::API_KEY);
    rogue::benchmarks::Dummy dummy{};
    
    for(uint32_t count{ 0 }; count < batchSize; ++count)
    {
        rogue::services::Query* query{ search.add_queries() };
        rogue::services::Basic& expression{ *query->mutable_basic() };
        expression.set_logical_operator(rogue::services::LogicalOperator::AND);
        expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
        expression.add_operands();
    }

    rogue::services::Response readResponse{};
    const auto start{ std::chrono::high_resolution_clock::now() };
    for(uint64_t count{ 0 }; count < operationsPerThread; count += batchSize)
    {
        stream->Write(search);
    }
    stream->WritesDone();

    for(uint64_t count{ 0 }; count < operationsPerThread; count += batchSize)
    {
        if(!stream->Read(&readResponse))
        {
            grpc::Status status{ stream->Finish() };
            std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
            throw std::runtime_error{"Could not recover."};
        }
    }

    const auto finish{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::logBenchmark(
        BENCHMARK_FILE, 
        std::format("Send All Receive All - Batch {}", batchSize), start, finish, operationsPerThread, 0);
}

void bulkReadWriteAlternate(const std::string& ipAddress, const uint64_t batchSize)
{
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS };
    
    std::unique_ptr<rogue::services::Experiment::Stub> readerStub{ rogue::services::Experiment::NewStub(
        grpc::CreateChannel(std::format("{}:80", ipAddress), grpc::InsecureChannelCredentials())) };
    
    grpc::ClientContext readerContext{};
    std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
        readerStub->bulkReadWriteAlternate(&readerContext) };
    std::random_device randomizer{};
    std::mt19937 generator{ randomizer() };
    
    rogue::services::Search search{};
    search.set_api_key(rogue::benchmarks::API_KEY);
    rogue::benchmarks::Dummy dummy{};
    
    for(uint32_t count{ 0 }; count < batchSize; ++count)
    {
        rogue::services::Query* query{ search.add_queries() };
        rogue::services::Basic& expression{ *query->mutable_basic() };
        expression.set_logical_operator(rogue::services::LogicalOperator::AND);
        expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
        expression.add_operands();
    }

    rogue::services::Response readResponse{};
    const auto start{ std::chrono::high_resolution_clock::now() };
    for(uint64_t count{ 0 }; count < operationsPerThread; count += batchSize)
    {
        stream->Write(search);
        if(!stream->Read(&readResponse))
        {
            grpc::Status status{ stream->Finish() };
            std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
            throw std::runtime_error{"Could not recover."};
        }
    }
    stream->WritesDone();
    
    const auto finish{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::logBenchmark(
        BENCHMARK_FILE, 
        std::format("Alternate Send Receive - Batch {}", batchSize), start, finish, operationsPerThread, 0);
}

void bulkReadAllNoResponse(const std::string& ipAddress, const uint64_t batchSize)
{
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS };
    
    std::unique_ptr<rogue::services::Experiment::Stub> readerStub{ rogue::services::Experiment::NewStub(
        grpc::CreateChannel(std::format("{}:80", ipAddress), grpc::InsecureChannelCredentials())) };
    
    grpc::ClientContext readerContext{};
    std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
        readerStub->bulkReadAllNoResponse(&readerContext) };
    std::random_device randomizer{};
    std::mt19937 generator{ randomizer() };
    
    rogue::services::Search search{};
    search.set_api_key(rogue::benchmarks::API_KEY);
    rogue::benchmarks::Dummy dummy{};
    
    for(uint32_t count{ 0 }; count < batchSize; ++count)
    {
        rogue::services::Query* query{ search.add_queries() };
        rogue::services::Basic& expression{ *query->mutable_basic() };
        expression.set_logical_operator(rogue::services::LogicalOperator::AND);
        expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
        expression.add_operands();
    }

    rogue::services::Response readResponse{};
    const auto start{ std::chrono::high_resolution_clock::now() };
    for(uint64_t count{ 0 }; count < operationsPerThread; count += batchSize)
    {
        stream->Write(search);
    }
    stream->WritesDone();
    
    const auto finish{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::logBenchmark(
        BENCHMARK_FILE, 
        std::format("Send All No Response - Batch {}", batchSize), start, finish, operationsPerThread, 0);
}

/*
Observations:

- An active stream causes baseline CPU idle of ~20%. 
- Multiple streams caps at ~40% utilization.
- 1 single stream is equivalent to 3 single streams. 
    - Diminishing returns post-6 streams.
    - Peaks around 7 streams. 1.9x Throughput Increase
- Useage of separate channels (not uniquely identifiable) and stubs did not affect performance.
*/
void singleReadAllWriteAllThreaded(const std::string& ipAddress, const uint64_t threadCount)
{
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS / 10 };
    std::latch latch{ 1 };
    
    for(uint32_t index{ 0 }; index < threadCount; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&]() mutable
            {
                std::unique_ptr<rogue::services::Experiment::Stub> readerStub{ rogue::services::Experiment::NewStub(
                    grpc::CreateChannel(std::format("{}:80", ipAddress), grpc::InsecureChannelCredentials())) };
                grpc::ClientContext readerContext{};
                std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
                    readerStub->singleReadAllWriteAll(&readerContext) };
                std::random_device randomizer{};
                std::mt19937 generator{ randomizer() };
                
                rogue::services::Search search{};
                search.set_api_key(rogue::benchmarks::API_KEY);
                
                rogue::services::Query* query{ search.add_queries() };
                rogue::services::Basic& expression{ *query->mutable_basic() };
                expression.set_logical_operator(rogue::services::LogicalOperator::AND);
                expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
                expression.add_operands();
            
                rogue::benchmarks::Dummy dummy{};
                rogue::utilities::ZipfianGenerator zipfian{rogue::benchmarks::STARTING_DATA, .9};
                rogue::concepts::Generator<uint64_t> zipfianGenerator{ zipfian.generate(generator) };
                rogue::services::Response readResponse{};
                dummy.set_id(zipfianGenerator());
                search.mutable_queries(0)->mutable_basic()->mutable_operands(0)->PackFrom(dummy);
                
                latch.wait();
                for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
                {
                    stream->Write(search);
                }
                stream->WritesDone();
            
                for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
                {
                    if(!stream->Read(&readResponse))
                    {
                        grpc::Status status{ stream->Finish() };
                        std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                        throw std::runtime_error{"Could not recover."};
                    }
                }
            }
        );
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    latch.count_down();
    const auto start{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::threadpool.wait();
    const auto finish{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::logBenchmark(
        THREAD_BENCHMARK_FILE, 
        std::format("gRPC Single Read All Write All - {} thread(s)", threadCount), 
        start, finish, operationsPerThread * threadCount, 0);
}

void singleReadAllWriteAllMultipleServers(const std::string& ipAddress, const uint64_t serverCount)
{
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS / 10 };
    std::latch latch{ 1 };
    std::vector<uint32_t> ports{ 80, 82, 83, 84, 85 };
    for(uint32_t count{ 0 }; count < serverCount; ++count)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, index = count]() mutable
            {
                std::unique_ptr<rogue::services::Experiment::Stub> readerStub{ rogue::services::Experiment::NewStub(
                    grpc::CreateChannel(std::format("{}:{}", ipAddress, ports[index]), grpc::InsecureChannelCredentials())) };
                grpc::ClientContext readerContext{};
                std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
                    readerStub->singleReadAllWriteAll(&readerContext) };
                std::random_device randomizer{};
                std::mt19937 generator{ randomizer() };
                
                rogue::services::Search search{};
                search.set_api_key(rogue::benchmarks::API_KEY);
                
                rogue::services::Query* query{ search.add_queries() };
                rogue::services::Basic& expression{ *query->mutable_basic() };
                expression.set_logical_operator(rogue::services::LogicalOperator::AND);
                expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
                expression.add_operands();
            
                rogue::benchmarks::Dummy dummy{};
                rogue::utilities::ZipfianGenerator zipfian{rogue::benchmarks::STARTING_DATA, .9};
                rogue::concepts::Generator<uint64_t> zipfianGenerator{ zipfian.generate(generator) };
                rogue::services::Response readResponse{};
                dummy.set_id(zipfianGenerator());
                search.mutable_queries(0)->mutable_basic()->mutable_operands(0)->PackFrom(dummy);
                
                latch.wait();
                for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
                {
                    stream->Write(search);
                }
                stream->WritesDone();
            
                for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
                {
                    if(!stream->Read(&readResponse))
                    {
                        grpc::Status status{ stream->Finish() };
                        std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                        throw std::runtime_error{"Could not recover."};
                    }
                }
            }
        );
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    latch.count_down();
    const auto start{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::threadpool.wait();
    const auto finish{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::logBenchmark(
        MULTI_SERVER_BENCHMARK_FILE, 
        std::format("gRPC Single Read All Write All - {} Servers", serverCount), 
        start, finish, operationsPerThread * serverCount, 0);
}

void singleReadAllWriteAllMultiplePorts(const std::string& ipAddress, const uint64_t portCount)
{
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS / 10 };
    std::latch latch{ 1 };
    std::vector<uint32_t> ports{ 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101 };
    
    for(uint32_t index{ 0 }; index < portCount; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, count = index]() mutable
            {
                std::unique_ptr<rogue::services::Experiment::Stub> readerStub{ rogue::services::Experiment::NewStub(
                    grpc::CreateChannel(std::format("{}:{}", ipAddress, ports[count]), grpc::InsecureChannelCredentials())) };
                grpc::ClientContext readerContext{};
                std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
                    readerStub->singleReadAllWriteAll(&readerContext) };
                std::random_device randomizer{};
                std::mt19937 generator{ randomizer() };
                
                rogue::services::Search search{};
                search.set_api_key(rogue::benchmarks::API_KEY);
                
                rogue::services::Query* query{ search.add_queries() };
                rogue::services::Basic& expression{ *query->mutable_basic() };
                expression.set_logical_operator(rogue::services::LogicalOperator::AND);
                expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
                expression.add_operands();
            
                rogue::benchmarks::Dummy dummy{};
                rogue::utilities::ZipfianGenerator zipfian{rogue::benchmarks::STARTING_DATA, .9};
                rogue::concepts::Generator<uint64_t> zipfianGenerator{ zipfian.generate(generator) };
                rogue::services::Response readResponse{};
                dummy.set_id(zipfianGenerator());
                search.mutable_queries(0)->mutable_basic()->mutable_operands(0)->PackFrom(dummy);
                
                latch.wait();
                for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
                {
                    stream->Write(search);
                }
                stream->WritesDone();
            
                for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
                {
                    if(!stream->Read(&readResponse))
                    {
                        grpc::Status status{ stream->Finish() };
                        std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                        throw std::runtime_error{"Could not recover."};
                    }
                }
            }
        );
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    latch.count_down();
    const auto start{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::threadpool.wait();
    const auto finish{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::logBenchmark(
        MULTI_PORT_BENCHMARK_FILE, 
        std::format("gRPC Single Read All Write All - {} port(s)", portCount), 
        start, finish, operationsPerThread * portCount, 0);
}

void singleReadAllWriteAllForcedChannel(const std::string& ipAddress, const uint64_t threadCount)
{
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS / 10 };
    std::latch latch{ 1 };
    
    for(uint32_t index{ 0 }; index < threadCount; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, count = index]() mutable
            {
                grpc::ChannelArguments arguments{};
                arguments.SetInt("dummy", count);
                std::unique_ptr<rogue::services::Experiment::Stub> readerStub{ rogue::services::Experiment::NewStub(
                    grpc::CreateCustomChannel(
                        std::format("{}:86",ipAddress), 
                        grpc::InsecureChannelCredentials(), 
                        arguments)) };
                
                grpc::ClientContext readerContext{};
                std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
                    readerStub->singleReadAllWriteAll(&readerContext) };
                std::random_device randomizer{};
                std::mt19937 generator{ randomizer() };
                
                rogue::services::Search search{};
                search.set_api_key(rogue::benchmarks::API_KEY);
                
                rogue::services::Query* query{ search.add_queries() };
                rogue::services::Basic& expression{ *query->mutable_basic() };
                expression.set_logical_operator(rogue::services::LogicalOperator::AND);
                expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
                expression.add_operands();
            
                rogue::benchmarks::Dummy dummy{};
                rogue::utilities::ZipfianGenerator zipfian{rogue::benchmarks::STARTING_DATA, .9};
                rogue::concepts::Generator<uint64_t> zipfianGenerator{ zipfian.generate(generator) };
                rogue::services::Response readResponse{};
                dummy.set_id(zipfianGenerator());
                search.mutable_queries(0)->mutable_basic()->mutable_operands(0)->PackFrom(dummy);
                
                latch.wait();
                for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
                {
                    stream->Write(search);
                }
                stream->WritesDone();
            
                for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
                {
                    if(!stream->Read(&readResponse))
                    {
                        grpc::Status status{ stream->Finish() };
                        std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                        throw std::runtime_error{"Could not recover."};
                    }
                }
            }
        );
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    latch.count_down();
    const auto start{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::threadpool.wait();
    const auto finish{ std::chrono::high_resolution_clock::now() };
    rogue::benchmarks::logBenchmark(
        FORCED_CHANNEL_BENCHMARK_FILE, 
        std::format("gRPC Single Read All Write All Forced Channel - {} threads", threadCount), 
        start, finish, operationsPerThread * threadCount, 0);
}

// NOTE: Run the following beforehand: bazel run //roguedb/management:sync_server &
int main(int argc, char** argv)
{
    std::filesystem::remove(FORCED_CHANNEL_BENCHMARK_FILE);
    rogue::benchmarks::initialLog(FORCED_CHANNEL_BENCHMARK_FILE);

    std::filesystem::remove(MULTI_PORT_BENCHMARK_FILE);
    rogue::benchmarks::initialLog(MULTI_PORT_BENCHMARK_FILE);

    std::filesystem::remove(MULTI_SERVER_BENCHMARK_FILE);
    rogue::benchmarks::initialLog(MULTI_SERVER_BENCHMARK_FILE);
    
    std::filesystem::remove(THREAD_BENCHMARK_FILE);
    rogue::benchmarks::initialLog(THREAD_BENCHMARK_FILE);

    std::filesystem::remove(BENCHMARK_FILE);
    rogue::benchmarks::initialLog(BENCHMARK_FILE);
    const std::string ipAddress{ argv[1] };

    singleReadAllWriteAll(ipAddress);
    singleReadWriteAlternate(ipAddress);
    singleReadAllNoResponse(ipAddress);

    const std::vector<uint64_t> grpcBatchSizes{ 10, 100, 1000 };
    for(const auto& batchSize : grpcBatchSizes)
    {
        bulkReadAllWriteAll(ipAddress, batchSize);
        bulkReadWriteAlternate(ipAddress, batchSize);
        bulkReadAllNoResponse(ipAddress, batchSize);
    }
    
    const std::vector<uint64_t> threads{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    for(const uint64_t threadCount : threads)
    {
        singleReadAllWriteAllForcedChannel(ipAddress, threadCount);
    }

    const std::vector<uint64_t> ports{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    for(const uint64_t portCount : ports)
    {
        singleReadAllWriteAllMultiplePorts(ipAddress, portCount);
    }

    const std::vector<uint64_t> servers{ 1, 2, 3, 4, 5 };
    for(const uint64_t serverCount : servers)
    {
        singleReadAllWriteAllMultipleServers(ipAddress, serverCount);
    }

    for(const uint64_t threadCount : threads)
    {
        singleReadAllWriteAllThreaded(ipAddress, threadCount);
    }
}