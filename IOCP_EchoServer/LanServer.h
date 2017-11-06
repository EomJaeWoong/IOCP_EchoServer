#ifndef __LANSERVER__H__
#define __LANSERVER__H__

enum
{
	// 최대 쓰레드 수
	MAX_THREAD = 50,

	// 최대 접속자 수
	MAX_SESSION = 200,

	
};

//-------------------------------------------------------------------------------------
// 세션 정보 구조체
//-------------------------------------------------------------------------------------
typedef struct stSESSION_INFO
{
	SOCKET _socket;
	WCHAR _IP[16];
	int _iPort;

} SESSION_INFO;

//-------------------------------------------------------------------------------------
// 세션
//-------------------------------------------------------------------------------------
typedef struct stSESSION
{
	SESSION_INFO _SessionInfo;
	__int64 _iSessionID;

	OVERLAPPED _SendOverlapped;
	OVERLAPPED _RecvOverlapped;

	CAyaStreamSQ _SendQ;
	CAyaStreamSQ _RecvQ;

	BOOL _bSendFlag;
	LONG _lIOCount;

	CRITICAL_SECTION _SessionCS;

	LONG _Debug;
} SESSION;



class CLanServer
{
public:
	//---------------------------------------------------------------------------------
	// 생성자, 소멸자
	//---------------------------------------------------------------------------------
	CLanServer();
	virtual ~CLanServer();

	//---------------------------------------------------------------------------------
	// 서버 시작, 멈춤
	//---------------------------------------------------------------------------------
	bool			Start(WCHAR *wOpenIP, int iPort, int iWorkerThdNum, bool bNagle, int iMaxConnection);
	void			Stop();

	//---------------------------------------------------------------------------------
	// 현재 Session 수
	//---------------------------------------------------------------------------------
	int				GetClientCount(){ return _iSessionCount; }

protected:
	//---------------------------------------------------------------------------------
	// 패킷 보내기
	//---------------------------------------------------------------------------------
	bool			SendPacket(__int64 iSessionID, CNPacket *pPacket);

	//---------------------------------------------------------------------------------
	// OnClientJoin			-> 접속처리 완료 후 호출
	// OnConnectionLeave		-> Disconnect후 호출
	// OnConnectionRequest		-> Accept 직후 호출
	//		return false; 시 클라이언트 거부
	//		return true; 시 접속 허용
	// OnRecv				-> 패킷 수신 완료 후
	// OnSend				-> 패킷 송신 완료 후
	// OnWorkerThreadBegin		-> 워커스레드 GQCS 바로 하단에서 호출
	// OnWorkerThreadEnd		-> 워커스레드 1루프 끝
	// OnError				-> 에러 메시지
	//---------------------------------------------------------------------------------
	virtual void	OnClientJoin(SESSION_INFO* pSessionInfo, __int64 iSessionID) = 0;
	virtual void	OnClientLeave(__int64 iSessionID) = 0;
	virtual bool	OnConnectionRequest(WCHAR *ClientIP, int Port) = 0;

	virtual void	OnRecv(__int64 iSessionID, CNPacket *pPacket) = 0;
	virtual void	OnSend(__int64 iSessionID, int sendsize) = 0;

	virtual void	OnWorkerThreadBegin() = 0;
	virtual void	OnWorkerThreadEnd() = 0;

	virtual void	OnError(int errorcode, WCHAR* errorMsg) = 0;

private:
	//---------------------------------------------------------------------------------
	// 쓰레드 관련
	//---------------------------------------------------------------------------------
	static unsigned __stdcall WorkerThread(LPVOID workerArg);
	static unsigned __stdcall AcceptThread(LPVOID acceptArg);
	static unsigned __stdcall MonitorThread(LPVOID monitorArg);

	int				WorkerThread_Update();
	int				AcceptThread_Update();
	int				MonitorThread_Update();

	//--------------------------------------------------------------------------------
	// Recv, Send 등록
	//--------------------------------------------------------------------------------
	bool			RecvPost(SESSION *pSession, bool bAcceptRecv = false);
	bool			SendPost(SESSION *pSession);

	//--------------------------------------------------------------------------------
	// Recv, Send 완료
	//--------------------------------------------------------------------------------
	void			CompleteRecv(SESSION *pSession, DWORD dwTransferred);
	void			CompleteSend(SESSION *pSession, DWORD dwTransferred);

	//--------------------------------------------------------------------------------
	// 세션 생성
	//--------------------------------------------------------------------------------
	SESSION*		CreateSession(SOCKET socket, WCHAR* wIP, int iPort);

	//--------------------------------------------------------------------------------
	// Disconnect, Release
	//--------------------------------------------------------------------------------
	void			SocketClose(SOCKET socket);
	void			DisconnectSession(SESSION *pSession);
	void			DisconnectSession(__int64 iSessionID);

	void			ReleaseSession(SESSION *pSession);
	void			ReleaseSession(__int64 iSessionID);

public:
	//--------------------------------------------------------------------------------
	// 모니터링 변수들
	//--------------------------------------------------------------------------------
	int				_AcceptCounter;
	int				_AcceptTotalCounter;
	int				_RecvPacketCounter;
	int				_SendPacketCounter;

	int				_AcceptTPS;
	int				_AcceptTotalTPS;
	int				_RecvPacketTPS;
	int				_SendPacketTPS;
	int				_PacketPoolTPS;
	int				_iSessionCount;

protected:
	////////////////////////////////////////////////////////////////////////
	// IOCP Handle
	////////////////////////////////////////////////////////////////////////
	HANDLE				_hIOCP;

	////////////////////////////////////////////////////////////////////////
	// Thread Handle
	////////////////////////////////////////////////////////////////////////
	HANDLE				_hAcceptThread;
	HANDLE				_hWorkerThread[MAX_THREAD];
	HANDLE				_hMonitorThread;

	////////////////////////////////////////////////////////////////////////
	// listen socket
	////////////////////////////////////////////////////////////////////////
	SOCKET				_listen_sock;

	////////////////////////////////////////////////////////////////////////
	// Session List
	////////////////////////////////////////////////////////////////////////
	list<SESSION *>		_SessionList;

	////////////////////////////////////////////////////////////////////////
	// WorkerThread Count
	////////////////////////////////////////////////////////////////////////
	int					_iWorkerThdNum;

	////////////////////////////////////////////////////////////////////////
	// Session에 부여할 ID(Interlocked)
	////////////////////////////////////////////////////////////////////////
	__int64				_iSessionID;

	////////////////////////////////////////////////////////////////////////
	// 쓰레드 종료 flag
	////////////////////////////////////////////////////////////////////////
	bool				_bShutdown;

	////////////////////////////////////////////////////////////////////////
	// Nagle Option
	////////////////////////////////////////////////////////////////////////
	bool				_bNagle;

	////////////////////////////////////////////////////////////////////////
	// SESSION List의 락
	////////////////////////////////////////////////////////////////////////
	CRITICAL_SECTION	_SessionListCS;
};


#endif