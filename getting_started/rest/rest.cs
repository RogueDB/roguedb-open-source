using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using Microsoft.IdentityModel.Tokens;
using System.IdentityModel.Tokens.Jwt;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

class Program
{
    private static readonly HttpClient client = new HttpClient();

    static async Task Main(string[] args)
    {
        string url = "c-5501234567890123456789012345.roguedb.dev";
        string apiKey = "c9db50398da24e78b1f48a1bc9be6f79";
        string jwtToken = CreateJwt("/home/dev/service_account.json");

        client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", jwtToken);

        // --- Insert/Update/Remove Example ---
        var requestBody = new
        {
            api_key = apiKey,
            messages = new[] {
                new {
                    @type = "type.googleapis.com/rogue.services.Test",
                    attribute1 = 10
                }
            }
        };

        await SendRequest(HttpMethod.Post, $"https://{url}/rest/insert", requestBody);
        await SendRequest(new HttpMethod("PATCH"), $"https://{url}/rest/update", requestBody);
        await SendRequest(HttpMethod.Delete, $"https://{url}/rest/remove", requestBody);

        // --- Search API Example ---
        var searchBody = new
        {
            api_key = apiKey,
            queries = new[] {
                new {
                    basic = new {
                        comparisons = new[] { "GREATER_EQUAL", "LESSER_EQUAL" },
                        operands = new[] {
                            new { @type = "type.googleapis.com/rogue.services.Test", attribute1 = 1, attribute2 = 1, attribute3 = true },
                            new { @type = "type.googleapis.com/rogue.services.Test", attribute1 = 10, attribute2 = 10, attribute3 = true }
                        }
                    }
                }
            }
        };

        var response = await client.PostAsync($"https://{url}/rest/search", 
            new StringContent(JsonConvert.SerializeObject(searchBody), Encoding.UTF8, "application/json"));
        
        Console.WriteLine($"Search Status: {response.StatusCode} {response.ReasonPhrase}");

        // --- Schema Change Example ---
        var schemas = DetectFiles(new List<string> { "/home/dev/protos" })
                        .Select(File.ReadAllText).ToList();

        await SendRequest(HttpMethod.Post, $"https://{url}/rest/subscribe", new {
            api_key = apiKey,
            schemas = schemas
        });
    }

    static string CreateJwt(string serviceAccountPath, int expireMinutes = 60)
    {
        var keyData = JObject.Parse(File.ReadAllText(serviceAccountPath));
        string privateKeyPem = keyData["private_key"].ToString();
        string clientEmail = keyData["client_email"].ToString();
        string clientId = clientEmail.Split('@')[0];

        using var rsa = RSA.Create();
        // Modern .NET can import PEM strings directly
        rsa.ImportFromPem(privateKeyPem.ToCharArray());

        var securityKey = new RsaSecurityKey(rsa) { KeyId = keyData["private_key_id"].ToString() };
        var credentials = new SigningCredentials(securityKey, SecurityAlgorithms.RsaSha256);

        var header = new JwtHeader(credentials);
        header["typ"] = "JWT";

        var payload = new JwtPayload(
            issuer: clientEmail,
            audience: $"{clientId}.roguedb.dev",
            claims: null,
            notBefore: null,
            expires: DateTime.UtcNow.AddMinutes(expireMinutes),
            issuedAt: DateTime.UtcNow
        );
        payload["sub"] = clientEmail;

        var token = new JwtSecurityToken(header, payload);
        return new JwtSecurityTokenHandler().WriteToken(token);
    }

    static List<string> DetectFiles(List<string> directories)
    {
        var files = new List<string>();
        foreach (var dir in directories)
        {
            if (Directory.Exists(dir))
            {
                // Recursive search for .proto files
                files.AddRange(Directory.GetFiles(dir, "*.proto", SearchOption.AllDirectories));
            }
        }
        return files;
    }

    static async Task SendRequest(HttpMethod method, string uri, object body)
    {
        var request = new HttpRequestMessage(method, uri)
        {
            Content = new StringContent(JsonConvert.SerializeObject(body), Encoding.UTF8, "application/json")
        };
        var response = await client.SendAsync(request);
        Console.WriteLine($"{method} {uri}: {response.StatusCode}");
    }
}

