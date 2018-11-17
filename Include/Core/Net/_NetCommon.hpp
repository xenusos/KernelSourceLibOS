/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

enum OAddressingProtocol
{
    kAddrIPV4,
    kAddrIPV6, 
    kAddrApple
};

typedef struct OTransportAddress
{
    uint16_t port;
    OAddressingProtocol defintion;
    union
    {
#pragma pack(push, 1)
        struct
        {
            union
            {
                uint8_t parts[4];
                struct
                {
                    uint8_t a;
                    uint8_t b;
                    uint8_t c;
                    uint8_t d;
                };
                uint32_t IP;
            };
        } ip_4;
        struct
        {
            union
            {
                uint8_t buf[16];
                uint16_t parts[8];
                struct
                {
                    uint16_t a;
                    uint16_t b;
                    uint16_t c;
                    uint16_t d;
                    uint16_t e;
                    uint16_t f;
                    uint16_t g;
                    uint16_t h;
                };
            };
        } ip_6;
#pragma pack(pop)
    };
} OTransportAddress_t, *OTransportAddress_p;