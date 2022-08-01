// Reference : https://github.com/jacking75/edu_cpp_IOCP/blob/master/Tutorial/01/IOCompletionPort.h

#pragma once
#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <Ws2tcpip.h>

#include <thread>
#include <vector>

#define MAX_SOCKBUF 1024 // 패킷의 크기
#define MAX_WORKERTHREAD 4 // 스레드에 넣을 쓰레드의 수

enum class IOOperation{
    RECV,
    SEND
};

//WSAOVERLAPPED 구조체를 확장해 필요한 정보 추가
struct stOverlappedEx{
    WSAOVERLAPPED   m_wasOverlapped;    //Overlapped IO 구조체체
    SOCKET          m_socketClient;     // 클라이언트 소켓
    WSABUF          m_wsaBuf;           //Overlapped IO 버퍼
    char            m_szBuf[ MAX_SOCKBUF ]; // 데이터 버퍼
    IOOperation     m_eOperation;       //작업 동작 종류
};

//클라이언트 정보를 담는 구조체
struct stClientInfo
{
    SOCKET          m_socketClient;     //Client와 연결되는 소켓
    stOverlappedEx  m_stRecvOverlappedEx; //RECV Overlapped IO작업을 위한 변수
    stOverlappedEx  m_stSendOverlappedEx; //SEND Overlapped IO작업을 위한 변수

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
    std::vector<std::thread>    mIOWorkThreads; //IOWorker 쓰레드
    std::thread                 mAccepterThread;
    HANDLE                      mIOCPHandle = INVALID_HANDLE_VALUE; //Compelete Port 객체 반환
    bool                        mIsWorkRun = true;
    bool                        mIsAccepterRun = true;
    char                        mSocketBuf[1024] = {0,}; // 소켓버퍼



public:
    IOCPServer(void){}

    ~IOCPServer(void){
        WSACleanup();
    }

    bool InitSocket(){
        WSADATA wsaData; //윈도우 소켓 정보

        int nRet = WSAStartup(MAKEWORD(2,2), &wsaData);
        if(0 != nRet){
            printf("[에러] WSAStartup()함수 실패 : %d\n", WSAGetLastError());
            return false;
        }

        mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

        if(INVALID_SOCKET == mListenSocket){
            printf("[에러] socket()함수 실패 : %d\n", WSAGetLastError());
        }

        printf("소켓 초기화 성공!");
        return true;
    }

    //----------------서버용 코드-----------------

    bool BindandListen(int nBindPort){
        SOCKADDR_IN         stServerAddr;
        stServerAddr.sin_family = AF_INET;
        stServerAddr.sin_port = htons(nBindPort);
        //서버의 포트를 설정하고 어떤 접속이라도 받아들이겠다.
        //특정 아이피를 받고 싶다면 inet_addr함수를 이용해 넣는다.
        stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
        if(0 != nRet){
            printf("[에러] listen()함수 실패 : %d\n", WSAGetLastError());
            return false;
        }

        //접속요청을 받아들이기 위해 cIOCompletePort 소켓을 등록하고
        //접속대기큐를 5개로 설정한다.
        nRet = listen(mListenSocket, 5);
        if(0 != nRet)
        {
            printf("[에러] listen()함수 실패 : %d\n", WSAGetLastError());
        }

        printf("서버 등록 성공");
        return true;
    }

    bool StartServer(const UINT32 maxClientCount){
        CreateClient(maxClientCount);

        mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,NULL, MAX_WORKERTHREAD);
        if(NULL == mIOCPHandle){
            printf("[에러] CreateIoCompletionPort()함수 실패 : %d\n", GetLastError());
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

        printf("서버시작\n");
        return true;
    }

    //생성되는 쓰레드를 파괴한다.
    void DestoryThread(){
        mIsWorkRun = false;
        CloseHandle(mIOCPHandle);

        for(auto& th : mIOWorkThreads){
            if(th.joinable()){
                th.join();
            }
        }

        //Accepter 쓰레드를 종료한다.
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

    //Waiting Thread Queue에서 대기할 쓰레드들 생성
    bool CreateWorkThread(){
        unsigned int uiThreadId = 0;
        //WaitingThread Queue에서 대기상태로 쓰레드 생성의 권장 개수 : (cpu 개수 * 2) + 1
        for(int i = 0; i < MAX_WORKERTHREAD; i++){
            mIOWorkThreads.emplace_back([this](){ WorkThread(); });
        }

        printf("WorkerThread 시작...\n");
        return true;
   }

   bool CreateAccepterThread(){
        mAccepterThread = std::thread([this]() {AcceptThread();});

        printf("AccepterThread 시작...\n");
        return true;
    }

    //사용하지 않는 클라이언트 구조체를 반환
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

    //CompletionPort객체와 소켓과 CompletionKey를 연결시키는 역할을 하는 함수
    bool BindIOCompletionPort(stClientInfo* pClientInfo)
    {
        auto hIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->m_socketClient, mIOCPHandle, (ULONG_PTR)(pClientInfo), 0);

        if(NULL == hIOCP || mIOCPHandle != hIOCP)
        {
            printf("[에러] CreateIoCompletionPort()함수 실패 : %d\n", GetLastError());
            return false;
        }

        return true;
    }

    bool BindRecv(stClientInfo* pClientInfo){
        DWORD dwFlag = 0;
        DWORD dwRecvNumBytes = 0;

        //Overlapped IO 저옵 셋팅
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
        //socket_error이면 client socket이 끊어진것 으로 처리한다.
        if(nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING)){
            printf("[에러] CreateIoCompletionPort()함수 실패 : %d\n");
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
            printf("[에러] WSASend()함수 실패 : %d\n");
            return false;
        }

        return true;
    }

    //Overlapped IO작업에 대한 완료 통보를 받아 처리하는 함수
    void WorkThread(){
        //Completion Key를 받을 포인터 변수
        stClientInfo* pClientInfo = NULL;
        //함수 호출 성공 여부
        BOOL bSuccess = TRUE;
        //Overlapped IO작업에서 전송된 데이터 크기
        DWORD dwIoSize = 0;
        //IO작업을 위해 요청한 Overlapped 구조체를 받을 포인터
        LPOVERLAPPED lpoverlapped = NULL;

        while(mIsWorkRun){
            //해당함수를 통해 WaitingThread Queue에 대기상태로 들어가게 된다.
            //완료된 Overlapped IO작업이 발생하면 IOCP Queue에서 완료된 작업을 가져와 처리한다.
            //PostQueuedCompletionStatus() 함수에 의해 사용자 메세지가 도착하면 쓰레드를 종료한다.

            bSuccess= GetQueuedCompletionStatus(mIOCPHandle,
                                                &dwIoSize, //실제로 전송된 바이트
                                                (PULONG_PTR)&pClientInfo, //CompletionKey
                                                &lpoverlapped,//Overlapped IO 객체
                                                INFINITE // 대기할시간
                                                );
            //사용자 쓰레드 메세지 종료 처리리
            if(TRUE == bSuccess && 0 == dwIoSize && NULL == lpoverlapped){
                mIsWorkRun = false;
                continue;
            }

            if(NULL == lpoverlapped)
            {
                continue;
            }

            if(FALSE == bSuccess || (0 == dwIoSize && TRUE == bSuccess)){
                printf("socket(%d) 접속 끊김\n" , (int)pClientInfo->m_socketClient);
                CloseSocket(pClientInfo);
                continue;
            }

            stOverlappedEx* pOverlappedEx = (stOverlappedEx*)lpoverlapped;

            //Overlapped IO Recv 작업결과 뒷처리
            if(IOOperation::RECV == pOverlappedEx->m_eOperation){
                pOverlappedEx->m_szBuf[dwIoSize] = NULL;
                printf("[수신] bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);

                SendMsg(pClientInfo, pOverlappedEx->m_szBuf, dwIoSize);
                BindRecv(pClientInfo);
            }
                //Overlapped IO Send 작업결과 뒷처리
            else if (IOOperation::SEND == pOverlappedEx->m_eOperation){
                printf("[송신] bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);
            }
            //예외상황
            else{
                printf("socket(%d)에서 예외상황\n", (int)pClientInfo->m_socketClient);
            }
        }
    }
    //사용자 접속을 받는 스레드
    void AcceptThread(){
        SOCKADDR_IN         stClientAddr;
        int nAddrLen = sizeof(SOCKADDR_IN);

        while(mIsAccepterRun){
            //접속 받은 구조체의 인덱스를 얻어온다.
            stClientInfo* pClientInfo = GetEmptyClientInfo();
            if(NULL == pClientInfo){
                printf("[에러] Client Full\n");
                return ;
            }
            //클라이언트 접속 요청이 들어올때까지 기다린다.
            pClientInfo->m_socketClient = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
            if(INVALID_SOCKET == pClientInfo->m_socketClient)
                continue;

            //IO Completion Port 객체와 소켓을 연결시킨다.
            bool bRet = BindIOCompletionPort(pClientInfo);
            if(false == bRet)
                return;

            bRet = BindRecv(pClientInfo);
            if(false == bRet)
                return;

            char clientIP[32] = {0,};
            inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
            printf("클라이언트 접속 : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_socketClient);

            mClientCnt++;

        }
    }



    void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false)
    {
        struct linger stLinger = {0,0}; // SO_DONTLINGER로 설정

        //bIsForce가 true이면 SO_LINGER, timeout = 0으로 설정하여 강제로 종료
        if( true == bIsForce){
            stLinger.l_onoff = 1;
        }

        //데이터 송수신을 모두 중단시킨다.
        shutdown(pClientInfo->m_socketClient, SD_BOTH);

        setsockopt(pClientInfo->m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof (stLinger));

        closesocket(pClientInfo->m_socketClient);

        pClientInfo->m_socketClient = INVALID_SOCKET;

    }
};

