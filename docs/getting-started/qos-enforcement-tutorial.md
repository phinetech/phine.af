# End-to-End QoS Enforcement Tutorial

This tutorial walks you through deploying the full 5G testbed, requesting QoS enforcement via the CAMARA QoD API, observing the signalling across the network, and verifying that bandwidth policies are applied on the data plane.

By the end of this tutorial you will have:

1. Deployed a complete 5G core (free5GC), RAN simulator (UERANSIM), and the phine.af Application Function
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

```
                 ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
                 │    UE    │────▶│   gNB    │────▶│   UPF    │────▶│  Ext DN  │
                 │ 10.60.0.1│     │ UERANSIM │     │ (OAI)    │     │ iperf3   │
                 └──────────┘     └──────────┘     └──────────┘     └──────────┘
                                                        ▲
                                                        │ N4 (PFCP)
                                                        │
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│ AF Core  │────▶│PCF Handler│───▶│   PCF    │────▶│   SMF    │
│ .70.141  │gRPC │  .70.140  │HTTP│  .70.139 │ N7  │  .70.133 │
└──────────┘     └──────────┘     └──────────┘     └──────────┘
      ▲
      │ gRPC
      │
┌──────────┐
│ grpcurl  │  (your terminal)
└──────────┘
```

**Key IP addresses** (from the Docker Compose network `192.168.70.128/26`):

| Container | IP Address | Role |
|---|---|---|
| `af-core` | 192.168.70.141 | AF Core — receives CAMARA QoD requests |
| `af-pcf-handler` | 192.168.70.140 | Southbound handler — translates to PCF API |
| `pcf` | 192.168.70.139 | free5GC PCF — policy control function |
| `smf` | 192.168.70.133 | free5GC SMF — session management |
| `upf` | host network | OAI UPF — user plane enforcement |
| `ue` | 192.168.70.181 (control) / 10.60.0.1 (data) | UERANSIM UE |
| `oai-ext-dn` | 192.168.70.135 | External data network (iperf3 client) |

## Step 1: Deploy the Setup

Build and start all containers:

```bash
docker compose -f docker-compose/docker-compose-free5gc-build.yaml up -d --build
```

Wait for all services to become healthy (~30–60 seconds):

```bash
docker compose -f docker-compose/docker-compose-free5gc-build.yaml ps
```

Verify the UE has registered and obtained an IP address:

```bash
docker exec -it ue ip addr show uesimtun0
```

You should see `10.60.0.1` assigned to the tunnel interface. If not, wait a few more seconds — the UE starts with a 15-second delay after the gNB.

Verify network connection

```bash
docker exec -it ue ping -I uesimtun0 192.168.70.135 -c 2
```

## Step 2: Start Traffic Capture

Open Wireshark (or `tshark`) and capture on the **`demo-oai`** bridge interface. Use the following display filter to observe control-plane signalling:

### Wireshark (GUI)

1. Open Wireshark
2. Select the **`demo-oai`** interface
3. Apply display filter:
   ```
   pfcp || http || (ip.src == 192.168.70.140 || ip.dst == 192.168.70.140)
   ```

### tshark (CLI alternative)

```bash
sudo tshark -i demo-oai -f "host 192.168.70.140 or host 192.168.70.139 or host 192.168.70.133" -Y "pfcp || http"
```

This filter captures:

- **HTTP traffic** between the PCF Handler (`.70.140`) and the PCF (`.70.139`) — the Npcf_PolicyAuthorization API calls
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

```json
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

```bash
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

## Step 4: Trace the Signalling Path

Go back to your Wireshark/tshark capture. You should see the following sequence of events:

### Expected Signalling Flow

```
grpcurl ──gRPC──▶ AF Core (.141)
                     │
                     │ gRPC (internal)
                     ▼
              PCF Handler (.140)
                     │
                     │ HTTP POST /npcf-policyauthorization/v1/app-sessions
                     ▼
                 PCF (.139)
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

1. **HTTP POST to PCF** (`.140` → `.139`): Look for `/npcf-policyauthorization/v1/app-sessions` in the request URI. The body should contain:
    - `medComponents` with `MediaSubComponent` entries
    - `fDescs` containing IPFilterRule flow descriptions like:
      ```
      permit out ip from 10.60.0.1 5070 to 0.0.0.0/0 5070
      permit in ip from 0.0.0.0/0 5070 to 10.60.0.1 5070
      ```
    - `marBwDl` / `marBwUl` set to `"10 Mbps"` and `mirBwDl` / `mirBwUl` set to `"5 Mbps"`

2. **PFCP Session Modification** (`.133` → UPF): The SMF sends a PFCP Session Modification Request to the UPF containing:
    - Updated QER (QoS Enforcement Rule) with the requested MBR/GBR values
    - PDR (Packet Detection Rule) updates for the matching flow

3. **PFCP Response** (UPF → `.133`): Confirms the QoS rule was installed on the user plane

## Step 5: Verify QoS Enforcement with iperf3

Now that the QoS policy is applied, verify that the UPF enforces the bandwidth limit on downlink traffic.

> **Important**: QoS enforcement in this setup applies to **downlink (DL) traffic only** — traffic from the external data network toward the UE. This is because the UPF enforces QoS on the GTP-U encapsulated downlink path.

### Start the iperf3 Server on the UE

```bash
docker exec -it ue iperf3 -s -B 10.60.0.1 -p 5070
```

This binds the iperf3 server to the UE's data-plane IP on port `5070` (one of the ports in our QoD request).

### Start the iperf3 Client on the External Data Network

In a separate terminal:

```bash
docker exec -it oai-ext-dn iperf3 -c 10.60.0.1 -p 5070
```

This sends downlink traffic from the external data network through the UPF to the UE.

### Interpreting the Results

**With QoS enforcement active**, you should see the throughput capped around the profile limits:

```
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

```bash
# Server on UE (different port)
docker exec -it ue iperf3 -s -B 10.60.0.1 -p 9000

# Client on data network
docker exec -it oai-ext-dn iperf3 -c 10.60.0.1 -p 9000
```

This traffic should **not** be rate-limited, showing a significantly higher throughput — confirming that the QoS enforcement is port-specific and working correctly.

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

```bash
# Create a custom request
cat > /tmp/qod_enterprise.json << 'EOF'
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
EOF

# Send the request
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

## Cleanup

To stop and remove all containers:

```bash
docker compose -f docker-compose/docker-compose-free5gc-build.yaml down
```

To also remove built images:

```bash
docker compose -f docker-compose/docker-compose-free5gc-build.yaml down --rmi all
```

## Troubleshooting

### UE does not get an IP address

- Check that the gNB and AMF are running: `docker logs gnb` and `docker logs amf`
- The UE has a 15-second startup delay — wait and retry
- Verify the UERANSIM config matches the free5GC subscriber data

### No HTTP traffic visible in Wireshark

- Make sure you are capturing on the correct interface: **`demo-oai`**
- Check the PCF Handler logs: `docker logs af-pcf-handler`
- Check AF Core logs: `docker logs af-core`

### iperf3 connection refused

- Verify the UE tunnel interface is up: `docker exec -it ue ip addr show uesimtun0`
- Make sure the iperf3 server is running before starting the client
- Check that port `5070` matches the ports in your QoD request

### No bandwidth limiting observed

- Check that the QoD session was created successfully (look for `"AVAILABLE"` status)
- Verify PFCP Session Modification in the capture — if absent, the policy did not reach the UPF
- Check SMF logs: `docker logs smf`
- Check UPF logs: `docker logs upf`

## Next Steps

- Try different QoS profiles and compare iperf3 results
- Explore session lifecycle operations: [extend duration](../../af_core/tests/requests/qod/qod_session_extend_duration.json), retrieve sessions
- Review the [Architecture Overview](../architecture/overview.md) for details on how the AF components interact
- See the [CAMARA Compliance](../apis/camara-compliance.md) documentation for API specification details
