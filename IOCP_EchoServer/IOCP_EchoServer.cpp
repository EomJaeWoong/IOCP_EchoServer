// IOCP_EchoServer.cpp : �ܼ� ���� ���α׷��� ���� �������� �����մϴ�.
//

#include "stdafx.h"

SOCKET		g_ListenSocket;
HANDLE		g_hIOCP;

HANDLE		hAcceptThread, hWorkerThread[MAX_THREAD];

SESSION		g_Session[MAX_SESSION];
__int64		g_iSessionID;

bool		g_bShutdown;


bool			initIOCPServer()
{
	int retval;
	DWORD dwThreadID;

	g_iSessionID = 0;
	g_bShutdown = false;

	//////////////////////////////////////////////////////////////////
	// ���� �ʱ�ȭ
	//////////////////////////////////////////////////////////////////
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	//////////////////////////////////////////////////////////////////
	// Listen Socket ����
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
	// IO Completion Port ����
	//////////////////////////////////////////////////////////////////
	g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (g_hIOCP == NULL)
		return false;

	//////////////////////////////////////////////////////////////////
	// ���� �迭 �ʱ�ȭ
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
	// Thread ����
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

unsigned __stdcall	AcceptThread(LPVOID acceptArg)
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

		RecvPost(pSession);
	}

	return 0;
}

unsigned __stdcall	WorkerThread(LPVOID workerArg)
{
	int result;

	DWORD dwTransferred;
	OVERLAPPED *pOverlapped;
	SESSION *pSession;

	while (!g_bShutdown)
	{
		///////////////////////////////////////////////////////////////////////////
		// Transferred, Overlapped, key �ʱ�ȭ
		///////////////////////////////////////////////////////////////////////////
		dwTransferred = 0;
		pOverlapped = NULL;
		pSession = NULL;

		result = GetQueuedCompletionStatus(g_hIOCP, &dwTransferred, (PULONG_PTR)&pSession,
			(LPOVERLAPPED *)&pOverlapped, INFINITE);

		//----------------------------------------------------------------------------
		// Error, ���� ó��
		//----------------------------------------------------------------------------
		// IOCP ���� ���� ����
		if (result == FALSE && (pOverlapped == NULL || pSession == NULL))
		{
			int iErrorCode = WSAGetLastError();
			wprintf(L"%d, IOCP ERROR\n", iErrorCode);

			break;
		}

		// ��Ŀ������ ���� ����
		else if (dwTransferred == 0 && pSession == NULL && pOverlapped == NULL)
		{
			PostQueuedCompletionStatus(g_hIOCP, 0, NULL, NULL);
			return 0;
		}

		//----------------------------------------------------------------------------
		// ��������
		// Ŭ���̾�Ʈ ���� closesocket() Ȥ�� shutdown() �Լ��� ȣ���� ����
		//----------------------------------------------------------------------------
		else if (dwTransferred == 0)
		{
			if (0 == InterlockedDecrement64((LONG64 *)&pSession->_lIOCount))
				ReleaseSession(pSession);

			continue;
		}
		//----------------------------------------------------------------------------

		//////////////////////////////////////////////////////////////////////////////
		// Recv �Ϸ�
		//////////////////////////////////////////////////////////////////////////////
		if (pOverlapped == &pSession->_RecvOverlapped)
		{
			CompleteRecv(pSession, dwTransferred);
		}

		//////////////////////////////////////////////////////////////////////////////
		// Send �Ϸ�
		//////////////////////////////////////////////////////////////////////////////
		else if (pOverlapped == &pSession->_SendOverlapped)
		{
			CompleteSend(pSession, dwTransferred);
		}

		if (0 == InterlockedDecrement64((LONG64 *)&pSession->_lIOCount))
			ReleaseSession(pSession);
	}

	return 0;
}

void				CompleteRecv(SESSION *pSession, DWORD dwTransferred)
{
	__int64 iValue = 0;

	pSession->_RecvQ.Get((char *)iValue, sizeof(__int64));

	SendPacket(pSession->_iSessionID, iValue);
	RecvPost(pSession);
}

void				CompleteSend(SESSION *pSession, DWORD dwTransferred)
{

}

bool				SendPacket(__int64 iSessionID, __int64 iPacket)
{
	for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
	{
		if (g_Session[iCnt]._iSessionID == iSessionID)
		{
			g_Session[iCnt]._SendQ.Lock();
			g_Session[iCnt]._SendQ.Put((char *)iPacket, sizeof(__int64));
			g_Session[iCnt]._SendQ.Unlock();

			SendPost(&g_Session[iCnt]);
			break;
		}
	}
	return true;
}

SESSION				*CreateSession(SOCKET socket)
{
	SESSION *pSession = NULL;

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

bool				RecvPost(SESSION *pSession)
{
	int result, iCount = 1;
	DWORD dwRecvSize, dwflag = 0;
	WSABUF wBuf[2];

	//////////////////////////////////////////////////////////////////////////////
	// WSABUF ���
	//////////////////////////////////////////////////////////////////////////////
	wBuf[0].buf = pSession->_RecvQ.GetWriteBufferPtr();
	wBuf[0].len = pSession->_RecvQ.GetNotBrokenPutSize();

	//////////////////////////////////////////////////////////////////////////////
	// ������ ��谡 ���ԵǸ� ������ �κе� WSABUF�� ����� ��
	//////////////////////////////////////////////////////////////////////////////
	if (pSession->_RecvQ.GetFreeSize() > pSession->_RecvQ.GetNotBrokenPutSize())
	{
		wBuf[1].buf = pSession->_RecvQ.GetBufferPtr();
		wBuf[1].len = pSession->_RecvQ.GetFreeSize() - pSession->_RecvQ.GetNotBrokenPutSize();
		iCount++;
	}

	InterlockedIncrement64((LONG64 *)&pSession->_lIOCount);
	result = WSARecv(pSession->_socket, wBuf, iCount, &dwRecvSize, &dwflag, &pSession->_RecvOverlapped, NULL);

	//////////////////////////////////////////////////////////////////////////////
	// WSARecv Error
	//////////////////////////////////////////////////////////////////////////////
	if (result == SOCKET_ERROR)
	{
		int iErrorCode = GetLastError();
		if (iErrorCode != WSA_IO_PENDING)
		{
			//////////////////////////////////////////////////////////////////////
			// IO_PENDING�� �ƴϸ� ��¥ ����
			//////////////////////////////////////////////////////////////////////
			if (0 == InterlockedDecrement64((LONG64 *)&(pSession->_lIOCount)))
				ReleaseSession(pSession);

			return false;
		}
	}

	return true;
}

bool				SendPost(SESSION *pSession)
{
	int retval, iCount = 1;
	DWORD dwSendSize, dwflag = 0;
	WSABUF wBuf[2];

	////////////////////////////////////////////////////////////////////////////////////
	// ���� ������ ������ �۾� ������ �˻�
	// Flag value�� true  => ������ ��
	//              false => �Ⱥ����� �� -> ������ ������ �ٲ۴�
	////////////////////////////////////////////////////////////////////////////////////
	if (true == InterlockedCompareExchange((LONG *)&pSession->_bSendFlag, true, false))
		return false;

	do
	{

		//////////////////////////////////////////////////////////////////////////////
		// WSABUF ���
		//////////////////////////////////////////////////////////////////////////////
		wBuf[0].buf = pSession->_SendQ.GetReadBufferPtr();
		wBuf[0].len = pSession->_SendQ.GetNotBrokenGetSize();

		if (pSession->_SendQ.GetUseSize() > pSession->_SendQ.GetNotBrokenGetSize())
		{
			wBuf[1].buf = pSession->_SendQ.GetBufferPtr();
			wBuf[1].len = pSession->_SendQ.GetUseSize() - pSession->_SendQ.GetNotBrokenGetSize();

			iCount++;
		}


		InterlockedIncrement64((LONG64 *)&pSession->_lIOCount);
		retval = WSASend(pSession->_socket, wBuf, iCount, &dwSendSize, dwflag, &pSession->_SendOverlapped, NULL);

		//////////////////////////////////////////////////////////////////////////////
		// WSASend Error
		//////////////////////////////////////////////////////////////////////////////
		if (retval == SOCKET_ERROR)
		{
			int iErrorCode = GetLastError();
			if (iErrorCode != WSA_IO_PENDING)
			{
				if (0 == InterlockedDecrement64((LONG64 *)&pSession->_lIOCount))
					ReleaseSession(pSession);

				return FALSE;
			}
		}
	} while (0);

	return TRUE;
}

void				DisconnectSession(SESSION *pSession)
{
	shutdown(pSession->_socket, SD_SEND);
}

void				ReleaseSession(SESSION *pSession)
{
	
}

void				SocketClose(SOCKET socket)
{

}

int					_tmain(int argc, _TCHAR* argv[])
{
	int retval;

	if (!initIOCPServer())
		return 0;

	while (!g_bShutdown)
	{

	}

	return 0;
}

