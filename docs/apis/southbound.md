# Southbound APIs (3GPP interfaces)

## Handlers and reference points

<!---
TODO: Update the table to show more information about the handler needed and their dependencies, or rather the CAMARA APIs that make use of them.
-->

| Handler | Reference point |
|---|---:|
| PCF Handler | N5 |
| NEF Handler | N33 |
| BSF Handler | Nbsf |
| TSCTSF Handler | N52 |
| AMF Handler | Namf |
| LMF Handler | Nlmf |
| GMLC Handler | Ngmlc |
| UDR Handler | Nudr |

## Implemented Southbound Modules

The following southbound handlers are currently implemented:

- **PCF Handler**: Located in `southbound/pcf_handler`. Implements the `N5` reference point (Npcf_PolicyAuthorization) to communicate with the Policy Control Function.

### Future/Planned Modules

<!---
TODO: Document NEF/BSF/TSCTSF handler implementations once they exist as modules or are wired up in `af_core`.
-->
