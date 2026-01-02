# Overview

When bandwidth allows, RogueDB aims to develop a suite of very light wrappers to achieve the ideal 1-to-1 API match with built-in data structures. We will start with C++, Go, and Python given internal familiarity with these languages. Additional programming languages will be supported based on demand or contributions.

## Objectives

We want to make RogueDB as easy as possible to use. This means doing the following with the utility functions:

- Handle serialization to and from the Any profobuf
- Handle optimal channel reuse and pooling
- Match usage to built-in data structures
- Standard library only dependencies for drop-in use by users
