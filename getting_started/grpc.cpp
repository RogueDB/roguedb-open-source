#include <format>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <jwt-cpp/jwt.h>

#include "roguedb/roguedb.grpc.pb.h"
#include "roguedb/queries.pb.h"
#include "roguedb/test.pb.h"

std::string createJwt()
{
    // Values found in service_account.json.
    const std::string SERVICE_ACCOUNT_EMAIL{ "YOUR_SERVICE_ACCOUNT_EMAIL" };
    const std::string PRIVATE_KEY_ID{ "YOUR_PRIVATE_KEY_ID" };
    const std::string PRIVATE_KEY{ "YOUR_PRIVATE_KEY" };
    const auto now{ std::chrono::system_clock::now() };
    return std::string{ jwt::create()
        .set_issuer(SERVICE_ACCOUNT_EMAIL)
        .set_subject(SERVICE_ACCOUNT_EMAIL)
        .set_audience(std::format(
        	"{}.roguedb.dev", 
            SERVICE_ACCOUNT_EMAIL.substr(
            	0, SERVICE_ACCOUNT_EMAIL.find("@"))))
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::hours(1))
        .set_header("kid", PRIVATE_KEY_ID)
        .sign(jwt::algorithm::rs256{ PRIVATE_KEY }) };
}

std::vector<std::filesystem::path> detectFiles(
    const std::vector<std::string>& directories)
{
    std::vector<std::filesystem::path> files{};
    for(const auto& directory : directories)
    {
        for(const auto& entry : std::filesystem::directory_iterator{directory})
        {
            if(!std::filesystem::is_directory(entry))
            {
                if(entry.path().extension() == ".proto")
                {
                    files.emplace_back(std::move(entry.path()));
                }
            }
            else
            {
                std::vector<std::string> temp{ entry.path() };
                for(auto& filename : detectFiles(entry.path()))
                {
                    files.emplace_back(std::move(filename));
                }
            }
        }
    }
    return files;
}

int main(int argc, char** argv)
{
    // See purchase confirmation emails for details and service_account.json.
	const std::string API_KEY{ "YOUR_API_KEY" };
    const std::string URL{ "c-[YOUR_IDENTIFIER_FIRST_28_CHARACTERS].roguedb.dev" };
    const std::string ENCODED_JWT{ createJwt() };
    
    ////////////////////////////////////////////////////////
    ///////  Insert, Update, and Remove API Example  ///////
    ////////////////////////////////////////////////////////
    
    rogue::utilities::Test test{};
    test.set_attribute1(10);
    
    rogue::services::Insert request{}; // Insert API
    // rogue::services::Update update{}; // Update API
    // rogue::services::Remove remove{}; // Remove API
    
    request.set_api_key(API_KEY);

    // Insert, Update, and Remove are identical in use.
    request.add_messages()->PackFrom(test);
    
    // gRPC Connection
    std::unique_ptr<rogue::services::RogueDB::Stub> stub{ rogue::services::RogueDB::NewStub(
    grpc::CreateCustomChannel(
        std::format("{}:443", URL),
        grpc::SslCredentials(grpc::SslCredentialsOptions()))) };
        
    grpc::ClientContext context{};
    context.AddMetadata("Authorization", std::format("Bearer {}", ENCODED_JWT));
    auto stream{ stub->insert(&context) };
    // auto stream{ stub->update(&context) }; // Update API
    // auto stream{ stub->remove(&context) }; // Remove API

    stream->Write(request);
    stream->WritesDone(); // Signal all queries sent. Otherwise, blocks.
    
    // No response is given for Insert, Update, and Remove
    // Any errors get reported in status.
    grpc::Status status{ stream->Finish() };
    
    //////////////////////////////////////
    ////////  Search API Example  ////////
    //////////////////////////////////////

    {
        // Example of a baisc index query.
        rogue::services::Search search{};
        search.set_api_key(API_KEY);
        
        // For Test, attribute1, attribute2, and attribute3 form the index.
        // Search Query:
        // Test.attribute1 >= 1 and Test.attribute2 >= 1 and Test.attribute3 >= true
        // AND
        // Test.attribute1 <= 10 and Test.attribute2 <= 10 and Test.attribute3 <= true
        rogue::services::Basic& expression{ *search.add_queries()->mutable_basic() };
        expression.set_logical_operator(rogue::services::LogicalOperator::AND);

        test = rogue::utilities::Test{};
        expression.add_comparisons(rogue::services::ComparisonOperator::GREATER_EQUAL);
        test.set_attribute1(1);
        (*expression.add_operands()).PackFrom(test);
            
        expression.add_comparisons(rogue::services::ComparisonOperator::LESSER_EQUAL);
        test.set_attribute1(10);
        (*expression.add_operands()).PackFrom(test);

        grpc::ClientContext context{};
        rogue::services::Response response{};
        std::shared_ptr<grpc::ClientReaderWriter<
            rogue::services::Search, rogue::services::Response>> stream{
                roguedb->search(&context) };
            
        stream->Write(search);
        stream->WritesDone(); // Signal all queries sent. Otherwise, blocks.

        // Multiple queries or large queries should be processed on a separate thread
        // to prevent blocking the database.
        stream->Read(&response);
        status = stream->Finish(); // Signal all queries sent. Blocks otherwise.

        std::vector<rogue::utilities::Test> results{};
        // Queries are zero-indexed. Partial results get sent
        // and mapped to that index.
        for(const auto& result : response.results()[0].messages())
        {
            result.PackTo(results.emplace_back());
        }

        // Each response sends a list of the query ids finished
        // with processing.
        for(const auto& finishedId : response.finished())
        {
            if(finishedId == 0)
            {}
        }
    }
    {
        // Example of a baisc non-indexed query.
        rogue::services::Search search{};
        search.set_api_key(API_KEY);
        
        // Search Query: 
        // Test.attribute1 >= 1 && Test.attribute2 <= 10
        rogue::services::Basic& expression{ *search.add_queries()->mutable_basic() };
        expression.set_logical_operator(rogue::services::LogicalOperator::AND);

        test = rogue::utilities::Test{};
        test.set_attribute1(1);
        expression.add_fields(1); // Corresponds to field id in test.proto
        (*expression.add_operands()).PackFrom(test);
        expression.add_comparisons(rogue::services::ComparisonOperator::GREATER_EQUAL);
            
        test.set_attribute2(10);
        expression.add_fields(2); // Corresponds to field id in test.proto
        (*expression.add_operands()).PackFrom(test);
        expression.add_comparisons(rogue::services::ComparisonOperator::LESSER_EQUAL);
    }

    ///////////////////////////////////
    //// Schema Change API Example ////
    ///////////////////////////////////

    rogue::services::Subscribe subscribe{};
    subscribe.set_api_key(apiKey);

    const std::vector<std::string> directories{ 
        "absolute/path/to/protos/directory1",
        "absolute/path/to/protos/directory2" };

    // All proto files should be sent in a list of
    // their contents. No modifications required.
    for(const auto& file : detectFiles(directories))
    {
        std::ifstream inputFile{ file.native() };
        std::stringstream buffer{};
        buffer << inputFile.rdbuf();
        subscribe.add_queries(buffer.str());
    }

    // Any schemas excluded will have associated data deleted.
    // Schema change failure results in no changes applied.
    roguedb.subscribe(&context, subscribe, &response, metadata=metadata);
}