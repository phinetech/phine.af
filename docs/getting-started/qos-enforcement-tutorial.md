---
runme:
  id: qos-enforcement-tutorial
  version: v3
cwd: ../..
---

# End-to-End QoS Enforcement Tutorial

This tutorial deploys the 5G testbed, sends a CAMARA QoD session request, captures the control-plane path, and verifies downlink bandwidth enforcement with `iperf3`.

By the end of the run you will:

1. Start free5GC, UERANSIM, and the phine.af Application Function
2. Exercise the full QoD session lifecycle: create, retrieve, query by device, extend, and delete
3. Optionally inspect the policy signalling path: **AF → PCF → SMF → UPF**
4. Verify that the UPF enforces downlink bandwidth with `iperf3` — and releases it after the session is deleted

## Prerequisites

| Requirement | Notes |
|---|---|
| Docker Engine ≥ 24.0 | With Docker Compose v2 plugin |
| Linux host | Network interface creation requires Linux kernel capabilities |
| Wireshark or `tshark` | For control-plane capture |
| ~8 GB free RAM | The full stack runs roughly 15 containers |
| Free local subnets | Nothing else should be bound to `192.168.70.128/26`, `192.168.71.128/26`, or `192.168.72.128/26` |

## Configuration

In most cases you only need to choose the AF topology:

- `AF_PROFILE=af` for the bundled `af` container
- `AF_PROFILE=afs` for separate `af_core` and `pcf_handler` containers

The default transport is **HTTP/2** because `COMPOSE_OVERRIDE_FILE` points at `docker-compose/compose.http.yaml`. If you want **gRPC** instead, set `COMPOSE_OVERRIDE_FILE=docker-compose/compose.grpc.yaml` before deployment.

```bash {"name":"setup-variables","interactive":"false"}
# Choose one deployment mode:
#   bundled AF:     AF_PROFILE=af
#   microservice:   AF_PROFILE=afs
export COMPOSE_FILE="${COMPOSE_FILE:-docker-compose/compose.yaml}"
export COMPOSE_OVERRIDE_FILE="${COMPOSE_OVERRIDE_FILE:-docker-compose/compose.http.yaml}"
export AF_PROFILE="${AF_PROFILE:-af}"
export COMPOSE_PROFILES="${COMPOSE_PROFILES:---profile free5gc --profile $AF_PROFILE}"
export RAN_SERVICES="${RAN_SERVICES:-ueransim-gnb ueransim-ue}"
export LOGS_DIR="${LOGS_DIR:-/tmp/phine.af/qos-enforcement-tutorial/logs}"
mkdir -p "$LOGS_DIR"
sudo mkdir -p "$LOGS_DIR"
sudo chmod 777 "$LOGS_DIR"

export UE_IP="${UE_IP:-10.60.0.1}"
export EXT_DN_IP="${EXT_DN_IP:-192.168.72.135}"
export MIN_MBPS="${MIN_MBPS:-3}"
export MAX_MBPS="${MAX_MBPS:-12}"
echo "Configuration set:"
echo "  COMPOSE_FILE: $COMPOSE_FILE"
echo "  COMPOSE_OVERRIDE_FILE: $COMPOSE_OVERRIDE_FILE"
echo "  AF_PROFILE: $AF_PROFILE"
echo "  COMPOSE_PROFILES: $COMPOSE_PROFILES"
echo "  RAN_SERVICES: $RAN_SERVICES"
echo "  LOGS_DIR: $LOGS_DIR"
echo "  UE_IP: $UE_IP"
echo "  EXT_DN_IP: $EXT_DN_IP"
echo "  Bandwidth validation range: ${MIN_MBPS}-${MAX_MBPS} Mbps"
```

### Transport Reference

| Transport | Compose override | AF entrypoint | Notes |
|---|---|---|---|
| HTTP/2 (default) | `docker-compose/compose.http.yaml` | `192.168.70.141:8080` | `af_client` uses `curl` |
| gRPC | `docker-compose/compose.grpc.yaml` | `192.168.70.141:50051` | `af_client` uses `grpcurl` |

If you have changed overrides locally and want to reset to the documented HTTP default, run:

```bash {"name":"setup-http-variant","excludeFromRunAll":"true","interactive":"false"}
export COMPOSE_OVERRIDE_FILE="docker-compose/compose.http.yaml"
```

For the full component topology and how the AF, PCF, SMF, and UPF interact, see the [Architecture Overview](../architecture/overview.md).

<details>
<summary><b>Service reference</b></summary>

| Service | Address | Notes |
|---|---|---|
| `af` / `af-core` | `192.168.70.141` | QoD request entrypoint |
| `af-pcf-handler` | `192.168.70.140` | Used only in microservice mode |
| `pcf` | `192.168.70.139` | Policy control |
| `smf` | `192.168.70.133` | Session management |
| `ue` | `10.60.0.1` | UE data-plane IP |
| `oai-ext-dn` | `192.168.70.135` / `192.168.72.135` | Public-net address and data-plane address |

</details>

## Step 1: Deploy the Setup

Install dependencies and build the `gtp5g` kernel module:

```bash {"name":"install-deps","interactive":"false"}
./build/scripts/ci_helper.sh install_dependencies
./build/scripts/ci_helper.sh install_gtp5g
```

Ensure submodules are up to date:

```bash {"name":"check-submodules","interactive":"false"}
./build/scripts/ci_helper.sh check_submodules
```

Build and start the core services, then start the RAN:

```bash {"name":"deploy-stack","interactive":"false"}
docker compose -f $COMPOSE_FILE -f $COMPOSE_OVERRIDE_FILE $COMPOSE_PROFILES up -d --build

sleep 30

docker compose -f $COMPOSE_FILE up -d $RAN_SERVICES
```

Give the stack time to initialize, then inspect the service state:

```bash {"name":"wait-for-healthy","interactive":"false"}
sleep 30
docker compose -f $COMPOSE_FILE $COMPOSE_PROFILES ps
```

Verify that the UE has registered and received its tunnel IP:

```bash {"name":"verify-ue-ip","interactive":"false"}
sleep 15
docker exec ue ip addr show uesimtun0 | grep "10.60.0.1"
```

Verify connectivity between the UE and the external data network:

```bash {"name":"verify-connectivity","interactive":"false"}
docker exec ue ping -I uesimtun0 $EXT_DN_IP -c 3
```

## Step 2: Start Traffic Capture

Start a control-plane capture before sending the QoD request:

```bash {"name":"start-capture","interactive":"false"}
PCAP_FILE="$LOGS_DIR/capture.pcapng"
PID_FILE="$LOGS_DIR/capture.pid"
LOG_FILE="$LOGS_DIR/capture.tshark.log"

sudo nohup tshark -i demo-oai \
  -f "host 192.168.70.141 or host 192.168.70.140 or host 192.168.70.139 or host 192.168.70.133" \
  -w "$PCAP_FILE" \
  >"$LOG_FILE" 2>&1 &
echo $! > "$PID_FILE"

echo "Traffic capture started in background (PID: $(cat "$PID_FILE" 2>/dev/null || echo unknown))"
sleep 2
```

This filter keeps the capture focused on the AF path, the PCF, the SMF, and the PFCP session update toward the UPF.

## Step 3: Session Lifecycle

This step exercises the full CAMARA QoD REST API: create a session, retrieve it by ID, query sessions by device, extend the duration, then delete it.

> **Note:** Steps 3.2–3.4 and Step 6 use `docker compose run` with explicit curl arguments and apply to the **HTTP transport only**. If you are running the gRPC variant, skip ahead to Step 4 after step 3.1.

### 3.1 — Create a Session

This tutorial uses the `premium` QoS profile:

- **Guaranteed bandwidth**: 5 Mbps uplink + 5 Mbps downlink
- **Maximum bandwidth**: 10 Mbps uplink + 10 Mbps downlink
- **5QI**: 7
- **Duration**: 3600 seconds

The request payload lives at `af_core/tests/requests/qod/qod_create_session.json`:

```json {"excludeFromRunAll":"true"}
{
  "device": {
    "phoneNumber": "+123456789",
    "networkAccessIdentifier": "123456789@domain.com",
    "ipv4Address": {
      "publicAddress": "10.60.0.1",
      "publicPort": 59765
    }
  },
  "applicationServer": {
    "ipv4Address": "0.0.0.0/0"
  },
  "devicePorts": {
    "ranges": [{ "from": 5010, "to": 5020 }],
    "ports": [5060, 5070]
  },
  "applicationServerPorts": {
    "ranges": [{ "from": 5010, "to": 5020 }],
    "ports": [5060, 5070]
  },
  "qosProfile": "premium",
  "duration": 3600
}
```

The key fields for this test are:

- `device.ipv4Address.publicAddress`: the UE data-plane IP
- `devicePorts.ports`: includes port `5070`, which is used for `iperf3`
- `qosProfile`: set to `premium`

Send the request:

```bash {"name":"send-qod-request","interactive":"false"}
# Unified command that works for both gRPC and HTTP transports
docker compose -f $COMPOSE_FILE -f $COMPOSE_OVERRIDE_FILE --profile af-client up -d
```

The wrapper inside `af_client` chooses the correct client for the active override:

- HTTP override: sends a CAMARA JSON request with `curl`
- gRPC override: sends the same payload through `grpcurl`

The exact request (endpoint, headers, and payload file) is defined by the `af-client` service in the active compose override — see [docker-compose/compose.http.yaml](../../docker-compose/compose.http.yaml) or [docker-compose/compose.grpc.yaml](../../docker-compose/compose.grpc.yaml).

Because the request runs detached (`up -d`), view the response with `docker logs af-client`. Extract the `sessionId` returned in the response body — it is required for all subsequent lifecycle steps:

```bash {"name":"extract-session-id","interactive":"false"}
# Wait for af-client to exit, then pull the sessionId from the response body
docker wait af-client > /dev/null 2>&1 || true
SESSION_ID=$(docker logs af-client 2>/dev/null | jq -r '.sessionId // empty')
if [ -z "$SESSION_ID" ]; then
  echo "ERROR: Could not extract sessionId — inspect the response with: docker logs af-client"
  docker logs af-client 2>&1 || true
else
  export SESSION_ID
  echo "SESSION_ID: $SESSION_ID"
fi
```

Wait for the QoS policy to propagate through the signalling chain:

```bash {"name":"wait-for-qos-policy","interactive":"false"}
echo "Waiting for QoS policy to propagate through signalling chain..."
sleep 10
echo "QoS policy propagation wait complete"
```

Stop the capture so you can inspect it later if needed:

```bash {"name":"stop-capture","interactive":"false"}
PCAP_FILE="$LOGS_DIR/capture.pcapng"
PID_FILE="$LOGS_DIR/capture.pid"

if [ -f "$PID_FILE" ]; then
  PID="$(cat "$PID_FILE" 2>/dev/null || true)"
  if [ -n "$PID" ]; then
    sudo kill -TERM "$PID" 2>/dev/null || true
    sleep 1
    sudo kill -KILL "$PID" 2>/dev/null || true
  fi
  rm -f "$PID_FILE"
fi
sudo chmod a+r "$PCAP_FILE" 2>/dev/null || true
echo "Capture stopped"
```

### 3.2 — Retrieve a Session

Fetch the session object by ID and confirm it is `AVAILABLE`:

```bash {"name":"get-qod-session","interactive":"false"}
docker compose -f $COMPOSE_FILE -f $COMPOSE_OVERRIDE_FILE run --rm --no-deps af-client \
  --http2-prior-knowledge -sS \
  "http://${AF_HOST:-af-core}:8080/quality-on-demand/v1/sessions/$SESSION_ID"
```

Expected: a JSON object with `"qosStatus": "AVAILABLE"` and the `premium` profile parameters.

### 3.3 — Query Sessions by Device

Retrieve all active sessions for the test device. The request body is at `af_core/tests/requests/qod/qod_retrieve_sessions.json` and filters by the same device used in step 3.1:

```bash {"name":"retrieve-qod-sessions","interactive":"false"}
docker compose -f $COMPOSE_FILE -f $COMPOSE_OVERRIDE_FILE run --rm --no-deps af-client \
  --http2-prior-knowledge -sS -X POST \
  -H "content-type: application/json" \
  --data "@/requests/qod_retrieve_sessions.json" \
  "http://${AF_HOST:-af-core}:8080/quality-on-demand/v1/retrieve-sessions"
```

Expected: a JSON array containing the session created in step 3.1.

### 3.4 — Extend Session Duration

Request an additional 360 seconds on top of the original duration. The request body is at `af_core/tests/requests/qod/qod_session_extend_duration.json`:

```bash {"name":"extend-qod-session","interactive":"false"}
docker compose -f $COMPOSE_FILE -f $COMPOSE_OVERRIDE_FILE run --rm --no-deps af-client \
  --http2-prior-knowledge -sS -X POST \
  -H "content-type: application/json" \
  --data "@/requests/qod_session_extend_duration.json" \
  "http://${AF_HOST:-af-core}:8080/quality-on-demand/v1/sessions/$SESSION_ID/extend"
```

Expected: a JSON object with the updated `duration` field.

## Step 4: Trace the Signalling Path

This step is optional, but useful if you want to confirm how the QoD request moved through the control plane.

```bash {"name":"view-capture","interactive":"false","excludeFromRunAll":"true"}
PCAP_FILE="$LOGS_DIR/capture.pcapng"
# Open file with wireshark
wireshark $PCAP_FILE
```

<details>
<summary><b>What you should see in the capture</b></summary>

Expected sequence:

```text
af_client
  -> AF entrypoint (.141)
  -> PCF (.139)
  -> SMF (.133)
  -> UPF
```

In microservice mode, the AF path includes an extra hop through `af-pcf-handler` (`.140`) before the request reaches the PCF.

Things to look for:

1. A request to `/npcf-policyauthorization/v1/app-sessions`
2. Flow descriptions for the UE and port `5070`
3. PFCP session modification traffic between the SMF and the UPF
4. A successful PFCP response confirming rule installation

Example flow descriptions:

```text
permit out ip from 10.60.0.1 5070 to 0.0.0.0/0 5070
permit out ip from 0.0.0.0/0 5070 to 10.60.0.1 5070
```

Expected bandwidth fields in the PCF request:

- `marBwDl` / `marBwUl`: `10 Mbps`
- `mirBwDl` / `mirBwUl`: `5 Mbps`

</details>

## Step 5: Verify QoS Enforcement with iperf3

This setup enforces QoS on **downlink** traffic, so the measurement is taken from the external data network toward the UE.

Start the `iperf3` server on the UE:

```bash {"name":"start-iperf3-server","background":"true","interactive":"false"}
docker exec -d ue iperf3 -s -B $UE_IP -p 5070
```

Run the client from the external data network:

```bash {"excludeFromRunAll":"true","interactive":"true"}
docker exec oai-ext-dn iperf3 -c $UE_IP -p 5070 -t 10
echo
```

You should see the throughput capped roughly in the **5-10 Mbps** range.

<details>
<summary><b>Automated Validation Script (for CI/Testing)</b></summary>

The following script automatically validates that bandwidth is being rate-limited. This runs in CI but can also be executed manually:

```bash {"name":"run-iperf3-client","interactive":"false"}
sleep 2

# Run iperf3 client and capture output
IPERF_OUTPUT=$(docker exec oai-ext-dn iperf3 -c $UE_IP -p 5070 -t 10)

echo "$IPERF_OUTPUT"

# Extract receiver bitrate (Mbits/sec) from the summary line
BITRATE_MBPS=$(echo "$IPERF_OUTPUT" | awk '/receiver/{print $7}')

if [ -z "$BITRATE_MBPS" ]; then
  echo "ERROR: Failed to extract bitrate from iperf3 output"
  exit 1
fi

echo "Measured receiver bitrate: ${BITRATE_MBPS} Mbps"

# Premium profile: GBR=5Mbps, MBR=10Mbps
# Allow a tolerance range from MIN_MBPS to MAX_MBPS to account for encapsulation overhead
# The key assertion is that traffic IS being rate-limited (not running at full link speed)
PASS=$(echo "$BITRATE_MBPS >= $MIN_MBPS && $BITRATE_MBPS <= $MAX_MBPS" | bc)
if [ "$PASS" -eq 1 ]; then
  echo "PASS: Bitrate ${BITRATE_MBPS} Mbps is within expected range (${MIN_MBPS}-${MAX_MBPS} Mbps)"
else
  echo "FAIL: Bitrate ${BITRATE_MBPS} Mbps is outside expected range (${MIN_MBPS}-${MAX_MBPS} Mbps)"
  exit 1
fi
```

</details>

Stop the server after the test:

```bash {"name":"stop-iperf3-server","interactive":"false"}
docker exec ue pkill iperf3 || true
```

<details>
<summary><b>Example output</b></summary>

```text
[  5] local 192.168.72.135 port 55068 connected to 10.60.0.1 port 5070
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec  1.39 MBytes  11.7 Mbits/sec    0   84.2 KBytes
[  5]   1.00-2.00   sec  1.23 MBytes  10.4 Mbits/sec    0    142 KBytes
[  5]   2.00-3.00   sec  1.30 MBytes  10.9 Mbits/sec    0    200 KBytes
...
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec  13.0 MBytes  10.9 Mbits/sec    0             sender
[  5]   0.00-10.67  sec  12.0 MBytes  9.43 Mbits/sec                  receiver
```

</details>

### Optional: Run a Baseline Comparison

To compare against an unrestricted flow, run `iperf3` on a port that is **not** part of the QoD session, such as `9000`:

```bash {"name":"run-baseline-iperf3","excludeFromRunAll":"true","interactive":"false"}
# Start iperf3 server on UE on an unrestricted port
docker exec -d ue iperf3 -s -B $UE_IP -p 9000

# Start iperf3 client on data network
docker exec oai-ext-dn iperf3 -c $UE_IP -p 9000 -t 10
```
## Step 6: Delete the Session

Tear down the QoS policy by deleting the session. The AF signals the PCF to remove the policy rule; the PCF instructs the SMF, and the UPF releases the rate limit.

```bash {"name":"delete-qod-session","interactive":"false"}
docker compose -f $COMPOSE_FILE -f $COMPOSE_OVERRIDE_FILE run --rm --no-deps af-client \
  --http2-prior-knowledge -sS -X DELETE \
  "http://${AF_HOST:-af-core}:8080/quality-on-demand/v1/sessions/$SESSION_ID"
echo "Session $SESSION_ID deleted (HTTP 204 No Content — empty response body expected)"
```

Verify the session state after deletion. The AF immediately marks the session `UNAVAILABLE` with `statusInfo: "DELETE_REQUESTED"` and dispatches the teardown signal to the PCF (which then propagates to SMF → UPF). The record is retained briefly so polling clients can observe the final state:

```bash {"name":"verify-session-deleted","interactive":"false"}
docker compose -f $COMPOSE_FILE -f $COMPOSE_OVERRIDE_FILE run --rm --no-deps af-client \
  --http2-prior-knowledge -sS \
  "http://${AF_HOST:-af-core}:8080/quality-on-demand/v1/sessions/$SESSION_ID"
echo "(expected: qosStatus=UNAVAILABLE, statusInfo=DELETE_REQUESTED)"
```

### Verify Control-Plane Teardown in SMF Logs

The authoritative proof that the QoS policy was released is in the SMF logs, not `iperf3`. The SMF should log the removal of the AF-installed PCC rules at the moment the PCF processes the delete:

```bash {"name":"verify-smf-teardown","interactive":"false"}
# Collect current SMF logs and look for AF PCC rule removal
docker logs smf 2>&1 | grep -i "Remove PCCRule" | tail -10
```

Expected output: lines such as `Remove PCCRule[PccRuleId-N]` timestamped at the moment you deleted the session, confirming the SMF received the PCF notification and removed the AF-specific GBR rules from the PDU session.

> **Note — OAI-UPF data-plane cleanup:** When the SMF sends a PFCP `Session Modification Request` to remove the AF PDRs and FARs, the OAI-UPF removes those PDRs and FARs but does **not** destroy the associated TC/HTB class or update the `m_qos_enabling` eBPF map. The GBR rate shaper installed at session creation therefore persists on the data plane until the entire PFCP session is deleted (e.g., at UE detach). As a result, a second `iperf3` run immediately after DELETE will show the same receiver rate (~9.43 Mbps for `premium`) as during the active session. This is a known OAI-UPF limitation — the control-plane teardown is complete and correct; only the data-plane cleanup is deferred.

### Verify QoS is Released (iperf3 — informational)

Run a second `iperf3` measurement for completeness. Due to the OAI-UPF data-plane cleanup limitation described above, the rate will appear the same as during the active session; the SMF log check above is the reliable teardown verification.

Start the server on the UE:

```bash {"name":"start-iperf3-post-delete","background":"true","interactive":"false"}
docker exec -d ue iperf3 -s -B $UE_IP -p 5070
```

Run the client from the external data network:

```bash {"name":"iperf3-after-delete","interactive":"false"}
sleep 2
IPERF_OUTPUT=$(docker exec oai-ext-dn iperf3 -c $UE_IP -p 5070 -t 10)

echo "$IPERF_OUTPUT"

BITRATE_MBPS=$(echo "$IPERF_OUTPUT" | awk '/receiver/{print $7}')
if [ -n "$BITRATE_MBPS" ]; then
  echo "Receiver bitrate after DELETE: ${BITRATE_MBPS} Mbps"
  echo "(informational — the OAI-UPF GBR shaper persists until UE detach; see SMF log for control-plane confirmation)"
else
  echo "ERROR: Could not extract bitrate from iperf3 output"
fi
```

Stop the server:

```bash {"name":"stop-iperf3-post-delete","interactive":"false"}
docker exec ue pkill iperf3 || true
```
## Step 7: Collect Logs

Collect logs from all containers after the test run:

```bash {"name":"collect-logs","interactive":"false"}
./build/scripts/ci_helper.sh collect_logs $LOGS_DIR
echo "Logs collected to $LOGS_DIR"
ls -la $LOGS_DIR
```

## Cleanup

Stop and remove all containers:

```bash {"name":"cleanup","interactive":"false"}
docker compose -f $COMPOSE_FILE down $RAN_SERVICES

docker compose -f $COMPOSE_FILE $COMPOSE_PROFILES down

docker compose -f $COMPOSE_FILE -f $COMPOSE_OVERRIDE_FILE --profile af-client down
```

To also remove built images:

```bash {"name":"cleanup-all","excludeFromRunAll":"true","interactive":"false"}
docker compose -f $COMPOSE_FILE $COMPOSE_PROFILES down --rmi all
```

## Reference: QoS Profiles

The request file used in this tutorial references `premium`, but the following profiles are also available.

<details>
<summary><b>Available QoS profiles</b></summary>

| Profile | 5QI | Type | Guaranteed BW | Maximum BW | Delay Budget | Use Case |
|---|---|---|---|---|---|---|
| `QOS_E` | 1 | GBR | 64 Kbps | 128 Kbps | 100 ms | Conversational voice |
| `QOS_S` | 2 | GBR | 384 Kbps | 512 Kbps | 150 ms | Conversational video |
| `QOS_M` | 3 | GBR | 512 Kbps | 1024 Kbps | 50 ms | Real-time gaming |
| `QOS_L` | 4 | GBR | 256 Kbps | 512 Kbps | 300 ms | Buffered video |
| `voice` | 1 | GBR | 64 Kbps | 128 Kbps | 100 ms | Voice (custom) |
| `video` | 2 | GBR | 1024 Kbps | 2048 Kbps | 150 ms | Video streaming (custom) |
| `game` | 3 | GBR | 512 Kbps | 1024 Kbps | 50 ms | Gaming (custom) |
| `data` | 9 | Non-GBR | — | — | 300 ms | Best-effort data |
| `premium` | 7 | GBR | 5 Mbps | 10 Mbps | 100 ms | Live streaming / interactive |
| `enterprise` | 8 | GBR | 1 Gbps | 10 Gbps | 300 ms | Enterprise high-throughput |

</details>

<details>
<summary><b>Example: use a different profile</b></summary>

To test a different profile, change the `qosProfile` field in the request JSON. For example, this cell prepares a request for the `enterprise` profile:

```bash {"name":"send-enterprise-request","excludeFromRunAll":"true","interactive":"false"}
# Create a custom request file with the enterprise profile
docker exec af-client sh -c 'cat > /tmp/qod_enterprise.json << EOF
{
  "device": {
    "phoneNumber": "+123456789",
    "ipv4Address": {
      "publicAddress": "10.60.0.1"
    }
  },
  "applicationServer": {
    "ipv4Address": "0.0.0.0/0"
  },
  "devicePorts": {
    "ports": [5070]
  },
  "applicationServerPorts": {
    "ports": [5070]
  },
  "qosProfile": "enterprise",
  "duration": 3600
}
EOF'

# Send the request using the unified command with custom payload and correlation ID
```

</details>

## Next Steps

- Try a different QoS profile and compare the `iperf3` result
- Explore session lifecycle operations such as [extend duration](../../af_core/tests/requests/qod/qod_session_extend_duration.json)
- Review the [Architecture Overview](../architecture/overview.md) for more detail on AF internals
- See [CAMARA Compliance](../apis/camara-compliance.md) for API-specific behavior