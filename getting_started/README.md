# Overview

RogueDB utilizes gRPC as the communication framework and Protocol Buffers (Protobuf) as the data format for sending and receiving messages. To communicate, you will need to generate the gRPC and Protobuf files in your target progamming language. All the files required are in the root of this directory with the suffix `.proto`.

gRPC Docs: https://grpc.io/
Protobuf Docs: https://protobuf.dev/

## gRPC and Protobuf

The following commands are sufficient with the pre-requisites installed:
- Protobuf: `protoc --cpp_out=. --python_out=. --pyi_out=. $(find -iname "*.proto")`
- gRPC: `protoc --grpc_out=. --plugin=protoc-gen-grpc="which grpc_cpp_plugin_native" path/to/directory/roguedb.proto`

Note the first command generates all proto files in the current directory for Python and C++. The second command requires the relative path to `roguedb.proto` and the gRPC plugin to build C++ only files. In both cases, refer to the documentation to determine the right options and pre-requisites for your specific progamming language.

## SSL/TLS Encryption

This feature is optional. We encourage folks to use it by default given very little cost in performance or extra code. See the website documentation for an example. 

Our SSL certificate is issued by Google's Managed Certificate service and renewed every 90 days. To obtain the server's public certificate, the following command works on Linux:

- Run: `echo | openssl s_client -servername roguedb.dev -connect 34.117.86.181:443 2>&1 | sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p'`

Produces a file with an output similar to this:

```
-----BEGIN CERTIFICATE-----
MIIFMTCCBBmgAwIBAgIQJOp7rG5Lwv0JpieJhRD1ojANBgkqhkiG9w0BAQsFADA7
MQswCQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRydXN0IFNlcnZpY2VzMQww
CgYDVQQDEwNXUjMwHhcNMjUxMTA0MTYxMzI4WhcNMjYwMjAyMTcwNzIxWjAWMRQw
EgYDVQQDEwtyb2d1ZWRiLmRldjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoC
ggEBALNAjvLOx2mGFT39wo707zNQx1nWiSJiHBcX+OU2TlwYbi5Zay9m8DgLd09g
CZ32zbCNU8veFb7CmM8XBIedbzSc07knEYWwwWTXjOuNHj5/e8dTEXCfjYUHMXOf
TE0iV/PMf7ItHTtfKoBV0RIv+psj2ICIlLXIcDWc80lhzQfp7Gpu6vrP31p/eh8e
EewvlZt4ZutCGQeGnzpWW740sMWLmtC/gkkJhZGYNVsWcahgxUge9UjU/n9dCzYi
n+orkzOeRl3W1im51cDQSNJb4RGSUJ4U5MYUsnxOuBadCXnHeLBzI8fhjvyR34Pu
oBOFle2dG02DLkeyyKdSVCxStt8CAwEAAaOCAlQwggJQMA4GA1UdDwEB/wQEAwIF
oDATBgNVHSUEDDAKBggrBgEFBQcDATAMBgNVHRMBAf8EAjAAMB0GA1UdDgQWBBTQ
jh5tObd6chILtjNRxvcErvfr/TAfBgNVHSMEGDAWgBTHgfX9jojZADxNY6JQMSSg
ziP+IzBeBggrBgEFBQcBAQRSMFAwJwYIKwYBBQUHMAGGG2h0dHA6Ly9vLnBraS5n
b29nL3Mvd3IzL0pPbzAlBggrBgEFBQcwAoYZaHR0cDovL2kucGtpLmdvb2cvd3Iz
LmNydDAnBgNVHREEIDAeggtyb2d1ZWRiLmRldoIPd3d3LnJvZ3VlZGIuZGV2MBMG
A1UdIAQMMAowCAYGZ4EMAQIBMDYGA1UdHwQvMC0wK6ApoCeGJWh0dHA6Ly9jLnBr
aS5nb29nL3dyMy9TNXpfTUFQXzU4QS5jcmwwggEDBgorBgEEAdZ5AgQCBIH0BIHx
AO8AdQAWgy2r8KklDw/wOqVF/8i/yCPQh0v2BCkn+OcfMxP1+gAAAZpP2/5aAAAE
AwBGMEQCIGDqMz5HxgyP8y6dtOoz8qUBJAYRhppHIGtEbiRhG/LgAiB8kl0mpy9X
NJ8qOiMEhccBdqnXsJ5s9znhwuIOgdHlUAB2AJaXZL9VWJet90OHaDcIQnfp8DrV
9qTzNm5GpD8PyqnGAAABmk/b/lsAAAQDAEcwRQIhAMCHj5bCYuSBPsF0HuAyiLVs
KzZlgppvASIamY0Ik44yAiAUlyqVzbOQUiu8lN9Tv8vuhK5TcpUy9MyIBVoPhySa
7jANBgkqhkiG9w0BAQsFAAOCAQEAHqVa3UxAxkqL7vXVz6w/cflfiLCVohIQMpxC
Owv7lxeW9pG3rqdQwX2pMFjgvFzIE7QqbDuPqCXWHckRG3nxS4el57fHDtpPtVFI
EhzjwhYF3iqaooF0+5mfnTXHo4S9XEQWhK/xIft4sbm0xgHkh3x1Pf0Sh3uScJZ1
z3PwCye4xOGb2XoG2bfewlu1sg45Wrql+4l3RBTS98KKCdgQSigvBAlji9vUxvid
/9y4MMlU74yswCZVQvqkYA+xfli0Du6Yf1tdk1ueU/uRIsUVaxT4CMF48wQg4N1i
ATHPoJ27tLAMnKRXWB+3rAJGurP95u07Mhs1z1p5wf9IUfFeWA==
-----END CERTIFICATE-----
```
