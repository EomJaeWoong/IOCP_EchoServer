#include "stdafx.h"

//---------------------------------------------------------------------------------
// ������
//---------------------------------------------------------------------------------
CLanServer::CLanServer()
{
	CNPacket::_ValueSizeCheck();
		

	///////////////////////////////////////////////////////////////////////////////
	// LanServer ���� ����
	///////////////////////////////////////////////////////////////////////////////
	_iSessionID = 0;
	_bShutdown = false;

	///////////////////////////////////////////////////////////////////////////////
	// SessionListCS �ʱ�ȭ
	///////////////////////////////////////////////////////////////////////////////
	InitializeCriticalSection(&_SessionListCS);
}

//---------------------------------------------------------------------------------
// �Ҹ���
//---------------------------------------------------------------------------------
CLanServer::~CLanServer()
{

}

//---------------------------------------------------------------------------------
// ���� ����
//---------------------------------------------------------------------------------
bool	CLanServer::Start(WCHAR *wOpenIP, int iPort, int iWorkerThdNum, bool bNagle, int iMaxConnection)
{
	int result;
	DWORD dwThreadID;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ���� �ʱ�ȭ
	//////////////////////////////////////////////////////////////////////////////////////////////////
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// IO Completion Port ����
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_hIOCP == NULL)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// socket ����
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_listen_sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (_listen_sock == INVALID_SOCKET)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// bind
	//////////////////////////////////////////////////////////////////////////////////////////////////
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(iPort);
	InetPton(AF_INET, wOpenIP, &serverAddr.sin_addr);
	result = bind(_listen_sock, (SOCKADDR *)&serverAddr, sizeof(SOCKADDR_IN));
	if (result == SOCKET_ERROR)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//listen
	//////////////////////////////////////////////////////////////////////////////////////////////////
	result = listen(_listen_sock, SOMAXCONN);
	if (result == SOCKET_ERROR)
		return FALSE;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// nagle �ɼ�
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_bNagle = bNagle;
	if (_bNagle == true)
	{
		int opt_val = TRUE;
		setsockopt(_listen_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt_val, sizeof(opt_val));
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Thread ����
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_hAcceptThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		AcceptThread,
		this,
		0,
		(unsigned int *)&dwThreadID
		);

	_iWorkerThdNum = iWorkerThdNum;

	for (int iCnt = 0; iCnt < iWorkerThdNum; iCnt++)
	{
		_hWorkerThread[iCnt] = (HANDLE)_beginthreadex(
			NULL,
			0,
			WorkerThread,
			this,
			0,
			(unsigned int *)&dwThreadID
			);
	}

	_hMonitorThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		MonitorThread,
		this,
		0,
		(unsigned int *)&dwThreadID
		);

	return TRUE;
}

//---------------------------------------------------------------------------------
// ���� ����
//---------------------------------------------------------------------------------
void	CLanServer::Stop()
{

}

//---------------------------------------------------------------------------------
// ��Ŷ ������
//---------------------------------------------------------------------------------
bool	CLanServer::SendPacket(__int64 iSessionID, CNPacket *pPacket)
{
	for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
	{
		if (Session[iCnt]->_iSessionID == iSessionID)
		{
			pPacket->addRef();

			Session[iCnt]->SendQ.Put((char *)&pPacket, sizeof(pPacket));
			InterlockedIncrement64((LONG64 *)&_SendPacketCounter);
			SendPost(Session[iCnt]);
			break;
		}
	}
	return true;
}

//---------------------------------------------------------------------------------
// ���� ���ư��� �κ�(������)
//---------------------------------------------------------------------------------
// Worker Thread
int		CLanServer::WorkerThread_Update()
{
	int result;

	DWORD dwTransferred;
	OVERLAPPED *pOverlapped;
	SESSION *pSession;

	while (!_bShutdown)
	{
		///////////////////////////////////////////////////////////////////////////
		// Transferred, Overlapped, key �ʱ�ȭ
		///////////////////////////////////////////////////////////////////////////
		dwTransferred = 0;
		pOverlapped = NULL;
		pSession = NULL;

		result = GetQueuedCompletionStatus(_hIOCP, &dwTransferred, (PULONG_PTR)&pSession,
			(LPOVERLAPPED *)&pOverlapped, INFINITE);

		//----------------------------------------------------------------------------
		// Error, ���� ó��
		//----------------------------------------------------------------------------
		// IOCP ���� ���� ����
		if (result == FALSE && (pOverlapped == NULL || pSession == NULL))
		{
			int iErrorCode = WSAGetLastError();
			OnError(iErrorCode, L"IOCP HANDLE Error\n");

			break;
		}

		// ��Ŀ������ ���� ����
		else if (dwTransferred == 0 && pSession == NULL && pOverlapped == NULL)
		{
			OnError(0, L"Worker Thread Done.\n");
			PostQueuedCompletionStatus(_hIOCP, 0, NULL, NULL);
			return 0;
		}

		//----------------------------------------------------------------------------
		// ��������
		// Ŭ���̾�Ʈ ���� closesocket() Ȥ�� shutdown() �Լ��� ȣ���� ����
		//----------------------------------------------------------------------------
		else if (dwTransferred == 0)
		{
			if (pOverlapped == &(pSession->_RecvOverlapped))
			{
				result = GetLastError();
			}

			else if (pOverlapped == &(pSession->_SendOverlapped))
				pSession->_bSendFlag = false;

			if (0 == InterlockedDecrement64((LONG64 *)&pSession->_lIOCount))
				ReleaseSession(pSession);

			continue;
		}
		//----------------------------------------------------------------------------

		OnWorkerThreadBegin();

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

		OnWorkerThreadEnd();
	}

	return 0;
}

// Accept Thread
int		CLanServer::AcceptThread_Update()
{
	HANDLE result;

	SOCKET ClientSocket;
	int addrlen = sizeof(SOCKADDR_IN);
	SOCKADDR_IN clientSock;
	WCHAR clientIP[16];

	while (!_bShutdown)
	{
		//////////////////////////////////////////////////////////////////////////////
		// Accept
		//////////////////////////////////////////////////////////////////////////////
		ClientSocket = WSAAccept(_listen_sock, (SOCKADDR *)&clientSock, &addrlen, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET)
		{
			DisconnectSession(ClientSocket);
			continue;
		}
		InetNtop(AF_INET, &clientSock.sin_addr, clientIP, 16);

		//////////////////////////////////////////////////////////////////////////////
		// Session���� �� á�� ��
		//////////////////////////////////////////////////////////////////////////////
		if (_iSessionCount >= MAX_SESSION)
		{
		}

		//////////////////////////////////////////////////////////////////////////////
		// Request ��û
		//////////////////////////////////////////////////////////////////////////////
		if (!OnConnectionRequest(clientIP, ntohs(clientSock.sin_port)))
		{
			SocketClose(ClientSocket);
			continue;
		}
		InterlockedIncrement64((LONG64 *)&_AcceptCounter);
		InterlockedIncrement64((LONG64 *)&_AcceptTotalCounter);

		//////////////////////////////////////////////////////////////////////////////
		// ���� �߰� ����
		//////////////////////////////////////////////////////////////////////////////
		SESSION* pSession = CreateSession(ClientSocket, clientIP, clientSock.sin_port);

		/////////////////////////////////////////////////////////////////////
		// recv ���
		/////////////////////////////////////////////////////////////////////
		RecvPost(pSession, true);

		InterlockedIncrement64((LONG64 *)&_iSessionCount);
	}

	return 0;
}

// MonitorThread
int		CLanServer::MonitorThread_Update()
{
	DWORD iGetTime = timeGetTime();

	while (1)
	{
		if (timeGetTime() - iGetTime < 1000)
			continue;

		_AcceptTPS = _AcceptCounter;
		_AcceptTotalTPS += _AcceptTotalCounter;
		_RecvPacketTPS = _RecvPacketCounter;
		_SendPacketTPS = _SendPacketCounter;
		_PacketPoolTPS = 0;

		_AcceptCounter = 0;
		_AcceptTotalCounter = 0;
		_RecvPacketCounter = 0;
		_SendPacketCounter = 0;

		iGetTime = timeGetTime();
	}

	return 0;
}

//---------------------------------------------------------------------------------
// ���� �������
//---------------------------------------------------------------------------------
unsigned __stdcall	CLanServer::WorkerThread(LPVOID workerArg)
{
	return ((CLanServer *)workerArg)->WorkerThread_Update();
}

unsigned __stdcall	CLanServer::AcceptThread(LPVOID acceptArg)
{
	return ((CLanServer *)acceptArg)->AcceptThread_Update();
}

unsigned __stdcall	CLanServer::MonitorThread(LPVOID monitorArg)
{
	return ((CLanServer *)monitorArg)->MonitorThread_Update();
}

//--------------------------------------------------------------------------------
// Recv ���
//--------------------------------------------------------------------------------
bool	CLanServer::RecvPost(SESSION *pSession, bool bAcceptRecv)
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
	result = WSARecv(pSession->_SessionInfo._socket, wBuf, iCount, &dwRecvSize, &dwflag, &pSession->_RecvOverlapped, NULL);

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

//--------------------------------------------------------------------------------
// Send ���
//--------------------------------------------------------------------------------
bool	CLanServer::SendPost(SESSION *pSession)
{
	int retval, iCount = 0;
	DWORD dwSendSize, dwflag = 0;
	WSABUF wBuf[2];
	CNPacket *pPacket = NULL;

	do
	{
		////////////////////////////////////////////////////////////////////////////////////
		// ���� ������ ������ �۾� ������ �˻�
		// Flag value�� true  => ������ ��
		//              false => �Ⱥ����� �� -> ������ ������ �ٲ۴�
		////////////////////////////////////////////////////////////////////////////////////
		if (true == InterlockedCompareExchange((LONG *)&pSession->_bSendFlag, true, false))
			return false;


		if (pSession->_SendQ.GetUseSize() == 0)
		{
			////////////////////////////////////////////////////////////////////////////////
			// SendFlag -> false
			////////////////////////////////////////////////////////////////////////////////
			InterlockedExchange((LONG *)&pSession->_bSendFlag, false);
			continue;
		}

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
		retval = WSASend(pSession->_SessionInfo._socket, wBuf, iCount, &dwSendSize, dwflag, &pSession->_SendOverlapped, NULL);

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

	return true;
}

//--------------------------------------------------------------------------------
// Recv, Send �Ϸ�
//--------------------------------------------------------------------------------
void	CLanServer::CompleteRecv(SESSION *pSession, DWORD dwTransferred)
{
	short header;

	//////////////////////////////////////////////////////////////////////////////
	// _RecvQ WritePos �̵�(���� ��ŭ)
	//////////////////////////////////////////////////////////////////////////////
	pSession->_RecvQ.MoveWritePos(dwTransferred);

	CNPacket *pPacket = CNPacket::Alloc();

	while (pSession->_RecvQ.GetUseSize() > 0)
	{
		//////////////////////////////////////////////////////////////////////////
		// _RecvQ�� ��� ���̸�ŭ �ִ��� �˻� �� ������ Peek
		//////////////////////////////////////////////////////////////////////////
		if (pSession->_RecvQ.GetUseSize() <= sizeof(header))
			break;
		pSession->_RecvQ.Peek((char *)&header, sizeof(header));

		//////////////////////////////////////////////////////////////////////////
		// _RecvQ�� ��� ���� + Payload ��ŭ �ִ��� �˻� �� ��� ����
		//////////////////////////////////////////////////////////////////////////
		if (pSession->_RecvQ.GetUseSize() < sizeof(header) + header)
			break;;
		pSession->_RecvQ.RemoveData(sizeof(header));

		//////////////////////////////////////////////////////////////////////////
		// Payload�� ���� �� ��Ŷ Ŭ������ ����
		//////////////////////////////////////////////////////////////////////////
		pPacket->Put(pSession->_RecvQ.GetReadBufferPtr(), header);
		pSession->_RecvQ.RemoveData(header);

		//////////////////////////////////////////////////////////////////////////
		// OnRecv ȣ��
		//////////////////////////////////////////////////////////////////////////
		OnRecv(pSession->_iSessionID, pPacket);

		InterlockedIncrement64((LONG64 *)&_RecvPacketCounter);
	}

	pPacket->Free();
	RecvPost(pSession);
}

void	CLanServer::CompleteSend(SESSION *pSession, DWORD dwTransferred)
{
	//////////////////////////////////////////////////////////////////////////////
	// ���´� ������ ����
	//////////////////////////////////////////////////////////////////////////////
	pSession->_SendQ.RemoveData(dwTransferred);

	//////////////////////////////////////////////////////////////////////////////
	// SendFlag => false
	//////////////////////////////////////////////////////////////////////////////
	InterlockedCompareExchange((LONG *)&pSession->_bSendFlag, false, true))

	//////////////////////////////////////////////////////////////////////////////
	// �������� ������ �ٽ� Send�ϵ��� ��� ��
	//////////////////////////////////////////////////////////////////////////////
	SendPost(pSession);
}

SESSION* CLanServer::CreateSession(SOCKET socket, WCHAR* wIP, int iPort)
{
	SESSION* pSession = new SESSION;
	
	///////////////////////////////////////////////////////////////////////////
	// ���� ���� ����ü �ʱ�ȭ
	///////////////////////////////////////////////////////////////////////////
	pSession->_SessionInfo._socket = socket;
	wcscpy_s(pSession->_SessionInfo._IP, 16, wIP);
	pSession->_SessionInfo._iPort = ntohs(iPort);

	///////////////////////////////////////////////////////////////////////////
	// ���� �ʱ�ȭ
	///////////////////////////////////////////////////////////////////////////
	pSession->_iSessionID = InterlockedIncrement64((LONG64 *)&_iSessionID);

	memset(&pSession->_SendOverlapped, 0, sizeof(OVERLAPPED));
	memset(&pSession->_RecvOverlapped, 0, sizeof(OVERLAPPED));

	pSession->_SendQ.ClearBuffer();
	pSession->_RecvQ.ClearBuffer();

	pSession->_bSendFlag = false;
	pSession->_lIOCount = 0;

	InitializeCriticalSection(&pSession->_SessionCS);
}

//--------------------------------------------------------------------------------
// Disconnect, Release
//--------------------------------------------------------------------------------
void CLanServer::SocketClose(SOCKET socket)
{
	SocketClose(socket);
}

void CLanServer::DisconnectSession(SESSION *pSession)
{

}

void CLanServer::DisconnectSession(__int64 iSessionID)
{

}

void CLanServer::ReleaseSession(SESSION *pSession)
{

}

void CLanServer::ReleaseSession(__int64 iSessionID)
{

}