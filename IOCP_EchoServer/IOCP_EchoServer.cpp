// IOCP_EchoServer.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"

SOCKET		g_ListenSocket;
HANDLE		g_hIOCP;

HANDLE		hAcceptThread, hWorkerThread[MAX_THREAD];

SESSION		g_Session[MAX_SESSION];

bool		g_bShutdown;


bool initIOCPServer()
{
	int retval;
	DWORD dwThreadID;

	//////////////////////////////////////////////////////////////////
	// 윈속 초기화
	//////////////////////////////////////////////////////////////////
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	//////////////////////////////////////////////////////////////////
	// Listen Socket 생성
	//////////////////////////////////////////////////////////////////
	g_ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (g_ListenSocket == INVALID_SOCKET)
		return false;

	//////////////////////////////////////////////////////////////////
	// bind
	//////////////////////////////////////////////////////////////////
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(dfSERVER_PORT);
	InetPton(AF_INET, dfSERVER_IP, &serverAddr.sin_addr);
	retval = bind(g_ListenSocket, (SOCKADDR *)&serverAddr, sizeof(SOCKADDR_IN));
	if (retval = SOCKET_ERROR)
		return false;

	//////////////////////////////////////////////////////////////////
	//listen
	//////////////////////////////////////////////////////////////////
	retval = listen(g_ListenSocket, SOMAXCONN);
	if (retval == SOCKET_ERROR)
		return FALSE;

	//////////////////////////////////////////////////////////////////
	// IO Completion Port 생성
	//////////////////////////////////////////////////////////////////
	g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (g_hIOCP == NULL)
		return false;

	//////////////////////////////////////////////////////////////////
	// 세션 배열 초기화
	//////////////////////////////////////////////////////////////////
	for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
	{
		g_Session[iCnt]._socket = INVALID_SOCKET;

		memset(&g_Session[iCnt]._RecvOverlapped, 0, sizeof(OVERLAPPED));
		memset(&g_Session[iCnt]._SendOverlapped, 0, sizeof(OVERLAPPED));
		
		g_Session[iCnt]._RecvQ.ClearBuffer();
		g_Session[iCnt]._SendQ.ClearBuffer();

		g_Session[iCnt]._bSendFlag = false;
		g_Session[iCnt]._lIOCount = 0;
	}

	//////////////////////////////////////////////////////////////////
	// Thread 생성
	//////////////////////////////////////////////////////////////////
	hAcceptThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		AcceptThread,
		NULL,
		0,
		(unsigned int *)&dwThreadID
		);

	for (int iCnt = 0; iCnt < MAX_THREAD; iCnt++)
	{
		hWorkerThread[iCnt] = (HANDLE)_beginthreadex(
			NULL,
			0,
			WorkerThread,
			NULL,
			0,
			(unsigned int *)&dwThreadID
			);
	}

	return true;
}

unsigned __stdcall AcceptThread(LPVOID acceptArg)
{
	SESSION *pSession;

	SOCKET ClientSocket;
	SOCKADDR_IN ClientAddr;
	int addrlen = sizeof(SOCKADDR_IN);

	while (!g_bShutdown)
	{
		ClientSocket = accept(g_ListenSocket, (SOCKADDR *)&ClientAddr, &addrlen);
		if (ClientSocket == INVALID_SOCKET)
			break;

		pSession = CreateSession(ClientSocket);
	}
}

unsigned __stdcall WorkerThread(LPVOID workerArg)
{

}

SESSION *CreateSession(SOCKET socket)
{
	SESSION *pSession;

	for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
	{
		if (INVALID_SOCKET == g_Session[iCnt]._socket)
		{
			pSession = &g_Session[iCnt];

			g_Session[iCnt]._socket = socket;

			memset(&g_Session[iCnt]._RecvOverlapped, 0, sizeof(OVERLAPPED));
			memset(&g_Session[iCnt]._SendOverlapped, 0, sizeof(OVERLAPPED));

			g_Session[iCnt]._RecvQ.ClearBuffer();
			g_Session[iCnt]._SendQ.ClearBuffer();

			g_Session[iCnt]._bSendFlag = false;
			g_Session[iCnt]._lIOCount = 0;

			break;
		}
	}

	return pSession;
}

int _tmain(int argc, _TCHAR* argv[])
{
	int retval;

	if (!initIOCPServer())
		return 0;

	return 0;
}

