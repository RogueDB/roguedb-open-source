import fs from 'fs';
import path from 'path';
import grpc from '@grpc/grpc-js';
import jwt from 'jsonwebtoken';
import { RogueDBClient } from './getting_started/roguedb_grpc_pb.js'; 
import rogue_pb from './getting_started/roguedb_pb.js';
import google_protobuf_any_pb from 'google-protobuf/google/protobuf/any_pb.js';

const { Test, Insert, Search, Basic, LogicalOperator, ComparisonOperator, Subscribe } = rogue_pb;

async function createJwt(serviceAccountPath, expireMinutes = 60) {
  const raw = await fs.readFile(serviceAccountPath, 'utf8');
  const keyData = JSON.parse(raw);

  const now = Math.floor(Date.now() / 1000);
  const payload = {
    iat: now,
    exp: now + expireMinutes * 60,
    iss: keyData.client_email,
    sub: keyData.client_email,
    aud: `${keyData.client_email.split('@')[0]}.roguedb.dev`,
  };

  // keyid placed in header automatically if provided via options.keyid
  return jwt.sign(payload, keyData.private_key, {
    algorithm: 'RS256',
    keyid: keyData.private_key_id,
    header: { typ: 'JWT' },
  });
}

function detectFiles(dir, fileList = []) {
    const files = fs.readdirSync(dir);
    files.forEach(file => {
        const filePath = path.join(dir, file);
        if (fs.statSync(filePath).isDirectory()) {
            detectFiles(filePath, fileList);
        } else if (path.extname(file) === '.proto') {
            fileList.push(filePath);
        }
    });
    return fileList;
}

async function main() {
    // See purchase confirmation emails for details and service_account.json.
    const API_KEY = "API_KEY";
    const URL = "DATABASE_URL";
    const ENCODED_JWT = createJwt();

    const client = new RogueDBClient(
        `${URL}:443`,
        grpc.credentials.createSsl()
    );

    const metadata = new grpc.Metadata();
    metadata.add('Authorization', `Bearer ${ENCODED_JWT}`);

    ////////////////////////////////////////////////////////
    ///////  Insert, Update, and Remove API Example  ///////
    ////////////////////////////////////////////////////////

    const test = new Test();
    test.setAttribute1(10);

    const anyMessage = new google_protobuf_any_pb.Any();
    anyMessage.pack(test.serializeBinary(), 'rogue.services.Test');

    const request = new Insert();
    // const request = new Update();
    // const request = new Remove();
    request.setApiKey(API_KEY);
    request.addMessages(anyMessage);

    // No response is given for Insert, Update, and Remove
    // Any errors get reported in status.
    const stream = client.insert(metadata, (error) => { // Insert API
    // const stream = client.update(metadata, (error) => { // Update API
    // const stream = client.remove(metadata, (error) => { // Remove API
        if (error) console.error("Insert Status Error:", error);
    });
    stream.write(request);
    stream.end();

    //////////////////////////////////////
    ////////  Search API Example  ////////
    //////////////////////////////////////

    const search = new Search();
    search.setApiKey(API_KEY);
    
    // Example of a basic index query.
    // For Test, attribute1, attribute2, and attribute3 form the index.
    // Search Query:
    // Test.attribute1 >= 1 and Test.attribute2 >= 1 and Test.attribute3 >= true
    // AND
    // Test.attribute1 <= 10 and Test.attribute2 <= 10 and Test.attribute3 <= true
    const expression = new Basic();
    expression.setLogicalOperator(LogicalOperator.AND);
    expression.addComparisons(ComparisonOperator.GREATER_EQUAL);
    expression.addComparisons(ComparisonOperator.LESSER_EQUAL);
    
    const testGte = new Test();
    testGte.setAttribute1(1);
    testGte.setAttribute2(1);
    testGte.setAttribute3(true);
    
    const gteAny = new google_protobuf_any_pb.Any();
    gteAny.pack(testGte.serializeBinary(), 'rogue.services.Test');
    expression.addOperands(gteAny);

    const testLte = new Test();
    testLte.setAttribute1(10);
    testLte.setAttribute2(10);
    testLte.setAttribute3(true);
    
    const lteAny = new google_protobuf_any_pb.Any();
    lteAny.pack(testLte.serializeBinary(), 'rogue.services.Test');
    expression.addOperands(lteAny);

    search.addQueries().setBasic(expression);

    const searchStream = client.search(metadata);
    searchStream.on('data', (response) => {
        // Queries are zero-indexed. Partial results get sent
        // and mapped to that index.
        const results = response.getResultsList()[0].getMessagesList();
        results.forEach(resAny => {
            const resultTest = resAny.unpack(Test.deserializeBinary, 'rogue.services.Test');
            console.log("Unpacked Attribute1:", resultTest.getAttribute1());
        });
    });
    searchStream.write(search);
    searchStream.end(); // Signal all queries sent. Otherwise, blocks.

    // Example of a basic non-indexed query.
    // Search Query: 
    // Test.attribute1 >= 1 && Test.attribute2 <= 10
    const expression2 = new Basic();
    expression2.setLogicalOperator(LogicalOperator.AND);
    expression2.addComparisons(ComparisonOperator.GREATER_EQUAL);
    expression2.addComparisons(ComparisonOperator.LESSER_EQUAL);
    expression2.addFields(1); // Corresponds to attribute1 field id in Test
    expression2.addFields(2); // Corresponds to attribute2 field id in Test
    
    const testGte2 = new Test();
    testGte2.setAttribute1(1);
    
    const gteAny2 = new google_protobuf_any_pb.Any();
    gteAny2.pack(testGte2.serializeBinary(), 'rogue.services.Test');
    expression2.addOperands(gteAny2);

    const testLte2 = new Test();
    testLte2.setAttribute1(10);
    
    const lteAny2 = new google_protobuf_any_pb.Any();
    lteAny2.pack(testLte2.serializeBinary(), 'rogue.services.Test');
    expression2.addOperands(lteAny2);

    search.addQueries().setBasic(expression2);

    ///////////////////////////////////
    //// Schema Change API Example ////
    ///////////////////////////////////

    const subscribe = new Subscribe();
    subscribe.setApiKey(API_KEY);
    detectFiles('/path/to/protos').forEach(file => {
        // All proto files should be sent in a list of
        // their contents. No modifications required.
        subscribe.addSchemas(fs.readFileSync(file, 'utf8'));
    });

    // Any schemas excluded will have associated data deleted.
    // Schema change failure results in no changes applied.
    client.subscribe(subscribe, metadata, (err, response) => {
        if (!err) console.log("Schema updated successfully");
    });
}

main();