package main

import (
	"context"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"github.com/roguedb/roguedb-open-source/roguedb/core_schemas"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/metadata"
	"google.golang.org/protobuf/types/known/anypb"
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

	connection, err := grpc.NewClient(URL, grpc.WithTransportCredentials(credentials.NewTLS(nil)))
	defer connection.Close()
	client := core_schemas.NewRogueDBClient(connection)

	md := metadata.Pairs("authorization", fmt.Sprintf("Bearer: %s", encodedJWT))
	ctx := metadata.NewOutgoingContext(context.Background(), md)

	stream, err := client.Insert(ctx) // Insert API
	// stream, err = client.Update(ctx) // Update API
	// stream, err = client.Remove(ctx) // Remove API
	if err != nil {
		panic(err)
	}

	complete := make(chan struct{})

	// No response is given for Insert, Update, and Remove
	// Any errors get reported in status.
	go func() {
		for {
			_, err := stream.Recv()
			if err == io.EOF {
				close(complete)
				return
			}

			if err != nil {
				panic(err)
			}
		}
	}()

	request := core_schemas.Insert{ApiKey: API_KEY}
	// request := core_schemas.Update{ApiKey: API_KEY} // Update API
	// request := core_schemas.Remove{ApiKey: API_KEY} // Remove API

	// Insert, Update, and Remove are identical in use.
	request.Messages = append(request.Messages, &anypb.Any{})
	test := core_schemas.Test{Attribute1: 10}
	err = request.Messages[0].MarshalFrom(&test)
	if err != nil {
		panic(err)
	}

	err = stream.Send(&request)
	if err != nil {
		panic(err)
	}

	err = stream.CloseSend() // Signal all queries sent. Otherwise, blocks.
	if err != nil {
		panic(err)
	}

	<-complete

	//////////////////////////////////////
	////////  Search API Example  ////////
	//////////////////////////////////////

	// Example of a basic index query.
	searchStream, err := client.Search(ctx)
	if err != nil {
		panic(err)
	}

	// Search Query:
	// Test.attribute1 >= 1 && Test.attribute2 >= 1 && Test.attribute3 >= true
	// AND
	// Test.attribute1 <= 10 && Test.attribute2 <= 10 && Test.attribute3 <= true
	search := core_schemas.Search{ApiKey: API_KEY}
	search.Queries = append(search.Queries, &core_schemas.Expression{})
	basic := search.Queries[0].GetBasic()
	basic.LogicalOperator = core_schemas.LogicalOperator_AND

	basic.Comparisons = append(basic.Comparisons, core_schemas.ComparisonOperator_GREATER_EQUAL)
	basic.Operands = append(basic.Operands, &anypb.Any{})
	test = core_schemas.Test{Attribute1: 1, Attribute2: 1, Attribute3: true}
	basic.Operands[0].MarshalFrom(&test)

	basic.Comparisons = append(basic.Comparisons, core_schemas.ComparisonOperator_LESSER_EQUAL)
	basic.Operands = append(basic.Operands, &anypb.Any{})
	test = core_schemas.Test{Attribute1: 10, Attribute2: 10, Attribute3: true}
	basic.Operands[1].MarshalFrom(&test)

	complete = make(chan struct{})

	// Multiple queries or large queries should be processed on a separate thread
	// to prevent blocking the database.
	go func() {
		for {
			response, err := stream.Recv()
			if err == io.EOF {
				close(complete)
				return
			}

			if err != nil {
				panic(err)
			}

			// Queries are zero-indexed. Partial results get sent
			// and mapped to that index.
			for _, message := range response.Results[0].Messages {
				temp := core_schemas.Test{}
				message.UnmarshalTo(&temp)
			}

			// Each response sends a list of the query ids finished
			// with processing.
			if slices.Contains(response.Finished, 0) {
			}
		}
	}()

	err = searchStream.Send(&search)
	if err != nil {
		panic(err)
	}
	err = searchStream.CloseSend() // Signal all queries sent. Otherwise, blocks.
	if err != nil {
		panic(err)
	}
	<-complete

	// Example of a basic non-indexed query.
	search = core_schemas.Search{ApiKey: API_KEY}
	search.Queries = nil

	// Search Query:
	// Test.attribute1 >= 1 && Test.attribute2 <= 10
	search.Queries = append(search.Queries, &core_schemas.Expression{})
	basic = search.Queries[0].GetBasic()
	basic.LogicalOperator = core_schemas.LogicalOperator_AND
	basic.Fields = append(basic.Fields, 1, 2) // Corresponds to field ids in test.proto

	basic.Comparisons = append(basic.Comparisons, core_schemas.ComparisonOperator_GREATER_EQUAL)
	basic.Operands = append(basic.Operands, &anypb.Any{})
	test = core_schemas.Test{Attribute1: 1}
	basic.Operands[0].MarshalFrom(&test)

	basic.Comparisons = append(basic.Comparisons, core_schemas.ComparisonOperator_LESSER_EQUAL)
	basic.Operands = append(basic.Operands, &anypb.Any{})
	test = core_schemas.Test{Attribute2: 10}
	basic.Operands[1].MarshalFrom(&test)

	///////////////////////////////////
	//// Schema Change API Example ////
	///////////////////////////////////

	subscribe := core_schemas.Subscribe{ApiKey: API_KEY}

	// All proto files should be sent in a list of
	// their contents. No modifications required.
	for _, file := range detectFiles([]string{"path/to/proto/directory1", "path/to/proto/directory2"}) {
		content, err := os.ReadFile(file)
		if err != nil {
			panic(err)
		}
		subscribe.Schemas = append(subscribe.Schemas, string(content))
	}

	// Any schemas excluded will have associated data deleted.
	// Schema change failure results in no changes applied.
	_, err = client.Subscribe(ctx, &subscribe)
	if err != nil {
		panic(err)
	}
}
