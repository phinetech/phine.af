db = db.getSiblingDB('oai_db_mongo');

/******************************************
*
* AccessAndMobilitySubscriptionData
*
*******************************************/
// Schema for collection 'AccessAndMobilitySubscriptionData'
db.createCollection('AccessAndMobilitySubscriptionData', {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["ueid", "servingPlmnid"],
            properties: {
                ueid: {
                    bsonType: "string",
                    description: "must be a string and is required"
                },
                servingPlmnid: {
                    bsonType: "string",
                    description: "must be a string and is required"
                },
                supportedFeatures: {
                    bsonType: "string",
                    description: "must be a string"
                },
                gpsis: {
                    bsonType: "object",
                    description: "must be an object"
                },
                internalGroupIds: {
                    bsonType: "object",
                    description: "must be an object"
                },
                sharedVnGroupDataIds: {
                    bsonType: "object",
                    description: "must be an object"
                },
                subscribedUeAmbr: {
                    bsonType: "object",
                    description: "must be an object"
                },
                nssai: {
                    bsonType: "object",
                    description: "must be an object"
                },
                ratRestrictions: {
                    bsonType: "object",
                    description: "must be an object"
                },
                forbiddenAreas: {
                    bsonType: "object",
                    description: "must be an object"
                },
                serviceAreaRestriction: {
                    bsonType: "object",
                    description: "must be an object"
                },
                coreNetworkTypeRestrictions: {
                    bsonType: "object",
                    description: "must be an object"
                },
                rfspIndex: {
                    bsonType: "int",
                    description: "must be an integer"
                },
                subsRegTimer: {
                    bsonType: "int",
                    description: "must be an integer"
                },
                ueUsageType: {
                    bsonType: "int",
                    description: "must be an integer"
                },
                mpsPriority: {
                    bsonType: "bool",
                    description: "must be a boolean"
                },
                mcsPriority: {
                    bsonType: "bool",
                    description: "must be a boolean"
                },
                activeTime: {
                    bsonType: "int",
                    description: "must be an integer"
                },
                sorInfo: {
                    bsonType: "object",
                    description: "must be an object"
                },
                sorInfoExpectInd: {
                    bsonType: "bool",
                    description: "must be a boolean"
                },
                sorafRetrieval: {
                    bsonType: "bool",
                    description: "must be a boolean"
                },
                sorUpdateIndicatorList: {
                    bsonType: "object",
                    description: "must be an object"
                },
                upuInfo: {
                    bsonType: "object",
                    description: "must be an object"
                },
                micoAllowed: {
                    bsonType: "bool",
                    description: "must be a boolean"
                },
                sharedAmDataIds: {
                    bsonType: "object",
                    description: "must be an object"
                },
                odbPacketServices: {
                    bsonType: "object",
                    description: "must be an object"
                },
                serviceGapTime: {
                    bsonType: "int",
                    description: "must be an integer"
                },
                mdtUserConsent: {
                    bsonType: "object",
                    description: "must be an object"
                },
                mdtConfiguration: {
                    bsonType: "object",
                    description: "Configuration for MDT"
                },
                traceData: {
                    bsonType: "object",
                    description: "Trace data"
                },
                cagData: {
                    bsonType: "object",
                    description: "CAG data"
                },
                stnSr: {
                    bsonType: "string",
                    description: "STN-SR"
                },
                cMsisdn: {
                    bsonType: "string",
                    description: "Combined MSISDN"
                },
                nbIoTUePriority: {
                    bsonType: "int",
                    description: "NB-IoT UE priority"
                },
                nssaiInclusionAllowed: {
                    bsonType: "bool",
                    description: "Flag indicating if NSSAI inclusion is allowed"
                },
                rgWirelineCharacteristics: {
                    bsonType: "string",
                    description: "Characteristics of RG wireline"
                },
                ecRestrictionDataWb: {
                    bsonType: "object",
                    description: "Restriction data for EC on WB"
                },
                ecRestrictionDataNb: {
                    bsonType: "bool",
                    description: "Flag indicating if EC restriction is allowed on NB"
                },
                expectedUeBehaviourList: {
                    bsonType: "object",
                    description: "List of expected UE behaviours"
                },
                primaryRatRestrictions: {
                    bsonType: "object",
                    description: "Primary RAT restrictions"
                },
                secondaryRatRestrictions: {
                    bsonType: "object",
                    description: "Secondary RAT restrictions"
                },
                edrxParametersList: {
                    bsonType: "object",
                    description: "List of eDRX parameters"
                },
                ptwParametersList: {
                    bsonType: "object",
                    description: "List of PTW parameters"
                },
                iabOperationAllowed: {
                    bsonType: "bool",
                    description: "Flag indicating if IAB operation is allowed"
                },
                wirelineForbiddenAreas: {
                    bsonType: "object",
                    description: "Forbidden wireline areas"
                },
                wirelineServiceAreaRestriction: {
                    bsonType: "object",
                    description: "Restriction for wireline service areas"
                }
            }
        }
    }
});

// Indexes for collection 'AccessAndMobilitySubscriptionData'
db.AccessAndMobilitySubscriptionData.createIndex({ ueid: 1, servingPlmnid: 1 });

// Inserts for collection 'AccessAndMobilitySubscriptionData'
db.AccessAndMobilitySubscriptionData.insertMany([
    {
        ueid: '208950000000125',
        servingPlmnid: '20895',
        nssai: {
            defaultSingleNssais: [{
                'sst': 1,
                'sd': '000001'
            }]
        }
    },
    {
        ueid: '208950000000126',
        servingPlmnid: '20895',
        nssai: {
            defaultSingleNssais: [{
                'sst': 1,
                'sd': '000001'
            }]
        }
    },
    {
        ueid: '208950000000127',
        servingPlmnid: '20895',
        nssai: {
            defaultSingleNssais: [{
                'sst': 1,
                'sd': '000001'
            }]
        }
    },
    {
        ueid: '208950000000128',
        servingPlmnid: '20895',
        nssai: {
            defaultSingleNssais: [{
                'sst': 1,
                'sd': '000001'
            }]
        }
    },
    {
        ueid: '208950000000129',
        servingPlmnid: '20895',
        nssai: {
            defaultSingleNssais: [{
                'sst': 1,
                'sd': '000001'
            }]
        }
    },
]);


/******************************************
*
* Amf3GppAccessRegistration
*
*******************************************/
// Schema for collection 'Amf3GppAccessRegistration'
db.createCollection("Amf3GppAccessRegistration", {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["ueid", "amfInstanceId", "deregCallbackUri", "guami", "ratType"],
            properties: {
                ueid: {
                    bsonType: "string",
                    description: "Unique identifier for the UE"
                },
                amfInstanceId: {
                    bsonType: "string",
                    description: "AMF instance identifier"
                },
                supportedFeatures: {
                    bsonType: "string",
                    description: "Supported features"
                },
                purgeFlag: {
                    bsonType: "bool",
                    description: "Purge flag"
                },
                pei: {
                    bsonType: "string",
                    description: "Pei"
                },
                imsVoPs: {
                    bsonType: "object",
                    description: "IMS VoPs"
                },
                deregCallbackUri: {
                    bsonType: "string",
                    description: "Deregistration callback URI"
                },
                amfServiceNameDereg: {
                    bsonType: "object",
                    description: "AMF service name for deregistration"
                },
                pcscfRestorationCallbackUri: {
                    bsonType: "string",
                    description: "PCSCF restoration callback URI"
                },
                amfServiceNamePcscfRest: {
                    bsonType: "object",
                    description: "AMF service name for PCSCF restoration"
                },
                initialRegistrationInd: {
                    bsonType: "bool",
                    description: "Initial registration indicator"
                },
                guami: {
                    bsonType: "object",
                    description: "Globally unique AMF identifier"
                },
                backupAmfInfo: {
                    bsonType: "object",
                    description: "Backup AMF information"
                },
                drFlag: {
                    bsonType: "bool",
                    description: "DR flag"
                },
                ratType: {
                    bsonType: "string",
                    description: "Radio access technology type"
                },
                urrpIndicator: {
                    bsonType: "bool",
                    description: "URRP indicator"
                },
                amfEeSubscriptionId: {
                    bsonType: "string",
                    description: "AMF EE subscription ID"
                },
                epsInterworkingInfo: {
                    bsonType: "object",
                    description: "EPS interworking information"
                },
                ueSrvccCapability: {
                    bsonType: "bool",
                    description: "UE SRVCC capability"
                },
                registrationTime: {
                    bsonType: "string",
                    description: "Registration time"
                },
                vgmlcAddress: {
                    bsonType: "object",
                    description: "VGMLC address"
                },
                contextInfo: {
                    bsonType: "object",
                    description: "Context information"
                },
                noEeSubscriptionInd: {
                    bsonType: "bool",
                    description: "No EE subscription indicator"
                }
            }
        }
    }
});

// Index for collection 'Amf3GppAccessRegistration'
db.Amf3GppAccessRegistration.createIndex({ ueid: 1 });

// Inserts for collection 'Amf3GppAccessRegistration'


/******************************************
*
* AuthenticationStatus
*
*******************************************/
// Schema for collection 'AuthenticationStatus'
db.createCollection("AuthenticationStatus", {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["ueid", "nfInstanceId", "success", "timeStamp", "authType", "servingNetworkName"],
            properties: {
                ueid: {
                    bsonType: "string",
                    description: "Unique identifier for the UE"
                },
                nfInstanceId: {
                    bsonType: "string",
                    description: "NF instance identifier"
                },
                success: {
                    bsonType: "bool",
                    description: "Authentication success status"
                },
                timeStamp: {
                    bsonType: "string",
                    description: "Time stamp"
                },
                authType: {
                    bsonType: "string",
                    description: "Authentication type"
                },
                servingNetworkName: {
                    bsonType: "string",
                    description: "Serving network name"
                },
                authRemovalInd: {
                    bsonType: "bool",
                    description: "Authentication removal indicator"
                }
            }
        }
    }
});

// Index for collection 'AuthenticationStatus'
db.AuthenticationStatus.createIndex({ ueid: 1 });

// Inserts for collection 'AuthenticationStatus'


/******************************************
*
* AuthenticationSubscription
*
********************************************/
// Schema for collection 'AuthenticationSubscription'
db.createCollection("AuthenticationSubscription", {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["ueid", "authenticationMethod"],
            properties: {
                ueid: {
                    bsonType: "string",
                    description: "UE ID"
                },
                authenticationMethod: {
                    bsonType: "string",
                    description: "Authentication Method"
                },
                encPermanentKey: {
                    bsonType: "string",
                    description: "Encrypted Permanent Key"
                },
                protectionParameterId: {
                    bsonType: "string",
                    description: "Protection Parameter ID"
                },
                sequenceNumber: {
                    bsonType: "object",
                    description: "Sequence Number",
                    additionalProperties: true
                },
                authenticationManagementField: {
                    bsonType: "string",
                    description: "Authentication Management Field"
                },
                algorithmId: {
                    bsonType: "string",
                    description: "Algorithm ID"
                },
                encOpcKey: {
                    bsonType: "string",
                    description: "Encrypted OPC Key"
                },
                encTopcKey: {
                    bsonType: "string",
                    description: "Encrypted TOPC Key"
                },
                vectorGenerationInHss: {
                    bsonType: "bool",
                    description: "Vector Generation in HSS"
                },
                n5gcAuthMethod: {
                    bsonType: "string",
                    description: "N5GC Authentication Method"
                },
                rgAuthenticationInd: {
                    bsonType: "bool",
                    description: "RG Authentication Indicator"
                },
                supi: {
                    bsonType: "string",
                    description: "SUPI"
                }
            }
        }
    }
});

// Index for collection 'AuthenticationSubscription'
db.AuthenticationSubscription.createIndex({ ueid: 1 });

// Inserts for collection 'AuthenticationSubscription'
db.AuthenticationSubscription.insertMany([
    {"ueid": "208950000000031",
    "authenticationMethod": "5G_AKA",
    "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
    "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
    "sequenceNumber":
        {"sqn": "000000000020",
            "sqnScheme": "NON_TIME_BASED",
            "lastIndexes": {"ausf": 0} },
    "authenticationManagementField": "8000",
    "algorithmId": "milenage",
    "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
    "supi": "208950000000031"  },
    {"ueid": "208950000000032",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000032"  },
    {"ueid": "208950000000033",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000033"  },
    {"ueid": "208950000000034",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000034"  },
    {"ueid": "208950000000035",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000035"  },
    {"ueid": "208950000000036",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000036"  },
    {"ueid": "208950000000037",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000037"  },
    {"ueid": "208950000000038",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000038"  },
    {"ueid": "208950000000039",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000039"  },
    {"ueid": "208950000000040",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000040"  },
    {"ueid": "208950000000041",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000041"  },
    {"ueid": "208950000000042",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000042"  },
    {"ueid": "208950000000043",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000043"  },
    {"ueid": "208950000000044",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000044"  },
    {"ueid": "208950000000045",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000045"  },
    {"ueid": "208950000000046",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000046"  },
    {"ueid": "208950000000047",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000047"  },
    {"ueid": "208950000000048",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000048"  },
    {"ueid": "208950000000049",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000049"  },
    {"ueid": "208950000000050",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000050"  },
    {"ueid": "208950000000051",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000051"  },
    {"ueid": "208950000000052",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000052"  },
    {"ueid": "208950000000053",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000053"  },
    {"ueid": "208950000000054",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000054"  },
    {"ueid": "208950000000055",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000055"  },
    {"ueid": "208950000000056",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000056"  },
    {"ueid": "208950000000057",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000057"  },
    {"ueid": "208950000000058",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000058"  },
    {"ueid": "208950000000059",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000059"  },
    {"ueid": "208950000000060",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000060"  },
    {"ueid": "208950000000061",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000061"  },
    {"ueid": "208950000000062",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000062"  },
    {"ueid": "208950000000063",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000063"  },
    {"ueid": "208950000000064",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000064"  },
    {"ueid": "208950000000065",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000065"  },
    {"ueid": "208950000000066",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000066"  },
    {"ueid": "208950000000067",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000067"  },
    {"ueid": "208950000000068",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000068"  },
    {"ueid": "208950000000069",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000069"  },
    {"ueid": "208950000000070",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000070"  },
    {"ueid": "208950000000071",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000071"  },
    {"ueid": "208950000000072",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000072"  },
    {"ueid": "208950000000073",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000073"  },
    {"ueid": "208950000000074",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000074"  },
    {"ueid": "208950000000075",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000075"  },
    {"ueid": "208950000000076",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000076"  },
    {"ueid": "208950000000077",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000077"  },
    {"ueid": "208950000000078",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000078"  },
    {"ueid": "208950000000079",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000079"  },
    {"ueid": "208950000000080",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000080"  },
    {"ueid": "208950000000081",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000081"  },
    {"ueid": "208950000000082",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000082"  },
    {"ueid": "208950000000083",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000083"  },
    {"ueid": "208950000000084",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000084"  },
    {"ueid": "208950000000085",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000085"  },
    {"ueid": "208950000000086",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000086"  },
    {"ueid": "208950000000087",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000087"  },
    {"ueid": "208950000000088",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000088"  },
    {"ueid": "208950000000089",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000089"  },
    {"ueid": "208950000000090",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000090"  },
    {"ueid": "208950000000091",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000091"  },
    {"ueid": "208950000000092",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000092"  },
    {"ueid": "208950000000093",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000093"  },
    {"ueid": "208950000000094",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000094"  },
    {"ueid": "208950000000095",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000095"  },
    {"ueid": "208950000000096",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000096"  },
    {"ueid": "208950000000097",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000097"  },
    {"ueid": "208950000000098",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000098"  },
    {"ueid": "208950000000099",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000099"  },
    {"ueid": "208950000000100",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000100"  },
    {"ueid": "208950000000101",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000101"  },
    {"ueid": "208950000000102",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000102"  },
    {"ueid": "208950000000103",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000103"  },
    {"ueid": "208950000000104",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000104"  },
    {"ueid": "208950000000105",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000105"  },
    {"ueid": "208950000000106",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000106"  },
    {"ueid": "208950000000107",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000107"  },
    {"ueid": "208950000000108",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000108"  },
    {"ueid": "208950000000109",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000109"  },
    {"ueid": "208950000000111",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000111"  },
    {"ueid": "208950000000113",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000113"  },
    {"ueid": "208950000000114",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000114"  },
    {"ueid": "208950000000115",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000115"  },
    {"ueid": "208950000000116",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000116"  },
    {"ueid": "208950000000117",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000117"  },
    {"ueid": "208950000000118",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000118"  },
    {"ueid": "208950000000119",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000119"  },
    {"ueid": "208950000000120",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000120"  },
    {"ueid": "208950000000121",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000121"  },
    {"ueid": "208950000000122",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000122"  },
    {"ueid": "208950000000123",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000123"  },
    {"ueid": "208950000000124",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000124"  },
    {"ueid": "208950000000125",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000125"  },
    {"ueid": "208950000000126",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000126"  },
    {"ueid": "208950000000127",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000127"  },
    {"ueid": "208950000000128",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000128"  },
    {"ueid": "208950000000129",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000129"  },
    {"ueid": "208950000000130",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000130"  },
    {"ueid": "208950000000131",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000131"  },
    {"ueid": "208950000000132",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000132"  },
    {"ueid": "208950000000133",
        "authenticationMethod": "569 milliseconds69 millisecondsG_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000133"  },
    {"ueid": "208950000000134",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "208950000000134"  },
    {"ueid": "001019000000000",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "465b5ce8b199b49faa5f0a2ee238a6bc",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "0000000005e0",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "cd63cb71954a9f4e48a5994e37a02baf",
        "supi": "001019000000000"  },
    {"ueid": "001019000000001",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "465b5ce8b199b49faa5f0a2ee238a6bc",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "000000000020",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "cd63cb71954a9f4e48a5994e37a02baf",
        "supi": "001019000000001"  },
    {"ueid": "001019000000002",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "0000000005e0",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "001019000000002"  },
    {"ueid": "001019000000003",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "0000000005e0",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "001019000000003"  },
    {"ueid": "001019000000004",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "0000000005e0",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "001019000000004"  },
    {"ueid": "001019000000005",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "0000000005e0",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "001019000000005"  },
    {"ueid": "001019000000006",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "0000000005e0",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "001019000000006"  },
    {"ueid": "001019000000007",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "0000000005e0",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "001019000000007"  },
    {"ueid": "001019000000008",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "0000000005e0",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "001019000000008"  },
    {"ueid": "001019000000009",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "0000000005e0",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "001019000000009"  },
    {"ueid": "001019000000010",
        "authenticationMethod": "5G_AKA",
        "encPermanentKey": "0C0A34601D4F07677303652C0462535B",
        "protectionParameterId": "0C0A34601D4F07677303652C0462535B",
        "sequenceNumber":
            {"sqn": "0000000005e0",
                "sqnScheme": "NON_TIME_BASED",
                "lastIndexes": {"ausf": 0} },
        "authenticationManagementField": "8000",
        "algorithmId": "milenage",
        "encOpcKey": "63bfa50ee6523365ff14c1f45f88737d",
        "supi": "001019000000010"  }

]);


/******************************************
*
* SdmSubscriptions
*
*******************************************/
// Schema for collection 'SdmSubscriptions'
db.createCollection("SdmSubscriptions", {
        validator: {
            $jsonSchema: {
                bsonType: "object",
                required: ["ueid", "subsId", "nfInstanceId", "callbackReference", "monitoredResourceUris"],
                properties: {
                    ueid: { bsonType: "string", description: "Network function instance identifier" },
                    implicitUnsubscribe: { bsonType: "bool", description: "Indicates whether the subscription is implicitly unsubscribed" },
                    expires: { bsonType: "string", description: "Expiration date/time of subscription" },
                    callbackReference: { bsonType: "string", description: "Callback reference of subscription" },
                    amfServiceName: { bsonType: "object", description: "AMF service name of subscription" },
                    monitoredResourceUris: { bsonType: "array", description: "Array of monitored resource URIs" },
                    singleNssai: { bsonType: "object", description: "Single Network Slice Selection Assistance Information" },
                    dnn: { bsonType: "string", description: "Data Network Name of subscription" },
                    subscriptionId: { bsonType: "string", description: "Subscription ID of subscription" },
                    plmnId: { bsonType: "object", description: "Public Land Mobile Network ID of subscription" },
                    immediateReport: { bsonType: "bool", description: "Indicates whether immediate reports are enabled for the subscription" },
                    report: { bsonType: "object", description: "Report configuration of subscription" },
                    supportedFeatures: { bsonType: "string", description: "Supported features of subscription" },
                    contextInfo: { bsonType: "object", description: "Context information of subscription" }
                }
            }
        }
    }
);

// Index for collection 'SdmSubscriptions'
db.SdmSubscriptions.createIndex({ subsId: 1, ueid: 1 });

db.SdmSubscriptions.createIndex({ subsId: 1 }, { unique: true });
db.SdmSubscriptions.find().forEach(function(doc) {
    const nextValue = db.SdmSubscriptions.find().count() + 1;
    db.SdmSubscriptions.update({ _id: doc._id }, { $set: { subsId: nextValue } });
});

// Inserts for collection 'SdmSubscriptions'


/******************************************
 *
 * SessionManagementSubscriptionData
 *
 *******************************************/
// Schema for collection 'SessionManagementSubscriptionData'
db.createCollection('SessionManagementSubscriptionData', {
        validator: {
            $jsonSchema: {
                bsonType: "object",
                required: [ "ueid", "servingPlmnid", "singleNssai", "dnnConfigurations" ],
                properties: {
                    ueid: {
                        bsonType: "string",
                        description: "Unique UE ID"
                    },
                    servingPlmnid: {
                        bsonType: "string",
                        description: "Serving PLMN ID"
                    },
                    singleNssai: {
                        bsonType: "object",
                        description: "Single Network Slice Selection Assistance Information"
                    },
                    dnnConfigurations: {
                        bsonType: "object",
                        description: "DNN (Data Network Name) Configurations"
                    },
                    internalGroupIds: {
                        bsonType: "object",
                        description: "Internal Group Identifiers"
                    },
                    sharedVnGroupDataIds: {
                        bsonType: "object",
                        description: "Shared VN (Virtual Network) Group Data Identifiers"
                    },
                    sharedDnnConfigurationsId: {
                        bsonType: "string",
                        description: "Shared DNN Configurations Identifier"
                    },
                    odbPacketServices: {
                        bsonType: "object",
                        description: "ODB (Overload Data Bit) Packet Services"
                    },
                    traceData: {
                        bsonType: "object",
                        description: "Trace Data"
                    },
                    sharedTraceDataId: {
                        bsonType: "string",
                        description: "Shared Trace Data Identifier"
                    },
                    expectedUeBehavioursList: {
                        bsonType: "object",
                        description: "Expected UE Behaviours List"
                    },
                    suggestedPacketNumDlList: {
                        bsonType: "object",
                        description: "Suggested Packet Number Downlink List"
                    },
                    threegppChargingCharacteristics: {
                        bsonType: "string",
                        description: "3GPP Charging Characteristics"
                    }
                }
            }
        }
    });

// Index for collection 'SessionManagementSubscriptionData'
db.SessionManagementSubscriptionData.createIndex({ ueid: 1, servingPlmnid: 1 });

// Inserts for collection 'SessionManagementSubscriptionData'
db.SessionManagementSubscriptionData.insertMany([
    {
        ueid: "208950000000031",
        servingPlmnid: "20895",
        singleNssai: {
            sst: 222,
            sd: "00007B"
        },
        dnnConfigurations: {
            default: {
                pduSessionTypes: {
                    defaultSessionType: "IPV4"
                },
                sscModes: {
                    defaultSscMode: "SSC_MODE_1"
                },
                "5gQosProfile": {
                    "5qi": 6,
                    arp: {
                        priorityLevel: 1,
                        preemptCap: "NOT_PREEMPT",
                        preemptVuln: "NOT_PREEMPTABLE"
                    },
                    priorityLevel: 1
                },
                sessionAmbr: {
                    uplink: "100Mbps",
                    downlink: "100Mbps"
                },
                staticIpAddress: [
                    {
                        ipv4Addr: "12.1.1.4"
                    }
                ]
            }
        }
    },
    {
        ueid: "208950000000032",
        servingPlmnid: "20895",
        singleNssai: {
            sst: 222,
            sd: "00007B"
        },
        dnnConfigurations: {
            default: {
                pduSessionTypes: {
                    defaultSessionType: "IPV4"
                },
                sscModes: {
                    defaultSscMode: "SSC_MODE_1"
                },
                "5gQosProfile": {
                    "5qi": 6,
                    arp: {
                        priorityLevel: 1,
                        preemptCap: "NOT_PREEMPT",
                        preemptVuln: "NOT_PREEMPTABLE"
                    },
                    priorityLevel: 1
                },
                sessionAmbr: {
                    uplink: "100Mbps",
                    downlink: "100Mbps"
                }
            }
        }
    },
    {
        ueid: "001019000000000",
        servingPlmnid: "00101",
        singleNssai: {
            sst: 1,
            sd: "000001"
        },
        dnnConfigurations: {
            default: {
                pduSessionTypes: {
                    defaultSessionType: "IPV4"
                },
                sscModes: {
                    defaultSscMode: "SSC_MODE_1"
                },
                "5gQosProfile": {
                    "5qi": 6,
                    arp: {
                        priorityLevel: 1,
                        preemptCap: "NOT_PREEMPT",
                        preemptVuln: "NOT_PREEMPTABLE"
                    },
                    priorityLevel: 1
                },
                sessionAmbr: {
                    uplink: "200Mbps",
                    downlink: "400Mbps"
                },
                staticIpAddress: [
                    {
                        ipv4Addr: "12.1.1.154"
                    }
                ]
            }
        }
    }
]);


/******************************************
 *
 * SmfRegistrations
 *
 *******************************************/
// Schema for collection 'SmfRegistrations'
db.createCollection("SmfRegistrations", {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["ueid", "subpduSessionId", "smfInstanceId", "singleNssai", "pduSessionId", "plmnId"],
            properties: {
                ueid: {
                    bsonType: "string",
                    description: "UE ID"
                },
                subpduSessionId: {
                    bsonType: "int",
                    description: "Sub PDU session ID"
                },
                smfInstanceId: {
                    bsonType: "string",
                    description: "SMF instance ID"
                },
                smfSetId: {
                    bsonType: "string",
                    description: "SMF set ID"
                },
                supportedFeatures: {
                    bsonType: "string",
                    description: "Supported features"
                },
                pduSessionId: {
                    bsonType: "int",
                    description: "PDU session ID"
                },
                singleNssai: {
                    bsonType: "object",
                    description: "Single Network Slice Selection Assistance Information"
                },
                dnn: {
                    bsonType: "string",
                    description: "Data Network Name"
                },
                emergencyServices: {
                    bsonType: "bool",
                    description: "Indicates if emergency services are supported"
                },
                pcscfRestorationCallbackUri: {
                    bsonType: "string",
                    description: "PCF restoration callback URI"
                },
                plmnId: {
                    bsonType: "object",
                    description: "Public Land Mobile Network Identifier"
                },
                pgwFqdn: {
                    bsonType: "string",
                    description: "PDN Gateway Fully Qualified Domain Name"
                },
                epdgInd: {
                    bsonType: "bool",
                    description: "Indicates if ePDG is supported"
                },
                deregCallbackUri: {
                    bsonType: "string",
                    description: "Deregistration callback URI"
                },
                registrationReason: {
                    bsonType: "object",
                    description: "Registration reason"
                },
                registrationTime: {
                    bsonType: "string",
                    description: "Registration time"
                },
                contextInfo: {
                    bsonType: "object",
                    description: "Context information"
                }
            }
        }
    }
});

// Index for collection 'SmfRegistrations'
db.SmfRegistrations.createIndex({ ueid: 1, subpduSessionId: 1 });

// Inserts for collection 'SmfRegistrations'


/******************************************
 *
 * SmfSelectionSubscriptionData
 *
 *******************************************/
// Schema for collection 'SmfSelectionSubscriptionData'
db.createCollection("SmfSelectionSubscriptionData", {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["ueid", "servingPlmnid", "supportedFeatures"],
            properties: {
                ueid: {
                    bsonType: "string",
                    description: "UE identifier"
                },
                servingPlmnid: {
                    bsonType: "string",
                    description: "PLMN identifier"
                },
                supportedFeatures: {
                    bsonType: "string",
                    description: "Supported features"
                },
                subscribedSnssaiInfos: {
                    bsonType: "object",
                    description: "Subscribed S-NSSAI information"
                },
                sharedSnssaiInfosId: {
                    bsonType: "string",
                    description: "ID of shared S-NSSAI information"
                }
            }
        }
    }
});

// Index for collection 'SmfSelectionSubscriptionData'
db.SmfSelectionSubscriptionData.createIndex({ ueid: 1, servingPlmnid: 1 });

// Inserts for collection 'SmfSelectionSubscriptionData'
