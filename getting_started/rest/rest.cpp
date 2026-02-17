#include <format>
#include <fstream>

// External dependencies. Single header files.
#include <cpr/cpr.h>
#include <jwt-cpp/jwt.h>

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
        .set_header_claim("kid", jwt::claim(PRIVATE_KEY_ID))
        .sign(jwt::algorithm::rs256{ PRIVATE_KEY }) };
}

std::vector<std::filesystem::path> detectFiles(
    const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> files{};
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
            for(auto& filename : detectFiles(entry.path()))
            {
                files.emplace_back(std::move(filename));
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

    // Insert | Update | Remove request with JSON. 
    std::string request{ "{\n\t\"api_key\": \"" };
    request.append(API_KEY);

    // @type: After '/', matches proto package and message name
    // attribute1: Field name in Test
    request.append(R"(",
  "messages": [
    {
      "@type": "type.googleapis.com/rogue.services.Test",
      "attribute1": 10,
      }
  ]
})");

    // REST call for Insert API.
    // No response given. Errors reported in status code.
    cpr::Response response = cpr::Post(
        cpr::Url{ std::format("{}/rest/insert", URL) },
        cpr::Bearer{ ENCODED_JWT },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Body{ request });

    // REST call for Update API.
    response = cpr::Patch(
        cpr::Url{ std::format("{}/rest/update", URL) },
        cpr::Bearer{ ENCODED_JWT },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Body{ request });

    // REST call for Remove API.
    response = cpr::Delete(
        cpr::Url{ std::format("{}/rest/remove", URL) },
        cpr::Bearer{ ENCODED_JWT },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Body{ request });
    
    //////////////////////////////////////
    ////////  Search API Example  ////////
    //////////////////////////////////////

    // Example of a basic index query. 
    // For Test, attribute1, attribute2, and attribute3 form the index.
    // Search Query: 
    // Test.attribute1 >= 1 and Test.attribute2 >= 1 and Test.attribute3 >= true
    // AND
    // Test.attribute1 <= 10 and Test.attribute2 <= 10 and Test.attribute3 <= true
    request = "{\n\"api_key";
    request.append(API_KEY);
    request.append(R"(",
  "queries": [
    {
      "basic": {
        "comparisons": ["GREATER_EQUAL", "LESSER_EQUAL"],
        "operands": [
          {
            "@type": "type.googleapis.com/rogue.services.Test",
            "attribute1": 1,
            "attribute2": 1,
            "attribute3": true
          },
          {
            "@type": "type.googleapis.com/rogue.services.Test",
            "attribute1": 10,
            "attribute2": 10,
            "attribute3": true
          }
        ]
      }
    }
  ]
})");

    // All search query types use this URL
    // Queries are zero-indexed. 
    // Results are mapped to results field.
    // All messages are stored in the messages field.
    response = cpr::Get(
        cpr::Url{ std::format("{}/rest/search", URL) },
        cpr::Bearer{ ENCODED_JWT },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Body{ request });

    // Example of a basic non-indexed query.
    // Search Query: Test.attribute1 < 1 and Test.attribute2 != 10
    request = "{\n\"api_key";
    request.append(API_KEY);

    // fields: // Corresponds to attribute1 and attribute2 field ids in Test
    request.append(R"(",
  "queries": [
    {
      "basic": {
        "comparisons": ["GREATER_EQUAL", "LESSER_EQUAL"],
        "fields": [1, 2],
        "operands": [
          {
            "@type": "type.googleapis.com/rogue.services.Test",
            "attribute1": 1,
          },
          {
            "@type": "type.googleapis.com/rogue.services.Test",
            "attribute1": 10,
          }
        ]
      }
    }
  ]
})");

    ///////////////////////////////////
    //// Schema Change API Example ////
    ///////////////////////////////////

    request = "{\n\"api_key";
    request.append(API_KEY);
    request.append(R"("
    "schemas": [)");

    // All proto files should be sent in a list of
    // their contents. No modifications required.
    for(const auto& file : detectFiles("absolute/path/to/protos/directory"))
    {
        std::ifstream inputFile{ file.native() };
        std::stringstream buffer{};
        buffer << inputFile.rdbuf();
        request.append(std::format("\"{}\",", buffer.str()));
    }
    request.pop_back();
    request.append("]\n}");

    // Any schemas excluded will have associated data deleted.
    // Schema change failure results in no changes applied.
    response = cpr::Post(
        cpr::Url{ std::format("{}/rest/subscribe", URL) },
        cpr::Bearer{ ENCODED_JWT },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Body{ request });
}
