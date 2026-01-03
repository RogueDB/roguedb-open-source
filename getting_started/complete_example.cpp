#include <cpr/cpr.h>
#include <format>
#include <google/protobuf/util/json_util.h>
#include <grpcpp/grpcpp.h>

#include "protos/roguedb.grpc.pb.h"
#include "protos/queries.pb.h"
#include "protos/test.pb.h"

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
        .sign(jwt::algorithm::rs256{ PRIVATE_KEY }) };
}

int main(int argc, char** argv)
{
    // Core Information
	const std::string API_KEY{ "YOUR_API_KEY" };
    const std::string URL{ "c-[YOUR_IDENTIFIER_FIRST_28_CHARACTERS].roguedb.dev" };
    const std::string ENCODED_JWT{ createJwt() };
    
    // Example Data
    rogue::services::Test test{};
    test.set_attribute1(10);

    // gRPC Connection Examples
    std::unique_ptr<rogue::services::RogueDB::Stub> stub{ rogue::services::RogueDB::NewStub(
        grpc::CreateCustomChannel(
        std::format("{}:443", URL),
        grpc::SslCredentials(grpc::SslCredentialsOptions()))) };
    
    // Insert
    rogue::services::Insert insert{};
    insert.set_api_key(API_KEY);
    insert.add_messages()->PackFrom(test);
    {
        grpc::ClientContext context{};
        context.AddMetadata("Authorization", std::format("Bearer {}", ENCODED_JWT));
        std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Insert, rogue::services::Response>> stream{
            stub->insert(&context) };
        
        stream->Write(insert);
        stream->WritesDone();
        const grpc::Status status{ stream->Finish() };
    }
    // Update
    rogue::services::Update update{};
    update.set_api_key(API_KEY);
    update.add_messages()->PackFrom(test);
    {
        grpc::ClientContext context{};
        context.AddMetadata("Authorization", std::format("Bearer {}", ENCODED_JWT));
        std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Update, rogue::services::Response>> stream{
            stub->update(&context) };
        
        stream->Write(update);
        stream->WritesDone();
        const grpc::Status status{ stream->Finish() };
    }
    // Remove
    rogue::services::Remove remove{};
    remove.set_api_key(API_KEY);
    remove.add_messages()->PackFrom(test);
    {
        grpc::ClientContext context{};
        context.AddMetadata("Authorization", std::format("Bearer {}", ENCODED_JWT));
        std::unique_ptr<grpc::ClientReaderWriter<rogue::services::Remove, rogue::services::Response>> stream{
            stub->remove(&context) };
        
        stream->Write(remove);
        stream->WritesDone();
        const grpc::Status status{ stream->Finish() };
    }
    {
        // Search - Index Query
        rogue::services::Search search{};
        search.set_api_key(API_KEY);
            
        // Conditional: Test.attribute1 >= 1 && Test.attribute1 <= 10
        rogue::services::Basic& expression{ *search.add_queries()->mutable_basic() };
        expression.set_logical_operator(rogue::services::LogicalOperator::AND);

        Test test{};
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
                stub->search(&context) };
            
        stream->Write(search);
        stream->WritesDone(); // Signal all queries sent. No risk of blocking due to single write.
        stream->Read(&response);
    }
    {
        // Search - Non-Index Query
        rogue::services::Search search{};
        search.set_api_key(API_KEY);

        // Conditional: Test.attribute1 >= 1 && Test.attribute2 != false
        rogue::services::Basic& expression{ *search.add_queries()->mutable_basic() };
        expression.set_logical_operator(rogue::services::LogicalOperator::AND);
            
        Test test{};
        expression.add_comparisons(rogue::services::ComparisonOperator::GREATER_EQUAL);
        expression.add_fields(1);
        test.set_attribute1(1);
        (*expression.add_operands()).PackFrom(test);
            
        expression.add_comparisons(rogue::services::ComparisonOperator::NOT_EQUAL);
        expression.add_fields(2);
        test.set_attribute2(false);
        (*expression.add_operands()).PackFrom(test);

        grpc::ClientContext context{};
        rogue::services::Response response{};
        std::shared_ptr<grpc::ClientReaderWriter<
            rogue::services::Search, rogue::services::Response>> stream{
                stub->search(&context) };
            
        stream->Write(search);
        stream->WritesDone(); // Signal all queries sent. No risk of blocking due to single write.
        stream->Read(&response);
    }
    {
        // Subscribe aka Schema Change
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
        grpc::ClientContext context{};
        rogue::services::Response response{};
        const grpc::Status status{ stub->subscribe(&context, subscribe, &response) };
    }
    
    // REST Connection w/ C++ Requests
    // DISCLAIMER: RogueDB does not use C++ for internal validation with REST. Below code is a theoretical demonstration only.
    cpr::Response response{ cpr::Post(
        cpr::Url{ std::format("{}/rest/insert", URL) },
        cpr::Bearer{ ENCODED_JWT },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Body{ google::protobuf::util::MessageToJsonString(insert) } )};

    response = cpr::Patch(
        cpr::Url{ std::format("{}/rest/update", URL) },
        cpr::Bearer{ ENCODED_JWT },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Body{ google::protobuf::util::MessageToJsonString(update) } );

    response = cpr::Delete(
        cpr::Url{ std::format("{}/rest/remove", URL) },
        cpr::Bearer{ ENCODED_JWT },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Body{ google::protobuf::util::MessageToJsonString(remove) } );

    rogue::services::Search search{};
    search.set_api_key(API_KEY);
        
    // Conditional: Test.attribute1 >= 1 && Test.attribute1 <= 10
    rogue::services::Basic& expression{ *search.add_queries()->mutable_basic() };
    expression.set_logical_operator(rogue::services::LogicalOperator::AND);

    Test test{};
    expression.add_comparisons(rogue::services::ComparisonOperator::GREATER_EQUAL);
    test.set_attribute1(1);
    (*expression.add_operands()).PackFrom(test);
        
    expression.add_comparisons(rogue::services::ComparisonOperator::LESSER_EQUAL);
    test.set_attribute1(10);
    (*expression.add_operands()).PackFrom(test);

    response = cpr::Get(
        cpr::Url{ std::format("{}/rest/search", URL) },
        cpr::Bearer{ ENCODED_JWT },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Body{ google::protobuf::util::MessageToJsonString(search) } );
}