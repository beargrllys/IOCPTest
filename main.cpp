#include "IOCPServer.h"
#include <string>
#include <iostream>

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 100;

/*int main()
{
    IOCPServer ioCompletionPort;

    //���� �ʱ�ȭ
    ioCompletionPort.InitSocket();

    //���ϰ� �����ּҸ� �����ϰ� ��Ͻ�Ų��.

    ioCompletionPort.BindandListen(SERVER_PORT);
    ioCompletionPort.StartServer(MAX_CLIENT);

    printf("�ƹ� Ű�� ���� ������ ����մϴ�.");
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