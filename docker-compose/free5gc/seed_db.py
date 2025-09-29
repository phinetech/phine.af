import http.client
import json

imsi="imsi-208930000000001"
base_url = "localhost:5000"

conn = http.client.HTTPConnection(base_url)


payload = {
    "plmnID": "20893",
    "ueId": imsi,
    "AuthenticationSubscription": {
        "authenticationManagementField": "8000",
        "authenticationMethod": "5G_AKA",
        "milenage": {"op": {
                "encryptionAlgorithm": 0,
                "encryptionKey": 0,
                "opValue": "8e27b6af0e692e750f32667a3b14605d"
            }},
        "opc": {
            "encryptionAlgorithm": 0,
            "encryptionKey": 0,
            "opcValue": ""
        },
        "permanentKey": {
            "encryptionAlgorithm": 0,
            "encryptionKey": 0,
            "permanentKeyValue": "8baf473f2f8fd09487cccbd7097c6862"
        },
        "sequenceNumber": "16f3b3f70fc2"
    },
    "AccessAndMobilitySubscriptionData": {
        "gpsis": ["msisdn-0900000000"],
        "nssai": {
            "defaultSingleNssais": [
                {
                    "sst": 1,
                    "sd": "010203",
                    "isDefault": True
                },
                {
                    "sst": 1,
                    "sd": "112233",
                    "isDefault": True
                }
            ],
            "singleNssais": []
        },
        "subscribedUeAmbr": {
            "downlink": "2 Gbps",
            "uplink": "1 Gbps"
        }
    },
    "SessionManagementSubscriptionData": [
        {
            "singleNssai": {
                "sst": 1,
                "sd": "010203"
            },
            "dnnConfigurations": {
                "internet": {
                    "sscModes": {
                        "defaultSscMode": "SSC_MODE_1",
                        "allowedSscModes": ["SSC_MODE_2", "SSC_MODE_3"]
                    },
                    "pduSessionTypes": {
                        "defaultSessionType": "IPV4",
                        "allowedSessionTypes": ["IPV4"]
                    },
                    "sessionAmbr": {
                        "uplink": "200 Mbps",
                        "downlink": "100 Mbps"
                    },
                    "5gQosProfile": {
                        "5qi": 9,
                        "arp": {"priorityLevel": 8},
                        "priorityLevel": 8
                    }
                },
                "internet2": {
                    "sscModes": {
                        "defaultSscMode": "SSC_MODE_1",
                        "allowedSscModes": ["SSC_MODE_2", "SSC_MODE_3"]
                    },
                    "pduSessionTypes": {
                        "defaultSessionType": "IPV4",
                        "allowedSessionTypes": ["IPV4"]
                    },
                    "sessionAmbr": {
                        "uplink": "200 Mbps",
                        "downlink": "100 Mbps"
                    },
                    "5gQosProfile": {
                        "5qi": 9,
                        "arp": {"priorityLevel": 8},
                        "priorityLevel": 8
                    }
                }
            }
        },
        {
            "singleNssai": {
                "sst": 1,
                "sd": "112233"
            },
            "dnnConfigurations": {
                "internet": {
                    "sscModes": {
                        "defaultSscMode": "SSC_MODE_1",
                        "allowedSscModes": ["SSC_MODE_2", "SSC_MODE_3"]
                    },
                    "pduSessionTypes": {
                        "defaultSessionType": "IPV4",
                        "allowedSessionTypes": ["IPV4"]
                    },
                    "sessionAmbr": {
                        "uplink": "200 Mbps",
                        "downlink": "100 Mbps"
                    },
                    "5gQosProfile": {
                        "5qi": 9,
                        "arp": {"priorityLevel": 8},
                        "priorityLevel": 8
                    }
                },
                "internet2": {
                    "sscModes": {
                        "defaultSscMode": "SSC_MODE_1",
                        "allowedSscModes": ["SSC_MODE_2", "SSC_MODE_3"]
                    },
                    "pduSessionTypes": {
                        "defaultSessionType": "IPV4",
                        "allowedSessionTypes": ["IPV4"]
                    },
                    "sessionAmbr": {
                        "uplink": "200 Mbps",
                        "downlink": "100 Mbps"
                    },
                    "5gQosProfile": {
                        "5qi": 9,
                        "arp": {"priorityLevel": 8},
                        "priorityLevel": 8
                    }
                }
            }
        }
    ],
    "SmfSelectionSubscriptionData": {"subscribedSnssaiInfos": {
            "01010203": {"dnnInfos": [{"dnn": "internet"}, {"dnn": "internet2"}]},
            "01112233": {"dnnInfos": [{"dnn": "internet"}, {"dnn": "internet2"}]}
        }},
    "AmPolicyData": {"subscCats": ["free5gc"]},
    "SmPolicyData": {"smPolicySnssaiData": {
            "01010203": {
                "snssai": {
                    "sst": 1,
                    "sd": "010203"
                },
                "smPolicyDnnData": {
                    "internet": {"dnn": "internet"},
                    "internet2": {"dnn": "internet2"}
                }
            },
            "01112233": {
                "snssai": {
                    "sst": 1,
                    "sd": "112233"
                },
                "smPolicyDnnData": {
                    "internet": {"dnn": "internet"},
                    "internet2": {"dnn": "internet2"}
                }
            }
        }},
    "FlowRules": []
}

payload = json.dumps(payload)
headers = {
    'Accept': "application/json",
    'Accept-Language': "en-GB,en-US;q=0.9,en;q=0.8",
    'Connection': "keep-alive",
    'Content-Type': "application/json;charset=UTF-8",
    'Token': "admin"
    }

conn.request("POST", "/api/subscriber/{}/20893".format(imsi), payload, headers)

res = conn.getresponse()
data = res.read()

print(data.decode("utf-8"))
