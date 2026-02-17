// Install deps: npm install axios jsonwebtoken
const fs = require('fs').promises;
const path = require('path');
const axios = require('axios');
const jwt = require('jsonwebtoken');

async function createJwt(serviceAccountPath, expireMinutes = 60) {
    const raw = await fs.readFile(serviceAccountPath, 'utf8');
    const keyData = JSON.parse(raw);

    const now = Math.floor(Date.now() / 1000);
    const payload = {
        iat: now,
        exp: now + expireMinutes * 60,
        iss: keyData.client_email,
        sub: keyData.client_email,
        aud: `${keyData.client_email.split('@')[0]}.roguedb.dev`, };

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
    const url = 'DATABASE_URL';
    const apiKey = 'API_KEY';
    const jwtPath = '/path/to/service_account.json';

    const token = await createJwt(jwtPath);
    const headers = {
    Authorization: `Bearer ${token}`,
    'Content-Type': 'application/json',
    };

    ////////////////////////////////////////////////////////
    ///////  Insert, Update, and Remove API Example  ///////
    ////////////////////////////////////////////////////////
  
    const requestBody = {
        api_key: apiKey,
        messages: [
            {
            // @type: After '/', matches proto package and message name
            '@type': 'type.googleapis.com/rogue.services.Test',
            // attribute1: Field name in Test
            attribute1: 10,
            },],};
    const response = await axios.post(`https://${url}/rest/insert`, requestBody, { headers }); // Insert API
    // const response = await axios.patch(`https://${url}/rest/update`, requestBody, { headers }); // Update API
    // const response = await axios.delete(`https://${url}/rest/remove`, { headers, data: requestBody }); // Remove API

    //////////////////////////////////////
    ////////  Search API Example  ////////
    //////////////////////////////////////

    // Example of a basic index query. 
    // For Test, attribute1, attribute2, and attribute3 form the index.
    // Search Query: 
    // Test.attribute1 >= 1 and Test.attribute2 >= 1 and Test.attribute3 >= true
    // AND
    // Test.attribute1 <= 10 and Test.attribute2 <= 10 and Test.attribute3 <= true
    const searchIndex = {
        api_key: apiKey,
        queries: [
        {
            basic: {
                comparisons: ['GREATER_EQUAL', 'LESSER_EQUAL'],
                operands: [
                {
                    '@type': 'type.googleapis.com/rogue.services.Test',
                    attribute1: 1,
                    attribute2: 1,
                    attribute3: true,
                },
                {
                    '@type': 'type.googleapis.com/rogue.services.Test',
                    attribute1: 10,
                    attribute2: 10,
                    attribute3: true,
                },],},},],};

    // All search query types use this URL
    const searchResp = await axios.post(`https://${url}/rest/search`, searchIndex, { headers });

    // Queries are zero-indexed. 
    // Results are mapped to results field.
    // All messages are stored in the messages field.
    const firstResults = searchResp.data?.results?.[0] ?? [];
    for (const r of firstResults) {
        continue;
    }

    // Example of a basic non-indexed query.
    // Search Query: Test.attribute1 < 1 and Test.attribute2 != 10
    const searchNonIndexed = {
        api_key: apiKey,
        queries: [
            {
                basic: {
                    comparisons: ['LESSER', 'NOT_EQUAL'],
                    fields: [1, 2], // Corresponds to attribute1 and attribute2 field ids in Test
                    operands: [
                        { '@type': 'type.googleapis.com/rogue.services.Test', attribute1: 1 },
                        { '@type': 'type.googleapis.com/rogue.services.Test', attribute2: 10 },
                    ],},},],};

    ///////////////////////////////////
    //// Schema Change API Example ////
    ///////////////////////////////////

    const protoFiles = await detectFiles(protoDirs);
    const schemas = [];
    detectFiles('/path/to/protos').forEach(file => {
        // All proto files should be sent in a list of
        // their contents. No modifications required.
        schemas.push(fs.readFileSync(file, 'utf8'));
    });
  
    // Any schemas excluded will have associated data deleted.
    // Schema change failure results in no changes applied.
    const subResp = await axios.post(
      `https://${url}/rest/subscribe`,
      { api_key: apiKey, schemas },
      { headers }
    );
}

main()