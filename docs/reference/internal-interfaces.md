# Internal interfaces

<!---
Got the internal gRPC/message routing concept from [common/communication] usage in [af_core/src/af_orchestrator.cpp](../../af_core/src/af_orchestrator.cpp) and the `grpcurl` examples in [README.md](../../README.md).
-->

## AF Core gRPC entry point

<!---
Got the default AF Core server port from [af_core/src/af_orchestrator.cpp](../../af_core/src/af_orchestrator.cpp) (server_port=50051) and from [af_core/config/af_core.yaml](../../af_core/config/af_core.yaml).
-->

AF Core runs a gRPC service (created via the communication factory) on port `50051` by default.

<!---
TODO: Document the actual protobuf service definitions and message schemas.
We need to inspect `common/protos` and generated outputs.
-->
