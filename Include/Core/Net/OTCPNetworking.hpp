/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once
#include "_NetCommon.hpp"

enum OTCPError
{
    kTcpErrorNoError,
    kTcpErrorExitTimeout,
    kTcpErrorExitClosed,
    kTcpErrorExitRemoteClose,
    kTcpErrorExitGenericFailure,
    kTcpErrorOpenGenericFailure,
    kTcpErrorSendGenericFailure,
    kTcpErrorSendSocketClose
};

struct OTCPBaseCallback_s
{
    struct
    {
        bool present;
        OTCPError code;
    } error;
};

typedef struct OTCPCloseInfo_s : OTCPBaseCallback_s
{

} OTCPCloseInfo_t, *OTCPCloseInfo_p;

typedef struct OTCPOpenInfo_s : OTCPBaseCallback_s
{

} OTCPOpenInfo_t, *OTCPOpenInfo_p;

typedef struct OTCPRecvInfo_s : OTCPBaseCallback_s
{
    const void * buffer;
    size_t length;
} OTCPRecvInfo_t, *OTCPRecvInfo_p;

typedef struct OTCPSendInfo_s : OTCPBaseCallback_s
{
    const void * priv;
} OTCPSendInfo_t, *OTCPSendInfo_p;

class OTCPClient : public OObject
{
    typedef void(*OTCPCloseCb_p)(OTCPCloseInfo_t &, OPtr<OTCPClient>);
    typedef void(*OTCPOpenCb_p)(OTCPOpenInfo_t &, OPtr<OTCPClient>);
    typedef void(*OTCPRecvCb_p)(OTCPRecvInfo_t &, OPtr<OTCPClient>);
    typedef void(*OTCPSendCb_p)(OTCPSendInfo_t &, OPtr<OTCPClient>);

    virtual error_t TryOpen(OTransportAddress_t & address)                                = 0;
    virtual error_t TryClose()                                                            = 0;
    virtual error_t TryTransmit(const void * buffer, size_t length, const void * priv)    = 0;

    virtual error_t OnOpen(OTCPOpenCb_p)                                                  = 0;
    virtual error_t OnClose(OTCPCloseCb_p)                                                = 0;
    virtual error_t OnRecv(OTCPRecvCb_p)                                                  = 0;
    virtual error_t OnSend(OTCPSendCb_p)                                                  = 0;

    virtual error_t RunCallbacks()                                                        = 0;
};

LIBLINUX_SYM error_t CreateNewTCPClient(const OOutlivableRef<OTCPClient> out);