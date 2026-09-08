# APIs overview


The project is designed to expose 3GPP network capabilities to third-party applications using the CAMARA API standard, implemented through northbound APIs, AF core services, and southbound handlers.

The HTTP surface today is delivered by [`northbound/api_component/`](../../northbound/api_component/README.md).
See [Northbound APIs](northbound.md) for the endpoint list and internal
message mapping, and [CAMARA compliance](camara-compliance.md) for the
spec references. OpenAPI specs are not yet published — the CAMARA
Quality-on-Demand upstream specs at
<https://github.com/camaraproject/QualityOnDemand> are the authoritative
schemas the QoD endpoints follow.