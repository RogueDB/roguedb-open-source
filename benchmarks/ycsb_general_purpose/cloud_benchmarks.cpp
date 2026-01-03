#include <latch>
#include <random>
#include <grpcpp/grpcpp.h>

#include "benchmarks/common.h"
#include "benchmarks/zipfian_generator.h"

#include "getting_started/roguedb.grpc.pb.h"
#include "getting_started/test.pb.h"
#include "protos/dummy.pb.h"


const std::string BENCHMARK_FILE{ "BENCHMARKS.md" };

void subscribe(const std::string& ipAddress)
{
    std::unique_ptr<rogue::services::RogueDB::Stub> stub{ rogue::services::RogueDB::NewStub(
        grpc::CreateChannel(std::format("{}:80", ipAddress), grpc::InsecureChannelCredentials()))};
    
    grpc::ClientContext context{};
    rogue::services::Response response{};
    grpc::Status status{ stub->subscribe(&context, rogue::benchmarks::createSubscribe(), &response) };
    std::cout << "Configuration::subscribe status - code: " << status.error_code() << ", message: " << status.error_message() << ", details: " << status.error_details() << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

void complete(const std::string& ipAddress)
{
    std::unique_ptr<rogue::services::RogueDB::Stub> stub{ rogue::services::RogueDB::NewStub(
        grpc::CreateChannel(std::format("{}:80", ipAddress), grpc::InsecureChannelCredentials()))};
    
    grpc::ClientContext context{};
    rogue::services::Response response{};
    rogue::services::Insert insert{};
    insert.set_api_key(rogue::benchmarks::API_KEY);
    grpc::Status status{ stub->complete(&context, insert, &response) };
    if(!status.ok())
    {
        std::cout << "RogueDB::complete status - code: " << status.error_code() << ", message: " << status.error_message() << ", details: " << status.error_details() << std::endl;
        throw std::runtime_error{ "Error in call to completion." };
    }
}

void initialData(const std::string& ipAddress)
{
    // 5M rows of data (eg. 2.5GBs)
    std::unique_ptr<rogue::services::RogueDB::Stub> stub{ rogue::services::RogueDB::NewStub(
        grpc::CreateChannel(std::format("{}:80", ipAddress), grpc::InsecureChannelCredentials()))
    };
    grpc::ClientContext context{};
    std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Insert, rogue::services::Response>> stream{
        stub->insert(&context) };

    std::cout << "Generating initial state." << std::endl;

    uint64_t count{ 0 };
    for(uint64_t round{ 1 }; round <= 5; ++round)
    {
        rogue::services::Insert insert{};
        insert.set_api_key(rogue::benchmarks::API_KEY);
        for(; count < round * 1000000;)
        {
            rogue::benchmarks::Dummy dummy{};
            dummy.set_id(count++);
            dummy.set_field1(rogue::benchmarks::BYTES_50);
            dummy.set_field2(rogue::benchmarks::BYTES_50);
            dummy.set_field3(rogue::benchmarks::BYTES_50);
            dummy.set_field4(rogue::benchmarks::BYTES_50);
            dummy.set_field5(rogue::benchmarks::BYTES_50);
            dummy.set_field6(rogue::benchmarks::BYTES_50);
            dummy.set_field7(rogue::benchmarks::BYTES_50);
            dummy.set_field8(rogue::benchmarks::BYTES_50);
            dummy.set_field9(rogue::benchmarks::BYTES_50);
            dummy.set_field10(rogue::benchmarks::BYTES_50);
            insert.add_messages()->PackFrom(dummy);
        }
        
        if(!stream->Write(insert))
        {
            grpc::Status status{ stream->Finish() };
            std::cout << "Write stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
            throw std::runtime_error{"Stream broken"};
        }
    }
    stream->WritesDone();
    complete(ipAddress);
    std::cout << "Finished generating initial state." << std::endl;
}

void validationData(const std::string& ipAddress)
{
    // 5M rows of data (eg. 2.5GBs)
    std::unique_ptr<rogue::services::RogueDB::Stub> stub{ rogue::services::RogueDB::NewStub(
        grpc::CreateChannel(std::format("{}:80", ipAddress), grpc::InsecureChannelCredentials()))
    };
    grpc::ClientContext context{};
    std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Insert, rogue::services::Response>> stream{
        stub->insert(&context) };

    std::cout << "Generating initial state." << std::endl;

    uint64_t count{ 0 };
    for(uint64_t round{ 1 }; round <= 5; ++round)
    {
        rogue::services::Insert insert{};
        insert.set_api_key(rogue::benchmarks::API_KEY);
        for(; count < round * 1000000;)
        {
            rogue::utilities::Test dummy{};
            dummy.set_attribute1(count++);
            insert.add_messages()->PackFrom(dummy);
        }
        std::cout << "count: " << count << std::endl;
        
        if(!stream->Write(insert))
        {
            grpc::Status status{ stream->Finish() };
            std::cout << "Write stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
            throw std::runtime_error{"Stream broken"};
        }
    }
    stream->WritesDone();
    complete(ipAddress);
    std::cout << "Finished generating initial state." << std::endl;
}

void generalEvenSplit(const std::string& ipAddress)
{
    subscribe(ipAddress);
    initialData(ipAddress);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::latch latch{ 1 };
    const uint64_t operationsPerThread{ (rogue::benchmarks::TOTAL_OPERATIONS / 2) / (rogue::benchmarks::TOTAL_WORKERS / 2) };
    
    for(uint64_t index{ 0 }; index < rogue::benchmarks::TOTAL_WORKERS / 2; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, temp = index]()
            {
                grpc::ChannelArguments arguments{};
                arguments.SetInt("dummy", temp);
                std::unique_ptr<rogue::services::RogueDB::Stub> readerStub{ rogue::services::RogueDB::NewStub(
                    grpc::CreateCustomChannel(
                        std::format("{}:80", ipAddress), 
                        grpc::InsecureChannelCredentials(),
                        arguments)) };
                
                grpc::ClientContext readerContext{};
                std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
                    readerStub->search(&readerContext) };
                std::random_device randomizer{};
                std::mt19937 generator{ randomizer() };
                
                rogue::services::Search search{};
                search.set_api_key(rogue::benchmarks::API_KEY);
                rogue::services::Query& query{ *search.add_queries() };
                rogue::services::Basic& expression{ *query.mutable_basic() };
                expression.set_logical_operator(rogue::services::LogicalOperator::AND);
                expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
                expression.add_operands();

                latch.wait();
                std::thread consumer{
                    [&stream]()
                    {
                        rogue::services::Response readResponse{};
                        while(stream->Read(&readResponse))
                        {}
                        grpc::Status status{ stream->Finish() };
                        if(!status.ok())
                        {
                            std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                            throw std::runtime_error{"Could not recover."};
                        }
                    }
                };

                rogue::benchmarks::Dummy dummy{};
                rogue::utilities::ZipfianGenerator zipfian{rogue::benchmarks::STARTING_DATA, .9};
                rogue::concepts::Generator<uint64_t> zipfianGenerator{ zipfian.generate(generator) };

                for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
                {
                    dummy.set_id(zipfianGenerator());
                    search.mutable_queries(0)->mutable_basic()->mutable_operands(0)->PackFrom(dummy);
                    stream->Write(search);
                }
                stream->WritesDone();
                consumer.join();
                std::cout << "finished reads" << std::endl;
            });
    }
    
    for(uint64_t index{ 0 }; index < rogue::benchmarks::TOTAL_WORKERS / 2; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, temp = index]()
            {
                grpc::ChannelArguments arguments{};
                arguments.SetInt("dummy", temp);
                std::unique_ptr<rogue::services::RogueDB::Stub> writeStub{ rogue::services::RogueDB::NewStub(
                    grpc::CreateCustomChannel(
                        std::format("{}:80", ipAddress), 
                        grpc::InsecureChannelCredentials(),
                        arguments)) };
                
                grpc::ClientContext writeContext{};
                rogue::services::Response writeResponse{};
                std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Insert, rogue::services::Response>> stream{ 
                    writeStub->insert(&writeContext) };
                
                rogue::services::Insert insert{};
                insert.set_api_key(rogue::benchmarks::API_KEY);
                insert.add_messages();
                
                rogue::benchmarks::Dummy dummy{};
                dummy.set_field1(rogue::benchmarks::BYTES_50);
                dummy.set_field2(rogue::benchmarks::BYTES_50);
                dummy.set_field3(rogue::benchmarks::BYTES_50);
                dummy.set_field4(rogue::benchmarks::BYTES_50);
                dummy.set_field5(rogue::benchmarks::BYTES_50);
                dummy.set_field6(rogue::benchmarks::BYTES_50);
                dummy.set_field7(rogue::benchmarks::BYTES_50);
                dummy.set_field8(rogue::benchmarks::BYTES_50);
                dummy.set_field9(rogue::benchmarks::BYTES_50);
                dummy.set_field10(rogue::benchmarks::BYTES_50);

                latch.wait();
                const uint64_t adjusted{ rogue::benchmarks::STARTING_DATA + (temp * operationsPerThread) };
                for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
                {
                    dummy.set_id(count + adjusted);
                    insert.mutable_messages(0)->PackFrom(dummy);
                    if(!stream->Write(insert))
                    {
                        grpc::Status status{ stream->Finish() };
                        std::cout << "Write stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                        throw std::runtime_error{"Could not recover."};
                    }
                }
                stream->WritesDone();
                std::cout << "finished writes" << std::endl;
            });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto start{ std::chrono::high_resolution_clock::now() };
    latch.count_down();
    rogue::benchmarks::threadpool.wait();
    const auto finish{ std::chrono::high_resolution_clock::now() };

    rogue::benchmarks::logBenchmark(BENCHMARK_FILE, 
        "General Read:Write 50:50", start, finish, 
        operationsPerThread * rogue::benchmarks::TOTAL_WORKERS / 2, 
        operationsPerThread * rogue::benchmarks::TOTAL_WORKERS / 2);
}

void readOnlyBulk(const std::string& ipAddress, const uint64_t batchSize)
{
    subscribe(ipAddress);
    initialData(ipAddress);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::latch latch{ 1 };
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS / rogue::benchmarks::TOTAL_WORKERS };
    
    for(uint64_t index{ 0 }; index < rogue::benchmarks::TOTAL_WORKERS; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, temp = index]()
            {
                grpc::ChannelArguments arguments{};
                arguments.SetInt("dummy", temp);
                std::unique_ptr<rogue::services::RogueDB::Stub> readerStub{ rogue::services::RogueDB::NewStub(
                    grpc::CreateCustomChannel(
                        std::format("{}:80", ipAddress), 
                        grpc::InsecureChannelCredentials(),
                        arguments)) };
    
                grpc::ClientContext readerContext{};
                std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
                    readerStub->search(&readerContext) };
                std::random_device randomizer{};
                std::mt19937 generator{ randomizer() };
                
                rogue::services::Search search{};
                search.set_api_key(rogue::benchmarks::API_KEY);
                
                for(uint32_t count{ 0 }; count < batchSize; ++count)
                {
                    rogue::services::Query* query{ search.add_queries() };
                    rogue::services::Basic& expression{ *query->mutable_basic() };
                    expression.set_logical_operator(rogue::services::LogicalOperator::AND);
                    expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
                    expression.add_operands();
                }
    
                rogue::benchmarks::Dummy dummy{};
                rogue::utilities::ZipfianGenerator zipfian{rogue::benchmarks::STARTING_DATA, .9};
                rogue::concepts::Generator<uint64_t> zipfianGenerator{ zipfian.generate(generator) };
                
                latch.wait();

                std::thread consumer{
                    [&stream]()
                    {
                        rogue::services::Response readResponse{};
                        while(stream->Read(&readResponse))
                        {}
                        grpc::Status status{ stream->Finish() };
                        if(!status.ok())
                        {
                            std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                            throw std::runtime_error{"Could not recover."};
                        }
                    }
                };
                for(uint64_t count{ 0 }; count < operationsPerThread;)
                {
                    for(uint64_t inner{ 0 }; inner < batchSize; ++inner, ++count)
                    {
                        dummy.set_id(zipfianGenerator());
                        search.mutable_queries(inner)->mutable_basic()->mutable_operands(0)->PackFrom(dummy);
                    }

                    stream->Write(search);
                }

                stream->WritesDone();
                std::cout << "finished searches" << std::endl;
                consumer.join();
            });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto start{ std::chrono::high_resolution_clock::now() };
    latch.count_down();
    rogue::benchmarks::threadpool.wait();
    const auto finish{ std::chrono::high_resolution_clock::now() };

    rogue::benchmarks::logBenchmark(BENCHMARK_FILE, std::format("Read Only Bulk {}", batchSize), start, finish, operationsPerThread * rogue::benchmarks::TOTAL_WORKERS, 0);
}

void writeOnlyBulk(const std::string& ipAddress, const uint64_t batchSize)
{
    subscribe(ipAddress);
    initialData(ipAddress);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::latch latch{ 1 };
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS / rogue::benchmarks::TOTAL_WORKERS };
    
    for(uint64_t index{ 0 }; index < rogue::benchmarks::TOTAL_WORKERS; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, temp = index]()
            {
                grpc::ChannelArguments arguments{};
                arguments.SetInt("dummy", temp);
                std::unique_ptr<rogue::services::RogueDB::Stub> writeStub{ rogue::services::RogueDB::NewStub(
                    grpc::CreateCustomChannel(
                        std::format("{}:80", ipAddress), 
                        grpc::InsecureChannelCredentials(),
                        arguments)) };

                grpc::ClientContext writeContext{};
                std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Insert, rogue::services::Response>> stream{ 
                    writeStub->insert(&writeContext) };
                
                rogue::benchmarks::Dummy dummy{};
                dummy.set_field1(rogue::benchmarks::BYTES_50);
                dummy.set_field2(rogue::benchmarks::BYTES_50);
                dummy.set_field3(rogue::benchmarks::BYTES_50);
                dummy.set_field4(rogue::benchmarks::BYTES_50);
                dummy.set_field5(rogue::benchmarks::BYTES_50);
                dummy.set_field6(rogue::benchmarks::BYTES_50);
                dummy.set_field7(rogue::benchmarks::BYTES_50);
                dummy.set_field8(rogue::benchmarks::BYTES_50);
                dummy.set_field9(rogue::benchmarks::BYTES_50);
                dummy.set_field10(rogue::benchmarks::BYTES_50);

                rogue::services::Insert insert{};
                insert.set_api_key(rogue::benchmarks::API_KEY);
                insert.mutable_messages()->Reserve(batchSize);
                for(uint64_t index{ 0 }; index < batchSize; ++index)
                {
                    insert.add_messages();
                }

                latch.wait();
                const uint64_t adjusted{ rogue::benchmarks::STARTING_DATA + (operationsPerThread * temp) };
                for(uint64_t count{ 0 }; count < operationsPerThread;)
                {
                    for(uint64_t index{ 0 }; index < batchSize; ++index, ++count)
                    {
                        dummy.set_id(count + adjusted);
                        insert.mutable_messages(index)->PackFrom(dummy);
                    }

                    if(!stream->Write(insert))
                    {
                        grpc::Status status{ stream->Finish() };
                        std::cout << "Write stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                        throw std::runtime_error{"Could not recover."};
                    }
                }
                stream->WritesDone();
                std::cout << "finished writes" << std::endl;
            });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto start{ std::chrono::high_resolution_clock::now() };
    latch.count_down();
    rogue::benchmarks::threadpool.wait();
    const auto finish{ std::chrono::high_resolution_clock::now() };

    rogue::benchmarks::logBenchmark(BENCHMARK_FILE, std::format("Write Only Bulk {}", batchSize), start, finish, 0, rogue::benchmarks::TOTAL_OPERATIONS);
}

void readWriteBulk(const std::string& ipAddress, const uint64_t batchSize)
{
    subscribe(ipAddress);
    initialData(ipAddress);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::latch latch{ 1 };
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS / rogue::benchmarks::TOTAL_WORKERS };

    for(uint64_t index{ 0 }; index < rogue::benchmarks::TOTAL_WORKERS / 2; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, temp = index]()
            {
                grpc::ChannelArguments arguments{};
                arguments.SetInt("dummy", temp);
                std::unique_ptr<rogue::services::RogueDB::Stub> writeStub{ rogue::services::RogueDB::NewStub(
                    grpc::CreateCustomChannel(
                        std::format("{}:80", ipAddress), 
                        grpc::InsecureChannelCredentials(),
                        arguments)) };

                grpc::ClientContext writeContext{};
                std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Insert, rogue::services::Response>> stream{ 
                    writeStub->insert(&writeContext) };
                
                rogue::benchmarks::Dummy dummy{};
                dummy.set_field1(rogue::benchmarks::BYTES_50);
                dummy.set_field2(rogue::benchmarks::BYTES_50);
                dummy.set_field3(rogue::benchmarks::BYTES_50);
                dummy.set_field4(rogue::benchmarks::BYTES_50);
                dummy.set_field5(rogue::benchmarks::BYTES_50);
                dummy.set_field6(rogue::benchmarks::BYTES_50);
                dummy.set_field7(rogue::benchmarks::BYTES_50);
                dummy.set_field8(rogue::benchmarks::BYTES_50);
                dummy.set_field9(rogue::benchmarks::BYTES_50);
                dummy.set_field10(rogue::benchmarks::BYTES_50);

                rogue::services::Insert insert{};
                insert.set_api_key(rogue::benchmarks::API_KEY);
                insert.mutable_messages()->Reserve(operationsPerThread);
                for(uint64_t index{ 0 }; index < operationsPerThread; ++index)
                {
                    insert.add_messages();
                }

                latch.wait();
                const uint64_t adjusted{ rogue::benchmarks::STARTING_DATA + (operationsPerThread * temp) };
                for(uint64_t count{ 0 }; count < operationsPerThread;)
                {
                    for(uint64_t index{ 0 }; index < batchSize; ++index, ++count)
                    {
                        dummy.set_id(count + adjusted);
                        insert.mutable_messages(index)->PackFrom(dummy);
                    }

                    if(!stream->Write(insert))
                    {
                        grpc::Status status{ stream->Finish() };
                        std::cout << "Write stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                        throw std::runtime_error{"Could not recover."};
                    }
                }
                stream->WritesDone();
                std::cout << "finished writes" << std::endl;
            });
    }

    for(uint64_t index{ 0 }; index < rogue::benchmarks::TOTAL_WORKERS / 2; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, temp = index]()
            {
                grpc::ChannelArguments arguments{};
                arguments.SetInt("dummy", temp);
                std::unique_ptr<rogue::services::RogueDB::Stub> readerStub{ rogue::services::RogueDB::NewStub(
                    grpc::CreateCustomChannel(
                        std::format("{}:80", ipAddress), 
                        grpc::InsecureChannelCredentials(),
                        arguments)) };
                
                grpc::ClientContext readerContext{};
                std::shared_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
                    readerStub->search(&readerContext) };
                std::random_device randomizer{};
                std::mt19937 generator{ randomizer() };
                
                rogue::services::Search search{};
                search.set_api_key(rogue::benchmarks::API_KEY);
                
                for(uint64_t count{ 0 }; count < operationsPerThread; ++count)
                {
                    rogue::services::Query* query{ search.add_queries() };
                    rogue::services::Basic& expression{ *query->mutable_basic() };
                    expression.set_logical_operator(rogue::services::LogicalOperator::AND);
                    expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
                    expression.add_operands();
                }

                rogue::benchmarks::Dummy dummy{};
                
                rogue::utilities::ZipfianGenerator zipfian{rogue::benchmarks::STARTING_DATA, .9};
                rogue::concepts::Generator<uint64_t> zipfianGenerator{ zipfian.generate(generator) };
                
                latch.wait();
                std::thread consumer{
                    [&stream]()
                    {
                        rogue::services::Response readResponse{};
                        while(stream->Read(&readResponse))
                        {}
                        grpc::Status status{ stream->Finish() };
                        if(!status.ok())
                        {
                            std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                            throw std::runtime_error{"Could not recover."};
                        }
                    }
                };

                for(uint64_t count{ 0 }; count < operationsPerThread;)
                {
                    for(uint64_t inner{ 0 }; inner < batchSize; ++inner, ++count)
                    {
                        dummy.set_id(zipfianGenerator());
                        search.mutable_queries(inner)->mutable_basic()->mutable_operands(0)->PackFrom(dummy);
                    }

                    stream->Write(search);
                }

                stream->WritesDone();
                std::cout << "finished searches" << std::endl;
                consumer.join();
            });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto start{ std::chrono::high_resolution_clock::now() };
    latch.count_down();
    rogue::benchmarks::threadpool.wait();
    const auto finish{ std::chrono::high_resolution_clock::now() };

    rogue::benchmarks::logBenchmark(BENCHMARK_FILE, "Even Batch Split", start, finish, operationsPerThread * rogue::benchmarks::TOTAL_WORKERS, operationsPerThread * rogue::benchmarks::TOTAL_WORKERS);
}

void dualMessageBulk(const std::string& ipAddress, const uint64_t batchSize)
{
    subscribe(ipAddress);
    initialData(ipAddress);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::latch latch{ 1 };
    const uint64_t operationsPerThread{ rogue::benchmarks::TOTAL_OPERATIONS / rogue::benchmarks::TOTAL_WORKERS };

    for(uint64_t index{ 0 }; index < rogue::benchmarks::TOTAL_WORKERS / 4; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, temp = index]()
            {
                grpc::ChannelArguments arguments{};
                arguments.SetInt("dummy", temp);
                std::unique_ptr<rogue::services::RogueDB::Stub> writeStub{ rogue::services::RogueDB::NewStub(
                    grpc::CreateCustomChannel(
                        std::format("{}:80", ipAddress), 
                        grpc::InsecureChannelCredentials(),
                        arguments)) };

                grpc::ClientContext writeContext{};
                std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Insert, rogue::services::Response>> stream{ 
                    writeStub->insert(&writeContext) };
                
                rogue::benchmarks::Dummy dummy{};
                dummy.set_field1(rogue::benchmarks::BYTES_50);
                dummy.set_field2(rogue::benchmarks::BYTES_50);
                dummy.set_field3(rogue::benchmarks::BYTES_50);
                dummy.set_field4(rogue::benchmarks::BYTES_50);
                dummy.set_field5(rogue::benchmarks::BYTES_50);
                dummy.set_field6(rogue::benchmarks::BYTES_50);
                dummy.set_field7(rogue::benchmarks::BYTES_50);
                dummy.set_field8(rogue::benchmarks::BYTES_50);
                dummy.set_field9(rogue::benchmarks::BYTES_50);
                dummy.set_field10(rogue::benchmarks::BYTES_50);

                rogue::services::Insert insert{};
                insert.set_api_key(rogue::benchmarks::API_KEY);
                insert.mutable_messages()->Reserve(batchSize);
                for(uint64_t index{ 0 }; index < batchSize; ++index)
                {
                    insert.add_messages();
                }

                latch.wait();
                const uint64_t adjusted{ rogue::benchmarks::STARTING_DATA + (temp * operationsPerThread) };
                for(uint64_t count{ 0 }; count < operationsPerThread;)
                {
                    for(uint64_t index{ 0 }; index < batchSize; ++index, ++count)
                    {
                        dummy.set_id(count + adjusted);
                        insert.mutable_messages(index)->PackFrom(dummy);
                    }

                    if(!stream->Write(insert))
                    {
                        grpc::Status status{ stream->Finish() };
                        std::cout << "Write stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                        throw std::runtime_error{"Could not recover."};
                    }
                }
                stream->WritesDone();
                std::cout << "finished writes" << std::endl;
            });
    }

    for(uint64_t index{ 0 }; index < rogue::benchmarks::TOTAL_WORKERS / 4; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, temp = index]()
            {
                grpc::ChannelArguments arguments{};
                arguments.SetInt("test", temp);
                std::unique_ptr<rogue::services::RogueDB::Stub> writeStub{ rogue::services::RogueDB::NewStub(
                    grpc::CreateCustomChannel(
                        std::format("{}:80", ipAddress), 
                        grpc::InsecureChannelCredentials(),
                        arguments)) };

                grpc::ClientContext writeContext{};
                std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Insert, rogue::services::Response>> stream{ 
                    writeStub->insert(&writeContext) };
                
                rogue::utilities::Test dummy{};
                dummy.set_attribute1(0);
                dummy.set_attribute2(0);
                dummy.set_attribute3(false);

                rogue::services::Insert insert{};
                insert.set_api_key(rogue::benchmarks::API_KEY);
                insert.mutable_messages()->Reserve(batchSize);
                for(uint64_t index{ 0 }; index < batchSize; ++index)
                {
                    insert.add_messages();
                }

                latch.wait();
                const uint64_t adjusted{ rogue::benchmarks::STARTING_DATA + (temp * operationsPerThread) };
                for(uint64_t count{ 0 }; count < operationsPerThread;)
                {
                    for(uint64_t index{ 0 }; index < batchSize; ++index, ++count)
                    {
                        dummy.set_attribute1(count + adjusted);
                        insert.mutable_messages(index)->PackFrom(dummy);
                    }

                    if(!stream->Write(insert))
                    {
                        grpc::Status status{ stream->Finish() };
                        std::cout << "Write stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                        throw std::runtime_error{"Could not recover."};
                    }
                }
                stream->WritesDone();
                std::cout << "finished writes" << std::endl;
            });
    }
    
    for(uint64_t index{ 0 }; index < rogue::benchmarks::TOTAL_WORKERS / 4; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, temp = index]()
            {
                grpc::ChannelArguments arguments{};
                arguments.SetInt("dummy", temp);
                std::unique_ptr<rogue::services::RogueDB::Stub> readerStub{ rogue::services::RogueDB::NewStub(
                    grpc::CreateCustomChannel(
                        std::format("{}:80", ipAddress), 
                        grpc::InsecureChannelCredentials(),
                        arguments)) };
                
                grpc::ClientContext readerContext{};
                std::shared_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
                    readerStub->search(&readerContext) };
                std::random_device randomizer{};
                std::mt19937 generator{ randomizer() };
                
                rogue::services::Search search{};
                search.set_api_key(rogue::benchmarks::API_KEY);
                
                for(uint32_t count{ 0 }; count < batchSize; ++count)
                {
                    rogue::services::Query* query{ search.add_queries() };
                    rogue::services::Basic& expression{ *query->mutable_basic() };
                    expression.set_logical_operator(rogue::services::LogicalOperator::AND);
                    expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
                    expression.add_operands();
                }
    
                rogue::benchmarks::Dummy dummy{};
                
                rogue::utilities::ZipfianGenerator zipfian{rogue::benchmarks::STARTING_DATA, .9};
                rogue::concepts::Generator<uint64_t> zipfianGenerator{ zipfian.generate(generator) };
                
                latch.wait();
                std::thread consumer{
                    [&stream]()
                    {
                        rogue::services::Response readResponse{};
                        while(stream->Read(&readResponse))
                        {}
                        grpc::Status status{ stream->Finish() };
                        if(!status.ok())
                        {
                            std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                            throw std::runtime_error{"Could not recover."};
                        }
                    }
                };
                for(uint64_t count{ 0 }; count < operationsPerThread;)
                {
                    for(uint64_t inner{ 0 }; inner < batchSize && count < operationsPerThread; ++inner, ++count)
                    {
                        dummy.set_id(zipfianGenerator());
                        search.mutable_queries(inner)->mutable_basic()->mutable_operands(0)->PackFrom(dummy);
                    }
                    stream->Write(search);
                }

                stream->WritesDone();
                consumer.join();
                std::cout << "finished reads" << std::endl;
            });
    }
    
    for(uint64_t index{ 0 }; index < rogue::benchmarks::TOTAL_WORKERS / 4; ++index)
    {
        rogue::benchmarks::threadpool.detach_task(
            [&, temp = index]()
            {
                grpc::ChannelArguments arguments{};
                arguments.SetInt("test", temp);
                std::unique_ptr<rogue::services::RogueDB::Stub> readerStub{ rogue::services::RogueDB::NewStub(
                    grpc::CreateCustomChannel(
                        std::format("{}:80", ipAddress), 
                        grpc::InsecureChannelCredentials(),
                        arguments)) };

                grpc::ClientContext readerContext{};
                std::shared_ptr<grpc::ClientReaderWriter<rogue::services::Search, rogue::services::Response>> stream{
                    readerStub->search(&readerContext) };
                std::random_device randomizer{};
                std::mt19937 generator{ randomizer() };
                
                rogue::services::Search search{};
                search.set_api_key(rogue::benchmarks::API_KEY);
                
                for(uint32_t count{ 0 }; count < batchSize; ++count)
                {
                    rogue::services::Query* query{ search.add_queries() };
                    rogue::services::Basic& expression{ *query->mutable_basic() };
                    expression.set_logical_operator(rogue::services::LogicalOperator::AND);
                    expression.add_comparisons(rogue::services::ComparisonOperator::EQUAL);
                    expression.add_operands();
                }
    
                rogue::utilities::Test dummy{};
                dummy.set_attribute2(0);
                dummy.set_attribute3(false);
                
                rogue::utilities::ZipfianGenerator zipfian{rogue::benchmarks::STARTING_DATA, .9};
                rogue::concepts::Generator<uint64_t> zipfianGenerator{ zipfian.generate(generator) };
                
                latch.wait();
                std::thread consumer{
                    [&stream]()
                    {
                        rogue::services::Response readResponse{};
                        while(stream->Read(&readResponse))
                        {}
                        grpc::Status status{ stream->Finish() };
                        if(!status.ok())
                        {
                            std::cout << "Read stream broken. Code: " << status.error_code() << ", Details: " << status.error_details() << ", Message: " << status.error_message() << std::endl;
                            throw std::runtime_error{"Could not recover."};
                        }
                    }
                };

                for(uint64_t count{ 0 }; count < operationsPerThread;)
                {
                    for(uint64_t inner{ 0 }; inner < batchSize && count < operationsPerThread; ++inner, ++count)
                    {
                        dummy.set_attribute1(zipfianGenerator());
                        search.mutable_queries(inner)->mutable_basic()->mutable_operands(0)->PackFrom(dummy);
                    }
                    stream->Write(search);
                }

                stream->WritesDone();
                consumer.join();
                std::cout << "finished reads" << std::endl;
            });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto start{ std::chrono::high_resolution_clock::now() };
    latch.count_down();
    rogue::benchmarks::threadpool.wait();
    const auto finish{ std::chrono::high_resolution_clock::now() };

    rogue::benchmarks::logBenchmark(BENCHMARK_FILE, 
        std::format("Dual Message Bulk {}", batchSize), 
        start, finish, 
        operationsPerThread * rogue::benchmarks::TOTAL_WORKERS / 2, 
        operationsPerThread * rogue::benchmarks::TOTAL_WORKERS / 2);
}

int main(int argc, char** argv)
{
    std::filesystem::remove(BENCHMARK_FILE);
    rogue::benchmarks::initialLog(BENCHMARK_FILE);
    const std::string ipAddress{ argv[1] };

    generalEvenSplit(ipAddress);
    
    const std::vector<uint64_t> batchSizes{ 1, 10, 100, 1000 };
    for(const auto& batchSize : batchSizes)
    {
        readOnlyBulk(ipAddress, batchSize);
        writeOnlyBulk(ipAddress, batchSize);
        dualMessageBulk(ipAddress, batchSize);
    }

    return 0;
}
