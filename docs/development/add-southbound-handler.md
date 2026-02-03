# Add a new southbound handler

## What a southbound handler is

A southbound handler isolates AF Core from NF-specific protocols and model mapping.

## Example: PCF handler module

The PCF handler module:

- Provides an interface between AF and PCF using the Npcf_PolicyAuthorization API.
- Communicates over HTTP/2 to PCF.

<!---
TODO: Provide a repository-driven checklist for new handlers (CMake targets, docker packaging, communication wiring) by inspecting `southbound/*` modules beyond PCF.
-->
