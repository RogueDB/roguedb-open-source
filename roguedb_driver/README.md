# Overview

When bandwidth allows, RogueDB aims to develop a suite of very light wrappers to achieve the ideal 1-to-1 API match with built-in data structures. We will start with C++ and Python given the extensive use of these two languages internally. Additional programming languages will be supported based on demand or contributions.

## Objectives

We want to make RogueDB as easy as possible to use. This means doing the following with the utility functions:

- Handle the SSL/TLS Certificate renewal logic
- Handle the SSL/TLS connection with gRPC
- Handle serialization to and from the Any profobuf
- Handle optimal channel reuse and pooling
- Match the API to built-in data structures
- Minimal dependencies to allow easy drop-in use by users

In particular, the implementation must also be straightforward and concise enough that copy and paste also works rather than dependency management wrangling fo us and you. The utility functions are ultimately very light constructs meant to reduce boilerplate code for you. If the utility functions do not match your needs precisely, then we hope that they serve as real use cases to leverage as examples as an accelerated starting point.
