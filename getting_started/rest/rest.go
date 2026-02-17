package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/golang-jwt/jwt/v5"
)

func createJWT() string {
	// Values found in service_account.json
	const SERVICE_ACCOUNT_EMAIL = "YOUR_SERVICE_ACCOUNT_EMAIL"
	const PRIVATE_KEY_ID = "YOUR_PRIVATE_KEY_ID"
	const PRIVATE_KEY = "YOUR_PRIVATE_KEY"

	now := time.Now()

	// Create JWT token
	token := jwt.NewWithClaims(jwt.SigningMethodRS256, jwt.MapClaims{
		"iss": SERVICE_ACCOUNT_EMAIL,
		"sub": SERVICE_ACCOUNT_EMAIL,
		"aud": fmt.Sprintf("%s.roguedb.dev", SERVICE_ACCOUNT_EMAIL[:len(SERVICE_ACCOUNT_EMAIL)-len(SERVICE_ACCOUNT_EMAIL[strings.LastIndex(SERVICE_ACCOUNT_EMAIL, "@")+1:])]),
		"iat": now.Unix(),
		"exp": now.Add(time.Hour).Unix(),
		"header": map[string]interface{}{
			"kid": PRIVATE_KEY_ID,
		},
	})

	tokenString, err := token.SignedString([]byte(PRIVATE_KEY))
	if err != nil {
		panic(err)
	}

	return tokenString
}

func detectFiles(directories []string) []string {
	var files []string

	for _, directory := range directories {
		err := filepath.Walk(directory, func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return err
			}

			// Skip directories
			if !info.IsDir() {
				files = append(files, path)
			}

			return nil
		})

		if err != nil {
			panic(err)
		}
	}

	return files
}

func main() {
	// See purchase confirmation emails for details and service_account.json
	const API_KEY = "YOUR_API_KEY"
	const URL = "c-[YOUR_IDENTIFIER_FIRST_28_CHARACTERS].roguedb.dev"
	encodedJWT := createJWT()

	////////////////////////////////////////////////////////
	///////  Insert, Update, and Remove API Example  ///////
	////////////////////////////////////////////////////////

	// NOTE: See roguedb.proto for all API definitions.
	// Creating an Insert, Update, or Remove API with JSON.
	payload := map[string]interface{}{
		"api_key": API_KEY,
		"messages": []map[string]interface{}{
			map[string]interface{}{
				// Part after '/' will always be the proto package and message name
				"@type": "type.googleapis.com/rogue.services.Test",

				// Matches the field names for the message
				"attribute1": 10,
				"attribute2": 5}},
	}

	jsonPayload, err := json.Marshal(payload)
	if err != nil {
		panic(err)
	}

	request, err := http.NewRequest("POST", URL+"/rest/insert", bytes.NewBuffer(jsonPayload)) // Insert API
	// request, err := http.NewRequest("PATCH", URL+"/rest/update", bytes.NewBuffer(jsonPayload)) // Update API
	// request, err := http.NewRequest("DELETE", URL+"/rest/remove", bytes.NewBuffer(jsonPayload)) // Remove API
	if err != nil {
		panic(err)
	}

	request.Header.Set("Authorization", "Bearer "+encodedJWT)
	request.Header.Set("Content-Type", "application/json")

	client := &http.Client{Timeout: 10 * time.Second}

	// No response given. Errors reported in status code.
	response, err := client.Do(request)
	response.Body.Close()

	//////////////////////////////////////
	////////  Search API Example  ////////
	//////////////////////////////////////

	// Example of a basic index query.
	// For Test, attribute1, attribute2, and attribute3 form the index.
	// Search Query:
	// Test.attribute1 >= 1 and Test.attribute2 >= 1 and Test.attribute3 >= true
	// AND
	// Test.attribute1 <= 10 and Test.attribute2 <= 10 and Test.attribute3 <= true
	payload = map[string]interface{}{
		"api_key": API_KEY,
		"queries": []map[string]interface{}{
			map[string]interface{}{
				"basic": map[string]interface{}{
					"comparisons": []string{"GREATER_EQUAL", "LESSER_EQUAL"},
					"operands": []map[string]interface{}{
						map[string]interface{}{
							// Part after '/' will always be the proto package and message name
							"@type": "type.googleapis.com/rogue.services.Test",

							// Matches the field names for the message
							"attribute1": 1,
							"attribute2": 1,
							"attribute3": true},
						map[string]interface{}{
							"@type":      "type.googleapis.com/rogue.services.Test",
							"attribute1": 10,
							"attribute2": 10,
							"attribute3": true}}}}}}

	jsonPayload, err = json.Marshal(payload)
	if err != nil {
		panic(err)
	}

	// All search query types use this URL
	request, err = http.NewRequest("GET", URL+"/rest/search", bytes.NewBuffer(jsonPayload))
	if err != nil {
		panic(err)
	}

	request.Header.Set("Authorization", "Bearer "+encodedJWT)
	request.Header.Set("Content-Type", "application/json")

	response, err = client.Do(request)
	body, err := io.ReadAll(response.Body)
	if err != nil {
		panic(err)
	}

	// Queries are zero-indexed.
	// Results are mapped to results field.
	// All messages are stored in the messages field.
	fmt.Print(body)
	response.Body.Close()

	// Example of a basic non-indexed query
	// Search Query:
	// Test.attribute1 >= 1 && Test.attribute2 >= 1 && Test.attribute3 >= true
	// AND
	// Test.attribute1 <= 10 && Test.attribute2 <= 10 && Test.attribute3 <= true
	payload = map[string]interface{}{
		"api_key": API_KEY,
		"queries": []map[string]interface{}{
			map[string]interface{}{
				"basic": map[string]interface{}{
					"comparisons": []string{"GREATER_EQUAL", "LESSER_EQUAL"},
					"fields":      []int{1, 2}, // Corresponds to field ids in test.proto
					"operands": []map[string]interface{}{
						map[string]interface{}{
							// Part after '/' will always be the proto package and message name
							"@type": "type.googleapis.com/rogue.services.Test",

							// Matches the field names for the message
							"attribute1": 1},
						map[string]interface{}{
							"@type":      "type.googleapis.com/rogue.services.Test",
							"attribute2": 10}}}}}}

	///////////////////////////////////
	//// Schema Change API Example ////
	///////////////////////////////////

	// All proto files should be sent in a list of
	// their contents. No modifications required.
	var protoFileDefinitions []string
	for _, file := range detectFiles([]string{"path/to/proto/directory1", "path/to/proto/directory2"}) {
		content, err := os.ReadFile(file)
		if err != nil {
			panic(err)
		}
		protoFileDefinitions = append(protoFileDefinitions, string(content))
	}

	// Any schemas excluded will have associated data deleted.
	// Schema change failure results in no changes applied.
	payload = map[string]interface{}{
		"api_key": API_KEY,
		"schemas": protoFileDefinitions}

	jsonPayload, err = json.Marshal(payload)
	if err != nil {
		panic(err)
	}

	request, err = http.NewRequest("POST", URL+"/rest/subscribe", bytes.NewBuffer(jsonPayload))
	if err != nil {
		panic(err)
	}

	request.Header.Set("Authorization", "Bearer "+encodedJWT)
	request.Header.Set("Content-Type", "application/json")

	// No response given. Errors reported in status code.
	response, err = client.Do(request)
	response.Body.Close()
}
