# Implementation Status

This page summarises what appears to be implemented vs. not implemented based on the repository documentation and code.

## High-level legend

- **Fully implemented**: Marked as fully implemented in the root architecture diagram.
- **Partially implemented**: Marked as partially implemented in the root architecture diagram.
- **Not implemented**: Marked as not implemented in the root architecture diagram.

## Northbound APIs

| API | Status (per diagram) | Notes                                                       |
|---|---|-------------------------------------------------------------|
| App-specific API | Partially implemented | The repo contains `northbound/api_component` — an HTTP/2 REST API service. |
| Quality on Demand (QoD) | Fully implemented | `AfOrchestrator` registers `qod_*` message handlers. |
| Device Location | Not implemented | Comming Soon |
| TSN | Not implemented | Roadmap |
| Traffic Influence | Not implemented | Roadmap |

## Southbound handlers

| Handler | 3GPP reference point (per diagram) | Status (per diagram) | Notes |
|---|---:|---|---|
| PCF Handler | N5 | Fully implemented | The repo has a dedicated module under `southbound/pcf_handler`. |
| NEF Handler | N33 | Not implemented | Roadmap. `AfOrchestrator` reserves a NEF communication service entry to make wiring straightforward once implemented. |
| BSF Handler | Nbsf | Not implemented | Roadmap |
| TSCTSF Handler | N52 | Not implemented | Roadmap |
| AMF Handler | Namf | Not implemented | Roadmap |
| LMF Handler | Nlmf | Not implemented | Roadmap |
| GMLC Handler | Ngmlc | Not implemented | Roadmap |
| UDR Handler | Nudr | Not implemented | Roadmap |

Contributions welcome — see [add a southbound handler](development/add-southbound-handler.md).