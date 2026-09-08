# First request

This page shows one known working request format used by the repository documentation.

## QoD create session (gRPC)

```bash
docker run --rm --network host -v ./common/protos:/var/protos/ fullstorydev/grpcurl -plaintext \
  -proto message.proto \
  -import-path /var/protos \
  -d '{
     "message_type": "qod_create_session",
     "correlation_id": "12345",
     "payload": "'$(cat ./af_core/tests/requests/qod/qod_create_session.json | base64 -w 0)'",
     "metadata": {
      "source": "command_line",
      "priority": "high"
    }
  }' \
  192.168.70.141:50051 \
  af.proto.InternalCommunication/SendMessage
```

A successful invocation returns an `af.proto.InternalMessage` with a
matching `correlation_id`, a `message_type` of `qod_create_session_response`,
and a base64-encoded JSON payload matching the CAMARA QoD
`CreateSession`/`SessionInfo` schema. See
[`common/protos/message.proto`](../../common/protos/message.proto) for the
envelope and [`af_core/src/qod/qod_session_manager.cpp`](../../af_core/src/qod/qod_session_manager.cpp)
for the payload shape.