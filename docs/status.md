# Implementation Status

This page summarises what appears to be implemented vs. not implemented based on the repository documentation and code.

## High-level legend

- **Fully implemented**: Marked as fully implemented in the root architecture diagram.
- **Partially implemented**: Marked as partially implemented in the root architecture diagram.
- **Not implemented**: Marked as not implemented in the root architecture diagram.

## Northbound adapters

| Adapter | Status (per diagram) | Notes |
|---|---|---|
| App-specific API | Partially implemented | The repo contains `northbound/api_component` which documents an HTTP/2 REST API component. |
| TSN | Not implemented | TODO |
| Quality on Demand (QoD) | Fully implemented | `AfOrchestrator` registers `qod_*` message handlers. |
| Device Location | Not implemented | TODO |
| Traffic Influence | Not implemented | TODO |

## Southbound handlers

| Handler | 3GPP reference point (per diagram) | Status (per diagram) | Notes |
|---|---:|---|---|
| PCF Handler | N5 | Fully implemented | The repo has a dedicated module under `southbound/pcf_handler`. |
| NEF Handler | N33 | Not implemented | `AfOrchestrator` creates a NEF comm service entry, but details are TODO/unknown. |
| BSF Handler | Nbsf | Not implemented | TODO |
| TSCTSF Handler | N52 | Not implemented | TODO |
| AMF Handler | Namf | Not implemented | TODO |
| LMF Handler | Nlmf | Not implemented | TODO |
| GMLC Handler | Ngmlc | Not implemented | TODO |
| UDR Handler | Nudr | Not implemented | TODO |

<!---
TODO: Replace “TODO/unknown” entries with concrete evidence from code once those components exist or are wired up.
-->
