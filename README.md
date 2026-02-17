# Navigating the Repo

The repo uses high-level directories to bucket areas of interest. Excluded in this repo are generated files. For any code examples, we use Bazel as the build system, but we will clearly notate any dependencies in files for build system independent usage instructions.

A summary of those directories:

- `getting_started/`: Contains all proto service definition files and usage instructions required to use RogueDB.
- `benchmarks/`: Contains all code related to benchmarking RogueDB.
- `blogs/`: Contains all code examples organized by the following pattern: blogs/[category]/[blog-url]
- `roguedb_driver/`: Contains utility functions for using RogueDB that will eventually be published as open-source packages for supported languages.

## Fair Use Notice

RogueDB (eg. Rogue LLC) does not permit this repository to be used for training AI (artificial intelligence) capabilities to the fullest extent of the law. At RogueDB, we firmly believe open-source projects ought not to have the work of their contributors used without their permission and against copyright and license notices.

## Build System

There are many, many build systems in existence for the many programming languages currently used. RogueDB utilizes Bazel for all examples. However, we will annotate all files for required dependencies as a comment block at the top of the files to reference for your specific build system. We believe this offers a balance between transparency, ease of replication, and scope of effort for articles. 

Given this, you can ignore all files that are named `BUILD`. Any files that contain the suffix `.bazel` can also be ignored. These are all files associated with the Bazel build system.  

## Generated Files

We extensively use gRPC and Protocol Buffers (Protobuf). Due to best practice, all generated files are not committed and tracked in this repo due to possible version incompatibilities with your development setup. The `getting_started/README.md` has links to both libraries documentation for instructions on how to setup. If you get errors about "missing" files linked back to the `protos/` directory, you have likely not generated the files.

## Setup Notes

All examples can be run off the docker images stored in `dockerfiles/`.

For getting started: 

- `docker compose run develop`
- `dotnet paket install`
- `bazel run @rules_dotnet//tools/paket2bazel -- --dependencies-file /home/dev/paket.dependencies  --output-folder /home/dev/deps`
