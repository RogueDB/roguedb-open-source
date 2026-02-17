import com.auth0.jwt.JWT;
import com.auth0.jwt.algorithms.Algorithm;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;

import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.Path;
import java.util.List;
import java.util.Base64;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.interfaces.RSAPrivateKey;
import java.security.KeyFactory;
import java.time.Instant;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

import com.google.protobuf.Any;
import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import io.grpc.Metadata;
import io.grpc.stub.MetadataUtils;
import io.grpc.stub.StreamObserver;

import rogue.services.RogueDBGrpc;
import rogue.services.Roguedb.*;

public class grpc {
    private static final ObjectMapper mapper = new ObjectMapper();
    
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
	    final String API_KEY = "YOUR_API_KEY";
        final String URL = "c-[YOUR_IDENTIFIER].roguedb.dev";
        final String JWT_TOKEN = createJwt("/path/to/service_account.json");

        ManagedChannel channel = ManagedChannelBuilder.forAddress(URL, 443)
            .useTransportSecurity()
            .build();

        // Setup Headers
        Metadata headers = new Metadata();
        headers.put(Metadata.Key.of("Authorization", Metadata.ASCII_STRING_MARSHALLER), "Bearer " + JWT_TOKEN);
        
        // Use the Async Stub
        RogueDBGrpc.RogueDBStub asyncStub = RogueDBGrpc.newStub(channel)
            .withInterceptors(MetadataUtils.newAttachHeadersInterceptor(headers));

        ////////////////////////////////////////////////////////
        ///////  Insert, Update, and Remove API Example  ///////
        ////////////////////////////////////////////////////////
        
        final CountDownLatch finishLatch = new CountDownLatch(1);
        // Insert API
        StreamObserver<Insert> stream = asyncStub.insert(new StreamObserver<Response>() {
        // StreamObserver<Update> stream = asyncStub.update(new StreamObserver<Response>() { // Update API
        // StreamObserver<Remove> stream = asyncStub.remove(new StreamObserver<Response>() { // Remove API
            
            // No response is given for Insert, Update, and Remove
            // Any errors get reported in status.
            @Override public void onNext(Response value) { /* No-op for Insert */ }
            @Override public void onError(Throwable t) { finishLatch.countDown(); }
            @Override public void onCompleted() { finishLatch.countDown(); }
        });

        stream.onNext(Insert.newBuilder()
        // stream.onNext(Update.newBuilder() // Update API
        // stream.onNext(Remove.newBuilder() // Remove API
            .setApiKey(API_KEY)
            // Insert, Update, and Remove are identical in use.
            .addMessages(Any.pack(Test.newBuilder().setAttribute1(10).build()))
            .build());
        stream.onCompleted(); // Signal all queries sent. Otherwise, blocks.
        finishLatch.await(1, TimeUnit.MINUTES);

        //////////////////////////////////////
        ////////  Search API Example  ////////
        //////////////////////////////////////
        
        final CountDownLatch searchFinishLatch = new CountDownLatch(1);
        StreamObserver<Search> searchStream = asyncStub.search(new StreamObserver<Response>() {
            @Override
            public void onNext(Response response) {
                // Queries are zero-indexed. Partial results get sent
                // and mapped to that index.
                response.getResultsMap().get(0L).getMessagesList().forEach(any -> {
                    try {
                        Test t = any.unpack(Test.class);
                    } catch (Exception e) { e.printStackTrace(); }
                });
            }
            @Override public void onError(Throwable t) { searchFinishLatch.countDown(); }
            @Override public void onCompleted() { searchFinishLatch.countDown(); }
        });

        // Example of a basic index query.
        // For Test, attribute1, attribute2, and attribute3 form the index.
        // Search Query:
        // Test.attribute1 >= 1 and Test.attribute2 >= 1 and Test.attribute3 >= true
        // AND
        // Test.attribute1 <= 10 and Test.attribute2 <= 10 and Test.attribute3 <= true
        searchStream.onNext(Search.newBuilder()
            .setApiKey(API_KEY)
            .addQueries(Expression.newBuilder()
                .setBasic(Basic.newBuilder()
                    .setLogicalOperator(LogicalOperator.AND)
                    .addComparisons(ComparisonOperator.GREATER_EQUAL)
                    .addOperands(Any.pack(Test.newBuilder()
                        .setAttribute1(1)
                        .setAttribute2(1)
                        .setAttribute3(true)
                        .build()))
                    .addComparisons(ComparisonOperator.LESSER_EQUAL)
                    .addOperands(Any.pack(Test.newBuilder()
                        .setAttribute1(10)
                        .setAttribute2(10)
                        .setAttribute3(true)
                        .build()))))
            .build());
        searchStream.onCompleted(); // Signal all queries sent. Otherwise, blocks.
        searchFinishLatch.await(1, TimeUnit.MINUTES);

        // Example of a basic non-indexed query.
        // Search Query: 
        // Test.attribute1 >= 1 && Test.attribute2 <= 10
        searchStream.onNext(Search.newBuilder()
            .setApiKey(API_KEY)
            .addQueries(Expression.newBuilder()
                .setBasic(Basic.newBuilder()
                    .setLogicalOperator(LogicalOperator.AND)
                    .addFields(1)
                    .addFields(2)
                    .addComparisons(ComparisonOperator.GREATER_EQUAL)
                    .addOperands(Any.pack(Test.newBuilder()
                        .setAttribute1(1)
                        .setAttribute2(1)
                        .setAttribute3(true)
                        .build()))
                    .addComparisons(ComparisonOperator.LESSER_EQUAL)
                    .addOperands(Any.pack(Test.newBuilder()
                        .setAttribute1(10)
                        .setAttribute2(10)
                        .setAttribute3(true)
                        .build()))))
            .build());

        ///////////////////////////////////
        //// Schema Change API Example ////
        ///////////////////////////////////
        
        var blockingStub = RogueDBGrpc.newBlockingStub(channel).withInterceptors(
            MetadataUtils.newAttachHeadersInterceptor(headers));

        Subscribe.Builder subscribeBuilder = Subscribe.newBuilder().setApiKey(API_KEY);
        // All proto files should be sent in a list of
        // their contents. No modifications required.
        for(String file : detectFiles(Paths.get("/path/to/protos"))) {
            subscribeBuilder.addSchemas(Files.readString(Paths.get(file)));
        }

        // Any schemas excluded will have associated data deleted.
        // Schema change failure results in no changes applied.
        Response subscribeResponse = blockingStub.subscribe(subscribeBuilder.build());
    }
}
