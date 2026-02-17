import com.auth0.jwt.JWT;
import com.auth0.jwt.algorithms.Algorithm;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import com.fasterxml.jackson.databind.node.ArrayNode;
import com.fasterxml.jackson.core.type.TypeReference;

import java.nio.file.*;
import java.net.URI;
import java.net.http.*;
import java.security.KeyFactory;
import java.security.interfaces.RSAPrivateKey;
import java.security.spec.PKCS8EncodedKeySpec;
import java.time.Instant;
import java.util.*;
import java.util.stream.Collectors;

public class rest {
    private static final ObjectMapper mapper = new ObjectMapper();
    private static final HttpClient httpClient = HttpClient.newHttpClient();

    public static String createJwt(String serviceAccountPath) throws Exception {
        byte[] jsonData = Files.readAllBytes(Paths.get(serviceAccountPath));
        var keyData = mapper.readTree(jsonData);

        String privateKeyContent = keyData.get("private_key").asText()
                .replace("-----BEGIN PRIVATE KEY-----", "")
                .replace("-----END PRIVATE KEY-----", "")
                .replaceAll("\\s", "");

        byte[] encoded = Base64.getDecoder().decode(privateKeyContent);
        PKCS8EncodedKeySpec keySpec = new PKCS8EncodedKeySpec(encoded);
        RSAPrivateKey privateKey = (RSAPrivateKey) KeyFactory.getInstance("RSA").generatePrivate(keySpec);

        Algorithm algorithm = Algorithm.RSA256(null, privateKey);
        String email = keyData.get("client_email").asText();
        String aud = email.split("@")[0] + ".roguedb.dev";

        return JWT.create()
                .withKeyId(keyData.get("private_key_id").asText())
                .withIssuer(email)
                .withSubject(email)
                .withAudience(aud)
                .withIssuedAt(Instant.now())
                .withExpiresAt(Instant.now().plusSeconds(3600))
                .sign(algorithm);
    }

    public static List<String> detectFiles(Path dir) throws Exception {
        try (var stream = Files.walk(dir)) {
            return stream
                .filter(p -> p.toString().endsWith(".proto"))
                .map(p -> {
                    try { return Files.readString(p); } 
                    catch (Exception e) { return ""; }
                })
                .collect(Collectors.toList());
        }
    }

    public static void main(String[] args) throws Exception {
    	// See purchase confirmation emails for details and service_account.json.
        String url = "DATABASE_URL";
        String apiKey = "API_KEY";
        String token = createJwt("/path/to/service_account.json");

        ////////////////////////////////////////////////
        //// Insert, Update, and Remove API Example ////
        ////////////////////////////////////////////////

        // Creating an Insert, Update, or Remove API with JSON. 
        ObjectNode requestBody = mapper.createObjectNode();
        requestBody.put("api_key", apiKey);
        var messages = requestBody.putArray("messages");
        var testMsg = messages.addObject();
        
        // @type: After '/', matches proto package and message name
        testMsg.put("@type", "type.googleapis.com/rogue.services.Test");
        // attribute1: Field name in Test
        testMsg.put("attribute1", 10);

        // REST call for Insert API.
        HttpRequest request = HttpRequest.newBuilder()
            .header("Authorization", "Bearer " + token)
            .header("Content-Type", "application/json")
            .uri(URI.create("https://" + url + "/rest/insert"))
            // .uri(URI.create("https://" + url + "/rest/update")) // REST call for Update API.
            // .uri(URI.create("https://" + url + "/rest/remove")) // REST call for Remove API
            .POST(HttpRequest.BodyPublishers.ofString(requestBody.toString()))
            // .PATCH(HttpRequest.BodyPublishers.ofString(requestBody.toString())) // REST call for Update API
            // .DELETE(HttpRequest.BodyPublishers.ofString(requestBody.toString())) // REST call for Remove API
            .build();
        
        // No response given. Errors reported in status code.
        HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
        
        ///////////////////////////////
        ///// Search API Examples /////
        ///////////////////////////////
        
        // Example of a basic index query. 
        // For Test, attribute1, attribute2, and attribute3 form the index.
        // Search Query: 
        // Test.attribute1 >= 1 and Test.attribute2 >= 1 and Test.attribute3 >= true
        // AND
        // Test.attribute1 <= 10 and Test.attribute2 <= 10 and Test.attribute3 <= true
        requestBody = mapper.createObjectNode();
        requestBody.put("api_key", apiKey);
        ArrayNode queries = requestBody.putArray("queries");
        ObjectNode query = queries.addObject();
        ObjectNode basic = query.putObject("basic");

        ArrayNode comparisons = basic.putArray("comparisons");
        comparisons.add("GREATER_EQUAL");
        comparisons.add("LESSER_EQUAL");

        ArrayNode operands = basic.putArray("operands");
        ObjectNode operand1 = mapper.createObjectNode();
        
        operand1.put("@type", "type.googleapis.com/rogue.services.Test");
        operand1.put("attribute1", 1);
        operand1.put("attribute2", 1);
        operand1.put("attribute3", true);
        operands.add(operand1);

        ObjectNode operand2 = mapper.createObjectNode();          
        operand2.put("@type", "type.googleapis.com/rogue.services.Test");
        operand2.put("attribute1", 10);
        operand2.put("attribute2", 10);
        operand2.put("attribute3", true);
        operands.add(operand2);

        // All search query types use this URL
        request = HttpRequest.newBuilder()
            .header("Authorization", "Bearer " + token)
            .header("Content-Type", "application/json")
            .uri(URI.create("https://" + url + "/rest/search"))
            .method("GET", HttpRequest.BodyPublishers.ofString(requestBody.toString()))
            .build();

        var root = mapper.readTree(response.body());
        var results = mapper.convertValue(
            root.get("results"), 
            new TypeReference<Map<String, List<Object>>>() {});;

        // Example of a basic non-indexed query.
        // Search Query: Test.attribute1 < 1 Test.attribute2 != 10
        requestBody.put("api_key", apiKey); // ensure api_key still present
        queries = requestBody.putArray("queries");
        query = queries.addObject();
        basic = query.putObject("basic");

        comparisons = basic.putArray("comparisons");
        comparisons.add("LESSER");
        comparisons.add("NOT_EQUAL");

        ArrayNode fields = basic.putArray("fields");
        fields.add(1);
        fields.add(2);

        operands = basic.putArray("operands");
        operand1 = mapper.createObjectNode();
        operand1.put("@type", "type.googleapis.com/rogue.services.Test");
        operand1.put("attribute1", 1);
        operands.add(operand1);

        operand2 = mapper.createObjectNode();
        operand2.put("@type", "type.googleapis.com/rogue.services.Test");
        operand2.put("attribute2", 10);
        operands.add(operand2);

        /////////////////////////////////////
        ///// Schema Change API Example /////
        /////////////////////////////////////
        
        requestBody = mapper.createObjectNode();
        requestBody.put("api_key", apiKey);
        ArrayNode schemasArray = requestBody.putArray("schemas");
        for(String file : detectFiles(Paths.get("/path/to/protos"))) {
            // Proto file definitions. No modifications required.
            schemasArray.add(Files.readString(Paths.get(file)));
        }

        request = HttpRequest.newBuilder()
            .header("Authorization", "Bearer " + token)
            .header("Content-Type", "application/json")
            .uri(URI.create("https://" + url + "/rest/subscribe"))
            .POST(HttpRequest.BodyPublishers.ofString(requestBody.toString()))
            .build();

        // Any schemas excluded will have associated data deleted.
        // Schema change failure results in no changes applied.
        response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
    }
}