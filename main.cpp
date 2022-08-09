#include "IOCPServer.h"
#include <string>
#include <iostream>

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 100;

/*int main()
{
    IOCPServer ioCompletionPort;

    //소켓 초기화
    ioCompletionPort.InitSocket();

    //소켓과 서버주소를 연결하고 등록시킨다.

    ioCompletionPort.BindandListen(SERVER_PORT);
    ioCompletionPort.StartServer(MAX_CLIENT);

    printf("아무 키나 누를 때까지 대기합니다.");
    while(true)
    {
        std::string inputCmd;
        std::getline(std::cin, inputCmd);

        if(inputCmd == "quit")
        {
            break;
        }
    }

    ioCompletionPort.DestoryThread();
    return 0;
}*/