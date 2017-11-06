#include "stdafx.h"

//---------------------------------------------------------------------------------
// 생성자
//---------------------------------------------------------------------------------
CLanServer::CLanServer()
{
	CNPacket::_ValueSizeCheck();
		

	///////////////////////////////////////////////////////////////////////////////
	// LanServer 변수 설정
	///////////////////////////////////////////////////////////////////////////////
	_iSessionID = 0;
	_bShutdown = false;

	///////////////////////////////////////////////////////////////////////////////
	// SessionListCS 초기화
	///////////////////////////////////////////////////////////////////////////////
	InitializeCriticalSection(&_SessionListCS);
}

//---------------------------------------------------------------------------------
// 소멸자
//---------------------------------------------------------------------------------
CLanServer::~CLanServer()
{

}

//---------------------------------------------------------------------------------
// 서버 시작
//---------------------------------------------------------------------------------
bool	CLanServer::Start(WCHAR *wOpenIP, int iPort, int iWorkerThdNum, bool bNagle, int iMaxConnection)
{
	int result;
	DWORD dwThreadID;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 윈속 초기화
	//////////////////////////////////////////////////////////////////////////////////////////////////
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// IO Completion Port 생성
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_hIOCP == NULL)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// socket 생성
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
	// nagle 옵션
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_bNagle = bNagle;
	if (_bNagle == true)
	{
		int opt_val = TRUE;
		setsockopt(_listen_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt_val, sizeof(opt_val));
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Thread 생성
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
// 서버 멈춤
//---------------------------------------------------------------------------------
void	CLanServer::Stop()
{

}

//---------------------------------------------------------------------------------
// 패킷 보내기
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
// 실제 돌아가는 부분(쓰레드)
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
		// Transferred, Overlapped, key 초기화
		///////////////////////////////////////////////////////////////////////////
		dwTransferred = 0;
		pOverlapped = NULL;
		pSession = NULL;

		result = GetQueuedCompletionStatus(_hIOCP, &dwTransferred, (PULONG_PTR)&pSession,
			(LPOVERLAPPED *)&pOverlapped, INFINITE);

		//----------------------------------------------------------------------------
		// Error, 종료 처리
		//----------------------------------------------------------------------------
		// IOCP 에러 서버 종료
		if (result == FALSE && (pOverlapped == NULL || pSession == NULL))
		{
			int iErrorCode = WSAGetLastError();
			OnError(iErrorCode, L"IOCP HANDLE Error\n");

			break;
		}

		// 워커스레드 정상 종료
		else if (dwTransferred == 0 && pSession == NULL && pOverlapped == NULL)
		{
			OnError(0, L"Worker Thread Done.\n");
			PostQueuedCompletionStatus(_hIOCP, 0, NULL, NULL);
			return 0;
		}

		//----------------------------------------------------------------------------
		// 정상종료
		// 클라이언트 에서 closesocket() 혹은 shutdown() 함수를 호출한 시점
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
		// Recv 완료
		//////////////////////////////////////////////////////////////////////////////
		if (pOverlapped == &pSession->_RecvOverlapped)
		{
			CompleteRecv(pSession, dwTransferred);
		}

		//////////////////////////////////////////////////////////////////////////////
		// Send 완료
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
		// Session수가 꽉 찼을 때
		//////////////////////////////////////////////////////////////////////////////
		if (_iSessionCount >= MAX_SESSION)
		{
		}

		//////////////////////////////////////////////////////////////////////////////
		// Request 요청
		//////////////////////////////////////////////////////////////////////////////
		if (!OnConnectionRequest(clientIP, ntohs(clientSock.sin_port)))
		{
			SocketClose(ClientSocket);
			continue;
		}
		InterlockedIncrement64((LONG64 *)&_AcceptCounter);
		InterlockedIncrement64((LONG64 *)&_AcceptTotalCounter);

		//////////////////////////////////////////////////////////////////////////////
		// 세션 추가 과정
		//////////////////////////////////////////////////////////////////////////////
		SESSION* pSession = CreateSession(ClientSocket, clientIP, clientSock.sin_port);

		/////////////////////////////////////////////////////////////////////
		// recv 등록
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
// 실제 쓰레드들
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
// Recv 등록
//--------------------------------------------------------------------------------
bool	CLanServer::RecvPost(SESSION *pSession, bool bAcceptRecv)
{
	int result, iCount = 1;
	DWORD dwRecvSize, dwflag = 0;
	WSABUF wBuf[2];

	//////////////////////////////////////////////////////////////////////////////
	// WSABUF 등록
	//////////////////////////////////////////////////////////////////////////////
	wBuf[0].buf = pSession->_RecvQ.GetWriteBufferPtr();
	wBuf[0].len = pSession->_RecvQ.GetNotBrokenPutSize();

	//////////////////////////////////////////////////////////////////////////////
	// 버퍼의 경계가 포함되면 나머지 부분도 WSABUF에 등록해 줌
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
			// IO_PENDING이 아니면 진짜 에러
			//////////////////////////////////////////////////////////////////////
			if (0 == InterlockedDecrement64((LONG64 *)&(pSession->_lIOCount)))
				ReleaseSession(pSession);

			return false;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------
// Send 등록
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
		// 현재 세션이 보내기 작업 중인지 검사
		// Flag value가 true  => 보내는 중
		//              false => 안보내는 중 -> 보내는 중으로 바꾼다
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
		// WSABUF 등록
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
// Recv, Send 완료
//--------------------------------------------------------------------------------
void	CLanServer::CompleteRecv(SESSION *pSession, DWORD dwTransferred)
{
	short header;

	//////////////////////////////////////////////////////////////////////////////
	// _RecvQ WritePos 이동(받은 만큼)
	//////////////////////////////////////////////////////////////////////////////
	pSession->_RecvQ.MoveWritePos(dwTransferred);

	CNPacket *pPacket = CNPacket::Alloc();

	while (pSession->_RecvQ.GetUseSize() > 0)
	{
		//////////////////////////////////////////////////////////////////////////
		// _RecvQ에 헤더 길이만큼 있는지 검사 후 있으면 Peek
		//////////////////////////////////////////////////////////////////////////
		if (pSession->_RecvQ.GetUseSize() <= sizeof(header))
			break;
		pSession->_RecvQ.Peek((char *)&header, sizeof(header));

		//////////////////////////////////////////////////////////////////////////
		// _RecvQ에 헤더 길이 + Payload 만큼 있는지 검사 후 헤더 제거
		//////////////////////////////////////////////////////////////////////////
		if (pSession->_RecvQ.GetUseSize() < sizeof(header) + header)
			break;;
		pSession->_RecvQ.RemoveData(sizeof(header));

		//////////////////////////////////////////////////////////////////////////
		// Payload를 뽑은 후 패킷 클래스에 넣음
		//////////////////////////////////////////////////////////////////////////
		pPacket->Put(pSession->_RecvQ.GetReadBufferPtr(), header);
		pSession->_RecvQ.RemoveData(header);

		//////////////////////////////////////////////////////////////////////////
		// OnRecv 호출
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
	// 보냈던 데이터 제거
	//////////////////////////////////////////////////////////////////////////////
	pSession->_SendQ.RemoveData(dwTransferred);

	//////////////////////////////////////////////////////////////////////////////
	// SendFlag => false
	//////////////////////////////////////////////////////////////////////////////
	InterlockedCompareExchange((LONG *)&pSession->_bSendFlag, false, true))

	//////////////////////////////////////////////////////////////////////////////
	// 못보낸게 있으면 다시 Send하도록 등록 함
	//////////////////////////////////////////////////////////////////////////////
	SendPost(pSession);
}

SESSION* CLanServer::CreateSession(SOCKET socket, WCHAR* wIP, int iPort)
{
	SESSION* pSession = new SESSION;
	
	///////////////////////////////////////////////////////////////////////////
	// 세션 정보 구조체 초기화
	///////////////////////////////////////////////////////////////////////////
	pSession->_SessionInfo._socket = socket;
	wcscpy_s(pSession->_SessionInfo._IP, 16, wIP);
	pSession->_SessionInfo._iPort = ntohs(iPort);

	///////////////////////////////////////////////////////////////////////////
	// 세션 초기화
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