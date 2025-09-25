
## Test policy authorization endpoint using curl

```bash
docker exec -it af curl -H 'Content-Type: application/json' -X POST -d '{
    "ascReqData": {
        "afAppId": "08f5bb39-eb54-4f35-a616-926d22f8825b", 
        "notifUri": "http://example.com/notifs",
        "suppFeat": "5",
        "supi": "imsi-208930000000001",
        "ueIpv4": "10.60.0.1",
        "dnn": "internet",
        "sliceInfo": {
				"sst": 1,
				"sd":  "010203"
		},
        "medComponents": {
            "1": {
                "medCompN": 1,
                "medSubComps": {
                    "1": {
                        "fNum": 1,
                        "fStatus": "ENABLED",
                        "fDescs": [
                            "permit out ip from 10.60.0.1 to 192.168.71.0/24"
                        ]
                    }
                },
                "medType": "AUDIO",
                "marBwDl":  "400 Mbps",
                "marBwUl":  "400 Mbps",
                "mirBwDl":  "20 Mbps",
                "mirBwUl":  "20 Mbps"
            }
        }
    }
}' http://pcf.free5gc.org:8000/npcf-policyauthorization/v1/app-sessions
```