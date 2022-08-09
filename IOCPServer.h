// Reference : https://github.com/jacking75/edu_cpp_IOCP/blob/master/Tutorial/01/IOCompletionPort.h

#pragma once
#pragma comment(lib, "ws2_32")
#pragma warning(disable: 4819)
#include <winsock2.h>
#include <Ws2tcpip.h>

#include <thread>
#include <vector>
#include <cstdio>

#define MAX_SOCKBUF 1024 // ��Ŷ�� ũ��
#define MAX_WORKERTHREAD 4 // �����忡 ���� �������� ��

enum class IOOperation{
    RECV,
    SEND
};

//WSAOVERLAPPED ����ü�� Ȯ���� �ʿ��� ���� �߰�
struct stOverlappedEx{
    WSAOVERLAPPED   m_wasOverlapped;    //Overlapped IO ����üü
    SOCKET          m_socketClient;     // Ŭ���̾�Ʈ ����
    WSABUF          m_wsaBuf;           //Overlapped IO ����
    char            m_szBuf[ MAX_SOCKBUF ]; // ������ ����
    IOOperation     m_eOperation;       //�۾� ���� ����
};

//Ŭ���̾�Ʈ ������ ��� ����ü
struct stClientInfo
{
    SOCKET          m_socketClient;     //Client�� ����Ǵ� ����
    stOverlappedEx  m_stRecvOverlappedEx; //RECV Overlapped IO�۾��� ���� ����
    stOverlappedEx  m_stSendOverlappedEx; //SEND Overlapped IO�۾��� ���� ����

    stClientInfo(){
        ZeroMemory(&m_stRecvOverlappedEx, sizeof(stOverlappedEx));
        ZeroMemory(&m_stSendOverlappedEx, sizeof(stOverlappedEx));
        m_socketClient = INVALID_SOCKET;
    }
};

class IOCPServer
{

    std::vector<stClientInfo>   mClientInfos;
    SOCKET                      mListenSocket = INVALID_SOCKET;
    int                         mClientCnt = 0;
    std::vector<std::thread>    mIOWorkThreads; //IOWorker ������
    std::thread                 mAccepterThread;
    HANDLE                      mIOCPHandle = INVALID_HANDLE_VALUE; //Compelete Port ��ü ��ȯ
    bool                        mIsWorkRun = true;
    bool                        mIsAccepterRun = true;
    char                        mSocketBuf[1024] = {0,}; // ���Ϲ���



public:
    IOCPServer(void){}

    ~IOCPServer(void){
        WSACleanup();
    }

    bool InitSocket(){
        WSADATA wsaData; //������ ���� ����

        int nRet = WSAStartup(MAKEWORD(2,2), &wsaData);
        if(0 != nRet){
            printf("[����] WSAStartup()�Լ� ���� : %d\n", WSAGetLastError());
            return false;
        }

        mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

        if(INVALID_SOCKET == mListenSocket){
            printf("[����] socket()�Լ� ���� : %d\n", WSAGetLastError());
        }

        printf("���� �ʱ�ȭ ����!");
        return true;
    }

    //----------------������ �ڵ�-----------------

    bool BindandListen(int nBindPort){
        SOCKADDR_IN         stServerAddr;
        stServerAddr.sin_family = AF_INET;
        stServerAddr.sin_port = htons(nBindPort);
        //������ ��Ʈ�� �����ϰ� � �����̶� �޾Ƶ��̰ڴ�.
        //Ư�� �����Ǹ� �ް� �ʹٸ� inet_addr�Լ��� �̿��� �ִ´�.
        stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
        if(0 != nRet){
            printf("[����] listen()�Լ� ���� : %d\n", WSAGetLastError());
            return false;
        }

        //���ӿ�û�� �޾Ƶ��̱� ���� cIOCompletePort ������ ����ϰ�
        //���Ӵ��ť�� 5���� �����Ѵ�.
        nRet = listen(mListenSocket, 5);
        if(0 != nRet)
        {
            printf("[����] listen()�Լ� ���� : %d\n", WSAGetLastError());
        }

        printf("���� ��� ����");
        return true;
    }

    bool StartServer(const UINT32 maxClientCount){
        CreateClient(maxClientCount);

        mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,NULL, MAX_WORKERTHREAD);
        if(NULL == mIOCPHandle){
            printf("[����] CreateIoCompletionPort()�Լ� ���� : %d\n", GetLastError());
            return false;
        }

        bool bRet = CreateWorkThread();
        if(bRet == false){
            return false;
        }

        bRet = CreateAccepterThread();
        if(false == bRet){
            return false;
        }

        printf("��������\n");
        return true;
    }

    //�����Ǵ� �����带 �ı��Ѵ�.
    void DestoryThread(){
        mIsWorkRun = false;
        CloseHandle(mIOCPHandle);

        for(auto& th : mIOWorkThreads){
            if(th.joinable()){
                th.join();
            }
        }

        //Accepter �����带 �����Ѵ�.
        mIsAccepterRun = false;
        closesocket(mListenSocket);

        if(mAccepterThread.joinable()){
            mAccepterThread.join();
        }
    }

    virtual void OnConnect(const UINT32 clientIndex_){}

    virtual void OnClose (const UINT32 clientIndex_) {}

    virtual void OnReceive (const UINT32 clientIndex_, const UINT32 size_, char* pData_){}


private:
    void CreateClient(const UINT32 maxClientCount){
        for(UINT32 i = 0; i < maxClientCount; i++){
            mClientInfos.emplace_back();
        }
    }

    //Waiting Thread Queue���� ����� ������� ����
    bool CreateWorkThread(){
        unsigned int uiThreadId = 0;
        //WaitingThread Queue���� �����·� ������ ������ ���� ���� : (cpu ���� * 2) + 1
        for(int i = 0; i < MAX_WORKERTHREAD; i++){
            mIOWorkThreads.emplace_back([this](){ WorkThread(); });
        }

        printf("WorkerThread ����...\n");
        return true;
   }

   bool CreateAccepterThread(){
        mAccepterThread = std::thread([this]() {AcceptThread();});

        printf("AccepterThread ����...\n");
        return true;
    }

    //������� �ʴ� Ŭ���̾�Ʈ ����ü�� ��ȯ
    stClientInfo* GetEmptyClientInfo()
    {
        for( auto& client : mClientInfos)
        {
            if(INVALID_SOCKET == client.m_socketClient){
                return &client;
            }
        }
        return nullptr;
    }

    //CompletionPort��ü�� ���ϰ� CompletionKey�� �����Ű�� ������ �ϴ� �Լ�
    bool BindIOCompletionPort(stClientInfo* pClientInfo)
    {
        auto hIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->m_socketClient, mIOCPHandle, (ULONG_PTR)(pClientInfo), 0);

        if(NULL == hIOCP || mIOCPHandle != hIOCP)
        {
            printf("[����] CreateIoCompletionPort()�Լ� ���� : %d\n", GetLastError());
            return false;
        }

        return true;
    }

    bool BindRecv(stClientInfo* pClientInfo){
        DWORD dwFlag = 0;
        DWORD dwRecvNumBytes = 0;

        //Overlapped IO ���� ����
        pClientInfo -> m_stRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
        pClientInfo -> m_stRecvOverlappedEx.m_wsaBuf.buf = pClientInfo->m_stRecvOverlappedEx.m_szBuf;
        pClientInfo -> m_stRecvOverlappedEx.m_eOperation = IOOperation::RECV;

        int nRet = WSARecv(pClientInfo->m_socketClient,
                           &(pClientInfo->m_stRecvOverlappedEx.m_wsaBuf),
                           1,
                           &dwRecvNumBytes,
                           &dwFlag,
                           (LPWSAOVERLAPPED) &(pClientInfo->m_stRecvOverlappedEx),
                           NULL);
        //socket_error�̸� client socket�� �������� ���� ó���Ѵ�.
        if(nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING)){
            printf("[����] CreateIoCompletionPort()�Լ� ���� : %d\n",WSAGetLastError());
            return false;
        }

        return true;
    }

    bool SendMsg(stClientInfo* pClientInfo, char* pMsg, int nLen){
        DWORD dwRecvNumBytes = 0;

        CopyMemory(pClientInfo->m_stSendOverlappedEx.m_szBuf, pMsg, nLen);

        pClientInfo -> m_stSendOverlappedEx.m_wsaBuf.len = nLen;
        pClientInfo -> m_stSendOverlappedEx.m_wsaBuf.buf = pClientInfo->m_stSendOverlappedEx.m_szBuf;
        pClientInfo ->m_stSendOverlappedEx.m_eOperation = IOOperation::SEND;

        int nRet = WSASend(pClientInfo -> m_socketClient,
                           &(pClientInfo->m_stSendOverlappedEx.m_wsaBuf),
                           1,
                           &dwRecvNumBytes,
                           0,
                           (LPWSAOVERLAPPED) & (pClientInfo->m_stSendOverlappedEx),
                           NULL
                           );

        if(nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING)){
            printf("[����] WSASend()�Լ� ���� : %d\n",WSAGetLastError());
            return false;
        }

        return true;
    }

    //Overlapped IO�۾��� ���� �Ϸ� �뺸�� �޾� ó���ϴ� �Լ�
    void WorkThread(){
        //Completion Key�� ���� ������ ����
        stClientInfo* pClientInfo = NULL;
        //�Լ� ȣ�� ���� ����
        BOOL bSuccess = TRUE;
        //Overlapped IO�۾����� ���۵� ������ ũ��
        DWORD dwIoSize = 0;
        //IO�۾��� ���� ��û�� Overlapped ����ü�� ���� ������
        LPOVERLAPPED lpoverlapped = NULL;

        while(mIsWorkRun){
            //�ش��Լ��� ���� WaitingThread Queue�� �����·� ���� �ȴ�.
            //�Ϸ�� Overlapped IO�۾��� �߻��ϸ� IOCP Queue���� �Ϸ�� �۾��� ������ ó���Ѵ�.
            //PostQueuedCompletionStatus() �Լ��� ���� ����� �޼����� �����ϸ� �����带 �����Ѵ�.

            bSuccess= GetQueuedCompletionStatus(mIOCPHandle,
                                                &dwIoSize, //������ ���۵� ����Ʈ
                                                (PULONG_PTR)&pClientInfo, //CompletionKey
                                                &lpoverlapped,//Overlapped IO ��ü
                                                INFINITE // ����ҽð�
                                                );
            //����� ������ �޼��� ���� ó����
            if(TRUE == bSuccess && 0 == dwIoSize && NULL == lpoverlapped){
                mIsWorkRun = false;
                continue;
            }

            if(NULL == lpoverlapped)
            {
                continue;
            }

            if(FALSE == bSuccess || (0 == dwIoSize && TRUE == bSuccess)){
                printf("socket(%d) ���� ����\n" , (int)pClientInfo->m_socketClient);
                CloseSocket(pClientInfo);
                continue;
            }

            stOverlappedEx* pOverlappedEx = (stOverlappedEx*)lpoverlapped;

            //Overlapped IO Recv �۾���� ��ó��
            if(IOOperation::RECV == pOverlappedEx->m_eOperation){
                pOverlappedEx->m_szBuf[dwIoSize] = NULL;
                printf("[����] bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);

                SendMsg(pClientInfo, pOverlappedEx->m_szBuf, dwIoSize);
                BindRecv(pClientInfo);
            }
                //Overlapped IO Send �۾���� ��ó��
            else if (IOOperation::SEND == pOverlappedEx->m_eOperation){
                printf("[�۽�] bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);
            }
            //���ܻ�Ȳ
            else{
                printf("socket(%d)���� ���ܻ�Ȳ\n", (int)pClientInfo->m_socketClient);
            }
        }
    }
    //����� ������ �޴� ������
    void AcceptThread(){
        SOCKADDR_IN         stClientAddr;
        int nAddrLen = sizeof(SOCKADDR_IN);

        while(mIsAccepterRun){
            //���� ���� ����ü�� �ε����� ���´�.
            stClientInfo* pClientInfo = GetEmptyClientInfo();
            if(NULL == pClientInfo){
                printf("[����] Client Full\n");
                return ;
            }
            //Ŭ���̾�Ʈ ���� ��û�� ���ö����� ��ٸ���.
            pClientInfo->m_socketClient = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
            if(INVALID_SOCKET == pClientInfo->m_socketClient)
                continue;

            //IO Completion Port ��ü�� ������ �����Ų��.
            bool bRet = BindIOCompletionPort(pClientInfo);
            if(false == bRet)
                return;

            bRet = BindRecv(pClientInfo);
            if(false == bRet)
                return;

            char clientIP[32] = {0,};
            inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
            printf("Ŭ���̾�Ʈ ���� : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_socketClient);

            mClientCnt++;

        }
    }



    void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false)
    {
        struct linger stLinger = {0,0}; // SO_DONTLINGER�� ����

        //bIsForce�� true�̸� SO_LINGER, timeout = 0���� �����Ͽ� ������ ����
        if( true == bIsForce){
            stLinger.l_onoff = 1;
        }

        //������ �ۼ����� ��� �ߴܽ�Ų��.
        shutdown(pClientInfo->m_socketClient, SD_BOTH);

        setsockopt(pClientInfo->m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof (stLinger));

        closesocket(pClientInfo->m_socketClient);

        pClientInfo->m_socketClient = INVALID_SOCKET;

    }
};

