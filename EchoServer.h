#pragma once

#include "IOCPServer.h"

class EchoServer : public IOCPServer{
    virtual void OnConnect(const UINT32 clientIndex_) override{
        printf("[OnConnect] : Index(%d)", clientIndex_);
    }
    virtual void OnClose(const UINT32 clientIndex_) override{
        printf("[OnClose] : Index(%d)", clientIndex_);
    }
    virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) override{
        printf("[OnReceive] : Index(%d)", clientIndex_);
    }

};