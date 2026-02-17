# Overview

This directory contains complete examples of how to use RogueDB. 

The changes required to be up and running are the following:
- API Key -> Found in your 1st purchase confirmation email.
- Database URL -> Found in your 1st purchase confirmation email.
- Service Account JSON -> Found in your 2nd purchase confirmation email.

The `service_account.json` in the email has values required to create a JWT token for authentication. Secure use is to keep these values in a keyvault or similar secret manager. 

This directory is split into two groupings:
- gRPC and Protocol Buffers under `grpc`
- REST and JSON under `rest`
