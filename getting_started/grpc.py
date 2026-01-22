import datetime
import json
import jwt
import time
import os
from pathlib import Path

# gRPC and Protocol Buffers Imports
import grpc
from grpc_status import rpc_status

from roguedb.roguedb_pb2_grpc import RogueDBStub
from roguedb.queries_pb2 import (
    Insert, Update, Remove, Basic, Search, 
    LogicalOperator, ComparisonOperator, Subscribe)
from roguedb.test_pb2 import Test

# External Dependency Requirements:
# grpcio, protobuf, PyJWT

def create_jwt(service_account: str, expire_minutes: int = 60) -> str:
    # NOTE: Replace with file path to service_account.json
    # from purchase confirmation email.
    with open(service_account) as input_file:
        key_data = json.load(input_file)

    now = datetime.datetime.now(datetime.timezone.utc)
    iat = int(time.mktime(now.timetuple()))
    exp = int(time.mktime((
        now + datetime.timedelta(minutes=expire_minutes)).timetuple()))

    payload = {
        "iat": iat,
        "exp": exp,
        "iss": key_data['client_email'],
        "aud": f"{key_data['client_email'].split('@')[0]}.roguedb.dev", 
        "sub": key_data['client_email'] }

    headers = {
        "kid": key_data['private_key_id'],
        "alg": "RS256", # Google Cloud service accounts typically use RS256
        "typ": "JWT" }

    return jwt.encode(
        payload, key_data['private_key'], 
        algorithm="RS256", headers=headers)

def detect_files(directories: list[str]) -> list[Path]:
    files = []
    for directory in directories:
        with os.scandir(directory) as scan:
            for item in scan:
                path = Path(item.path)
                if path.is_file() and path.suffix == ".proto":
                    files.append(path)
                elif path.is_dir():
                    for subitem in detect_files(directory=[item.path, ]):
                        files.append(subitem)
    return files

if __name__ == "__main__":
	# See purchase confirmation emails for details and service_account.json.
    url = "c-[YOUR_IDENTIFIER_FIRST_28_CHARACTERS].roguedb.dev"
    api_key = "YOUR_API_KEY"
    encoded_jwt = create_jwt("path/to/service_account.json")

    ################################################
    #### Insert, Update, and Remove API Example ####
    ################################################
    
    request = Insert(api_key=api_key) # Insert API
    # update = Update(api_key=api_key) # Update API
    # remove = Remove(api_key=api_key) # Remove API
    
    # Insert, Update, and Remove are identical in use.
    request.messages.add()

    # See test.proto for the schema definition.
    test = Test(attribute1=10)
    request.messages[0].Pack(test)
    
    roguedb = RogueDBStub(
        channel=grpc.secure_channel(f"{url}:443", 
        grpc.ssl_channel_credentials()))

    def yield_request():
        yield request

    try:
        for _ in roguedb.insert(
        # for _ in roguedb.update( # Update API call
        # for _ in roguedb.remove( # Remove API call
            yield_request(), 
            metadata=[("authorization", f"Bearer {encoded_jwt}")]):
            pass
    except grpc.RpcError as rpc_error:
        # Any errors get reported in status.
        status = rpc_status.from_call(rpc_error)
    
    ###############################
    ##### Search API Examples #####
    ###############################

    # Example of a basic index query. 
    # For Test, attribute1, attribute2, and attribute3 form the index.
    # Search Query: 
    # Test.attribute1 >= 1 and Test.attribute2 >= 1 and Test.attribute3 >= true
    # AND
    # Test.attribute1 <= 10 and Test.attribute2 <= 10 and Test.attribute3 <= true
    search = Search(api_key=api_key)
    expression = search.queries.add().basic
    expression.logical_operator = LogicalOperator.AND

    expression.comparisons.append(ComparisonOperator.GREATER_EQUAL)
    expression.operands.add().Pack(Test(attribute1=1, attribute2=1, attribute3=True))

    expression.comparisons.append(ComparisonOperator.LESSER_EQUAL)
    expression.operands.add().Pack(Test(attribute2=10, attribute2=10, attribute3=True))

    def yield_search():
        yield search

    for response in roguedb.search(
        yield_search(),
        metadata=[("authorization", f"Bearer {encoded_jwt}")]):
        results = []

        # Queries are zero-indexed. Partial results get sent
        # and mapped to that index.
        for result in response.results[0].messages:
            test = Test()
            response.results[0].Unpack(test)
        
        # Each response sends a list of the query ids finished
        # with processing.
        if 0 in response.finished:
            pass # Finished processing.

    # Example of a basic non-indexed query. 
    # Search Query: Test.attribute1 < 1 AND Test.attribute2 != 10
    search = Search(api_key=api_key)
    expression = search.queries.add().basic
    expression.logical_operator = LogicalOperator.AND

    expression.comparisons.append(ComparisonOperator.LESSER)
    expression.operands.add().Pack(Test(attribute1=1))
    expression.fields.append(1) # Corresponds to field id in test.proto

    expression.comparisons.append(ComparisonOperator.NOT_EQUAL)
    expression.operands.add().Pack(Test(attribute2=10))
    expression.fields.append(2) # Corresponds to field id in test.proto

    #####################################
    ##### Schema Change API Example #####
    #####################################

    subscribe = Subscribe(api_key=api_key)

    # All proto files should be sent in a list of
    # their contents. No modifications required.
    for file in detect_files(directories=[
        "absolute/path/to/protos/directory1",
        "absolute/path/to/protos/directory2"]):
        with open(file) as input_file:
            subscribe.schemas.append(input_file.read())

    # Any schemas excluded will have associated data deleted.
    # Schema change failure results in no changes applied.
    response = roguedb.subscribe(subscribe)
