---
runme:
  id: qos-enforcement-tutorial
  version: v3
cwd: ../..
---

# End-to-End QoS Enforcement Tutorial

This tutorial walks you through deploying the full 5G testbed, requesting QoS enforcement via the CAMARA QoD API, observing the signalling across the network, and verifying that bandwidth policies are applied on the data plane.

The same workflow supports both phine.af deployment models:

- **Bundled AF**: `af_core` and the southbound PCF handler run inside a single `af` container
- **Microservice AF**: `af_core` and `pcf_handler` run as separate services

The steps below stay the same for both modes. You only switch the deployment by changing `COMPOSE_FILE`.

> **Runme compatible**: This tutorial is designed to run with [Runme](https://runme.dev/) — both interactively in VS Code and non-interactively in CI via the Runme CLI. See the [Runme Guide](../development/runme-guide.md) for details.

By the end of this tutorial you will have:

1. Deployed a complete 5G core (free5GC), RAN simulator (UERANSIM), and the phine.af Application Function in bundled or microservice form
2. Sent a QoD session creation request for the **premium** QoS profile (5 Mbps guaranteed, 10 Mbps max)
3. Captured and traced the policy signalling path: **AF → PCF → SMF → UPF**
4. Verified that the UPF enforces the requested bandwidth on downlink traffic using `iperf3`

## Prerequisites

| Requirement | Notes |
|---|---|
| Docker Engine ≥ 24.0 | With Docker Compose v2 plugin |
| Linux host | Network interface creation requires Linux kernel capabilities |
| Wireshark or `tshark` | For capturing control-plane traffic |
| ~8 GB free RAM | The full 5G core + RAN + AF stack runs ~15 containers |
| Ports available | No services bound to `192.168.70.128/26`, `192.168.71.128/26`, or `192.168.72.128/26` subnets |

## Network Architecture

Both deployment modes expose the same gRPC request entrypoint on `192.168.70.141:50051`. The internal AF path differs depending on which compose file you choose.

```text
                 ┌──────────┐
                 │ grpcurl  │  same QoD request in both modes
                 └────┬─────┘
                      │ gRPC
                      ▼
                  ┌──────────────────────────────┐
                  │        af (.70.141)          |
                  |                              │
                  │    AF Core + PCF Handler     |──────────────┐
                  |                              │              |
                  └──────────────────────────────┘              |
                                                                |
                                                                |
                                                                │ HTTP / N7
                                                                ▼
                                                        ┌──────────────┐
                                                        │ PCF (.70.139)│
                                                        └──────┬───────┘
                                                               │ N7
                                                               ▼
                                                        ┌──────────────┐
                                                        │ SMF (.70.133)│
                                                        └──────┬───────┘
                                                               │ N4 / PFCP
                                                               ▼
                 ┌──────────┐     ┌──────────┐          ┌──────────────┐     ┌──────────┐
                 │    UE    │────▶│   gNB    │─────────▶│  UPF (host)  │────▶│  Ext DN  │
                 │ 10.60.0.1│     │ UERANSIM │          │    (OAI)     │     │  iperf3  │
                 └──────────┘     └──────────┘          └──────────────┘     └──────────┘
```

**Key IP addresses** (from the Docker Compose network `192.168.70.128/26`):

| Container | IP Address | Role |
|---|---|---|
| `af` / `af-core` | 192.168.70.141 | gRPC entrypoint for QoD requests — bundled mode uses `af`, microservice mode uses `af-core` |
| `af-pcf-handler` | 192.168.70.140 | Southbound handler in microservice mode; bundled mode keeps this hop inside `af` |
| `pcf` | 192.168.70.139 | free5GC PCF — policy control function |
| `smf` | 192.168.70.133 | free5GC SMF — session management |
| `upf` | host network | OAI UPF — user plane enforcement |
| `ue` | 192.168.70.181 (control) / 10.60.0.1 (data) | UERANSIM UE |
| `oai-ext-dn` | 192.168.70.135 | External data network (iperf3 client) |

## Configuration

Set up environment variables for this tutorial. Modify these values if you need to test different configurations:

```bash {"name":"setup-variables","interactive":"false"}
# Choose one deployment mode:
#   bundled AF:     AF_PROFILE=af
#   microservice:   AF_PROFILE=afs
export COMPOSE_FILE="docker-compose/compose.yaml"
export AF_PROFILE="af"
export COMPOSE_PROFILES="--profile free5gc --profile $AF_PROFILE"
export RAN_SERVICES="ueransim-gnb ueransim-ue"
export LOGS_DIR="/tmp/phine.af/qos-enforcement-tutorial/logs"
mkdir -p "$LOGS_DIR"
sudo mkdir -p "$LOGS_DIR"
sudo chmod 777 "$LOGS_DIR"

export UE_IP="10.60.0.1"
export EXT_DN_IP="192.168.72.135"
export MIN_MBPS=3
export MAX_MBPS=12
echo "Configuration set:"
echo "  COMPOSE_FILE: $COMPOSE_FILE"
echo "  AF_PROFILE: $AF_PROFILE"
echo "  COMPOSE_PROFILES: $COMPOSE_PROFILES"
echo "  RAN_SERVICES: $RAN_SERVICES"
echo "  CAPTURE_DIR: $LOGS_DIR"
echo "  UE_IP: $UE_IP"
echo "  EXT_DN_IP: $EXT_DN_IP"
echo "  Bandwidth validation range: ${MIN_MBPS}-${MAX_MBPS} Mbps"
```

Use [docker-compose/compose.yaml](../../docker-compose/compose.yaml) with `AF_PROFILE=af` for the bundled `af` container, or `AF_PROFILE=afs` for separate `af_core` and `pcf_handler` services. The rest of the tutorial is unchanged.

## Step 1: Deploy the Setup

Install required dependencies and build the gtp5g kernel module:

```bash {"name":"install-deps","interactive":"false"}
./build/scripts/ci_helper.sh install_dependencies
./build/scripts/ci_helper.sh install_gtp5g
```

Ensure submodules are up to date
```bash {"name":"check-submodules","interactive":"false"}
./build/scripts/ci_helper.sh check_submodules
```

Build and start all containers:

```bash {"name":"deploy-stack","interactive":"false"}
docker compose -f $COMPOSE_FILE $COMPOSE_PROFILES up -d --build

sleep 30

docker compose -f $COMPOSE_FILE up -d $RAN_SERVICES
```

Wait for all services to become healthy. The retry loop ensures we don't proceed until the stack is ready:

```bash {"name":"wait-for-healthy","interactive":"false"}
sleep 30
docker compose -f $COMPOSE_FILE $COMPOSE_PROFILES ps
```

Verify the UE has registered and obtained an IP address:

```bash {"name":"verify-ue-ip","interactive":"false"}
sleep 15
docker exec ue ip addr show uesimtun0 | grep -q "10.60.0.1"
```

Verify network connectivity between UE and external data network:

```bash {"name":"verify-connectivity","interactive":"false"}
docker exec ue ping -I uesimtun0 192.168.72.135 -c 3
```

## Step 2: Start Traffic Capture

Start capturing the control-plane signalling:

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

This filter captures:

- **HTTP traffic** from the AF path to the PCF (`.70.139`) — from `.70.141` in bundled mode or `.70.140` in microservice mode
- **PFCP traffic** between the SMF (`.70.133`) and the UPF — the session modification that installs QoS rules
- Any additional signalling on the control plane

> **Tip**: Keep the capture running for the rest of the tutorial so you can trace the full end-to-end flow.

## Step 3: Send a QoD Session Request

Send a QoD create session request for the **premium** profile. This profile provides:

- **Guaranteed bandwidth**: 5 Mbps uplink + 5 Mbps downlink
- **Maximum bandwidth**: 10 Mbps uplink + 10 Mbps downlink
- **5QI**: 7 (Interactive Gaming / Live Streaming)
- **Packet Delay Budget**: 100 ms

### The Request Payload

The request payload is in `af_core/tests/requests/qod/qod_create_session.json`:

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

Key fields:

- **`device.ipv4Address.publicAddress`**: The UE's data-plane IP (`10.60.0.1`)
- **`devicePorts.ports`**: Includes port `5070` which we will use for iperf3 testing
- **`qosProfile`**: `"premium"` — maps to 5 Mbps GBR with 10 Mbps MBR
- **`duration`**: 3600 seconds (1 hour)

### Send the Request

```bash {"name":"send-qod-request","interactive":"false"}
docker run --rm --network host \
  -v ./common/protos:/var/protos/ \
  fullstorydev/grpcurl -plaintext \
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

You should receive a gRPC response with the session details. The `qos_status` will initially be `"REQUESTED"` and transition to `"AVAILABLE"` once the PCF confirms the policy is applied.

Wait for the QoS policy to propagate through the signalling chain. In bundled mode this stays inside `af` before reaching the PCF. In microservice mode it passes through `af_core` and `pcf_handler` as separate services:

```bash {"name":"wait-for-qos-policy","interactive":"false"}
echo "Waiting for QoS policy to propagate through signalling chain..."
sleep 10
echo "QoS policy propagation wait complete"
```

Stop the traffic capture:

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

## Step 4: Trace the Signalling Path

Review the captured traffic to trace the QoD signalling flow:

```bash {"name":"view-capture","interactive":"false","excludeFromRunAll":"true"}
PCAP_FILE="$LOGS_DIR/adapter_capture.pcapng"
# Open file with wireshark
wireshark $PCAP_FILE
```

### Expected Signalling Flow

You should see the following sequence of events:

```text
grpcurl ──gRPC──▶ AF entrypoint (.141)
           │
           ├─ bundled mode:       HTTP POST /npcf-policyauthorization/v1/app-sessions ──▶ PCF (.139)
           │
           └─ microservice mode:  gRPC ──▶ PCF Handler (.140)
                          │
                          └─ HTTP POST /npcf-policyauthorization/v1/app-sessions ──▶ PCF (.139)
                                                         │
                                                         │ HTTP (N7 policy update to SMF)
                                                         ▼
                                                       SMF (.133)
                                                         │
                                                         │ PFCP Session Modification Request
                                                         ▼
                                                       UPF (host)
                                                         │
                                                         │ PFCP Session Modification Response
                                                         ▼
                                                       SMF (.133)
```

### What to Look For in the Capture

1. **HTTP POST to PCF** (`.141` → `.139` in bundled mode, or `.140` → `.139` in microservice mode): Look for `/npcf-policyauthorization/v1/app-sessions` in the request URI. The body should contain:
    - `medComponents` with `MediaSubComponent` entries
    - `fDescs` containing IPFilterRule flow descriptions like:
      ```text
      permit out ip from 10.60.0.1 5070 to 0.0.0.0/0 5070
      permit out ip from 0.0.0.0/0 5070 to 10.60.0.1 5070
      ```
    - `marBwDl` / `marBwUl` set to `"10 Mbps"` and `mirBwDl` / `mirBwUl` set to `"5 Mbps"`

2. **PFCP Session Modification** (`.133` → UPF): The SMF sends a PFCP Session Modification Request to the UPF containing:
    - Updated QER (QoS Enforcement Rule) with the requested MBR/GBR values
    - PDR (Packet Detection Rule) updates for the matching flow

3. **PFCP Response** (UPF → `.133`): Confirms the QoS rule was installed on the user plane

## Step 5: Verify QoS Enforcement with iperf3

Now that the QoS policy is applied, verify that the UPF enforces the bandwidth limit on downlink traffic.

> **Important**: QoS enforcement in this setup applies to **downlink (DL) traffic only** — traffic from the external data network toward the UE. This is because the UPF enforces QoS on the GTP-U encapsulated downlink path.

### Run the iperf3 Bandwidth Test

Start the iperf3 server on the UE in the background:

```bash {"name":"start-iperf3-server","background":"true","interactive":"false"}
docker exec -d ue iperf3 -s -B $UE_IP -p 5070
```

Run the iperf3 client from the external data network to measure bandwidth:

```bash {"excludeFromRunAll":"true","interactive":"true"}
docker exec oai-ext-dn iperf3 -c $UE_IP -p 5070 -t 10
echo
```

Observe the throughput in the output. With QoS enforcement active, you should see the bitrate capped around 5-10 Mbps (matching the premium profile limits).

<details>
<summary><b>Automated Validation Script (for CI/Testing)</b></summary>

The following script automatically validates that bandwidth is being rate-limited. This runs in CI but can also be executed manually:

```bash {"name":"run-iperf3-client","interactive":"false"}
sleep 2

# Run iperf3 client and capture JSON output
IPERF_OUTPUT=$(docker exec oai-ext-dn iperf3 -c $UE_IP -p 5070 -t 10 --json)

# Extract receiver bitrate (bits_per_second) and convert to Mbps
BITRATE_BPS=$(echo "$IPERF_OUTPUT" | jq -r '.end.sum_received.bits_per_second // empty')

if [ -z "$BITRATE_BPS" ]; then
  echo "ERROR: Failed to extract bitrate from iperf3 output"
  exit 1
fi

BITRATE_MBPS=$(echo "scale=4; $BITRATE_BPS / 1000000" | bc)

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

Stop the iperf3 server:

```bash {"name":"stop-iperf3-server","interactive":"false"}
docker exec ue pkill iperf3 || true
```

### Interpreting the Results

**With QoS enforcement active**, you should see the throughput capped around the profile limits:

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

The throughput should be limited to approximately **5 Mbps** (the guaranteed bitrate) or up to **10 Mbps** (the maximum bitrate) depending on network conditions.

**Without QoS enforcement** (for comparison), the throughput would typically be much higher (limited only by the Docker bridge network and CPU).

> **Note**: The exact numbers may vary slightly depending on your hardware, but the key observation is that the bitrate is being capped rather than running at full link speed.

### Optional: Run a Baseline Comparison

To see the difference, you can run iperf3 on a port that is **not** in the QoD session's port list (e.g., port 9000):

```bash {"name":"run-baseline-iperf3","excludeFromRunAll":"true","interactive":"false"}
# Start iperf3 server on UE on an unrestricted port
docker exec -d ue iperf3 -s -B $UE_IP -p 9000

# Start iperf3 client on data network
docker exec oai-ext-dn iperf3 -c $UE_IP -p 9000 -t 10
```

## Available QoS Profiles

The following QoS profiles are available for use in the `qosProfile` field:

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

To use a different profile, change the `qosProfile` field in your request JSON. For example, to test the `enterprise` profile:

```bash {"name":"send-enterprise-request","excludeFromRunAll":"true","interactive":"false"}
cat > /tmp/qod_enterprise.json << EOF
{
  "device": {
    "phoneNumber": "+123456789",
    "ipv4Address": {
      "publicAddress": "$UE_IP"
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
EOF

docker run --rm --network host \
  -v ./common/protos:/var/protos/ \
  -v /tmp/qod_enterprise.json:/tmp/qod_enterprise.json \
  fullstorydev/grpcurl -plaintext \
  -proto message.proto \
  -import-path /var/protos \
  -d '{
     "message_type": "qod_create_session",
     "correlation_id": "67890",
     "payload": "'$(cat /tmp/qod_enterprise.json | base64 -w 0)'",
     "metadata": {
      "source": "command_line",
      "priority": "high"
    }
  }' \
  192.168.70.141:50051 \
  af.proto.InternalCommunication/SendMessage
```

## Step 6: Collect Logs

After the run, collect logs from all containers for analysis:

```bash {"name":"collect-logs","interactive":"false"}
./build/scripts/ci_helper.sh collect_logs $LOGS_DIR
echo "Logs collected to $LOGS_DIR"
ls -la $LOGS_DIR
```

## Cleanup

To stop and remove all containers:

```bash {"name":"cleanup","interactive":"false"}
docker compose -f $COMPOSE_FILE down $RAN_SERVICES

docker compose -f $COMPOSE_FILE $COMPOSE_PROFILES down
```

To also remove built images:

```bash {"name":"cleanup-all","excludeFromRunAll":"true","interactive":"false"}
docker compose -f $COMPOSE_FILE $COMPOSE_PROFILES down --rmi all
```

## Next Steps

- Try different QoS profiles and compare iperf3 results
- Explore session lifecycle operations: [extend duration](../../af_core/tests/requests/qod/qod_session_extend_duration.json), retrieve sessions
- Review the [Architecture Overview](../architecture/overview.md) for details on how the AF components interact
- See the [CAMARA Compliance](../apis/camara-compliance.md) documentation for API specification details
