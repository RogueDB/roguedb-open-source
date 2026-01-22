import datetime
import json
import jwt
import time
import os
from pathlib import Path

# REST and JSON Imports
import requests

# External Dependency Requirements:
# PyJWT, requests

def create_jwt(service_account: str, expire_minutes: int = 60) -> str:
    # NOTE: Replace with file path to service_account.json
    # from purchase confirmation email.
    with open(service_account) as input_file:
        key_data = json.load(input_file)

    now = datetime.datetime.now(datetime.timezone.utc)
    iat = int(time.mktime(now.timetuple()))
    exp = int(time.mktime((now + datetime.timedelta(minutes=expire_minutes)).timetuple()))

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
    
    headers = {
        "Authorization": f"Bearer {encoded_jwt}",
        "Content-Type": "application/json" }

    ################################################
    #### Insert, Update, and Remove API Example ####
    ################################################

    # NOTE: See queries.proto for all API definitions.
    # Creating an Insert, Update, or Remove API with JSON. 
    request = {
        # api_key and messages match Insert definition in queries.proto
        "api_key" : api_key,
        "messages": [
            {
                # Part after '/' will always be the proto package and message name
                "@type": "type.googleapis.com/rogue.services.Test",
                
                # Matches the field names for the message
                "attribute1" : 10,
                "attribute2" : 5,
            } ]}

    # No response given. Errors reported in status code.
    # REST call for Insert API.
    response = requests.post(
        f"https://{url}/rest/insert", 
        headers=headers, 
        data=request)
    
    # REST call for Update API.
    response = requests.patch(
        f"https://{url}/rest/update", 
        headers=headers, 
        data=request)
    
    # REST call for Remove API.
    response = requests.delete(
        f"https://{url}/rest/remove", 
        headers=headers, 
        data=request)
    

    ###############################
    ##### Search API Examples #####
    ###############################

    # NOTE: See queries.proto for full API.
    # Example of a basic index query. 
    # For Test, attribute1, attribute2, and attribute3 form the index.
    # Search Query: 
    # Test.attribute1 >= 1 and Test.attribute2 >= 1 and Test.attribute3 >= true
    # AND
    # Test.attribute1 <= 10 and Test.attribute2 <= 10 and Test.attribute3 <= true
    search = {
        "api_key": api_key,
        "queries": [
        {
            "basic": 
            {
                "comparisons": ["GREATER_EQUAL", "LESSER_EQUAL"],
                "operands": [
                {
                    # Part after '/' will always be the proto package and message name
                    "@type": "type.googleapis.com/rogue.services.Test",
                    
                    # Matches the field names for the message
                    "attribute1" : 1,
                    "attribute2" : 1,
                    "attribute3" : True,
                },
                {
                    "@type": "type.googleapis.com/rogue.services.Test",
                    "attribute1" : 10,
                    "attribute2" : 10,
                    "attribute3" : True
                }] 
            }
        }] }

    # All search query types use this URL
    response = requests.get(
        f"https://{url}/rest/search", 
        headers=headers, 
        data=search)
    
    # Queries are zero-indexed. 
    # Results are mapped to that index.
    for result in response.results[0]:
        pass # Will be JSON objects
    
    # Example of a basic non-indexed query.
    # Search Query: Test.attribute1 < 1 Test.attribute2 != 10
    search = {
        "api_key": api_key,
        "queries": [
        {
            "basic": 
            {
                "comparisons": ["LESSER", "NOT_EQUAL"],
                "fields" : [1, 2], # Corresponds to field ids in test.proto
                "operands": [
                {
                    # Part after '/' will always be the proto package and message name
                    "@type": "type.googleapis.com/rogue.services.Test",

                    # Matches the field names for the message
                    "attribute1" : 1,
                },
                {
                    "@type": "type.googleapis.com/rogue.services.Test",
                    "attribute2" : 10,
                }]
            }
        }] }
    
    #####################################
    ##### Schema Change API Example #####
    #####################################
    
    proto_file_definitions = []
    # Proto file definitions. No modifications required.
    for file in detect_files(directories=[
        "absolute/path/to/protos/directory1",
        "absolute/path/to/protos/directory2"]):
        with open(file) as input_file:
            proto_file_definitions.schemas.append(input_file.read())

    # Any schemas excluded will have associated data deleted.
    # Schema change failure results in no changes applied.
    response = requests.post(
        f"https://{url}/rest/search", 
        headers=headers, 
        data={
            "api_key" : api_key,
            "schemas" : proto_file_definitions
        })