/* Copyright 2020  Ronald Landheer-Cieslak
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License. */
#ifndef dnp3sav6_details_ecdhpublickey_hpp
#define dnp3sav6_details_ecdhpublickey_hpp

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/ec.h>

struct AlgorithmIdentifier
{
    ASN1_OBJECT *algorithm = nullptr;
    ECPARAMETERS *parameters = nullptr;
};
DECLARE_ASN1_FUNCTIONS(AlgorithmIdentifier);

struct ECDHPublicKey
{
    AlgorithmIdentifier *algorithm = nullptr;
    ASN1_BIT_STRING *publicKey = nullptr;
};
DECLARE_ASN1_FUNCTIONS(ECDHPublicKey);

#endif