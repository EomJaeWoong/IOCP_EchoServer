#ifndef __LANSERVER__H__
#define __LANSERVER__H__

enum
{
	// �ִ� ������ ��
	MAX_THREAD = 50,

	// �ִ� ������ ��
	MAX_SESSION = 200,

	
};

//-------------------------------------------------------------------------------------
// ���� ���� ����ü
//-------------------------------------------------------------------------------------
typedef struct stSESSION_INFO
{
	SOCKET _socket;
	WCHAR _IP[16];
	int _iPort;

} SESSION_INFO;

//-------------------------------------------------------------------------------------
// ����
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
	// ������, �Ҹ���
	//---------------------------------------------------------------------------------
	CLanServer();
	virtual ~CLanServer();

	//---------------------------------------------------------------------------------
	// ���� ����, ����
	//---------------------------------------------------------------------------------
	bool			Start(WCHAR *wOpenIP, int iPort, int iWorkerThdNum, bool bNagle, int iMaxConnection);
	void			Stop();

	//---------------------------------------------------------------------------------
	// ���� Session ��
	//---------------------------------------------------------------------------------
	int				GetClientCount(){ return _iSessionCount; }

protected:
	//---------------------------------------------------------------------------------
	// ��Ŷ ������
	//---------------------------------------------------------------------------------
	bool			SendPacket(__int64 iSessionID, CNPacket *pPacket);

	//---------------------------------------------------------------------------------
	// OnClientJoin			-> ����ó�� �Ϸ� �� ȣ��
	// OnConnectionLeave		-> Disconnect�� ȣ��
	// OnConnectionRequest		-> Accept ���� ȣ��
	//		return false; �� Ŭ���̾�Ʈ �ź�
	//		return true; �� ���� ���
	// OnRecv				-> ��Ŷ ���� �Ϸ� ��
	// OnSend				-> ��Ŷ �۽� �Ϸ� ��
	// OnWorkerThreadBegin		-> ��Ŀ������ GQCS �ٷ� �ϴܿ��� ȣ��
	// OnWorkerThreadEnd		-> ��Ŀ������ 1���� ��
	// OnError				-> ���� �޽���
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
	// ������ ����
	//---------------------------------------------------------------------------------
	static unsigned __stdcall WorkerThread(LPVOID workerArg);
	static unsigned __stdcall AcceptThread(LPVOID acceptArg);
	static unsigned __stdcall MonitorThread(LPVOID monitorArg);

	int				WorkerThread_Update();
	int				AcceptThread_Update();
	int				MonitorThread_Update();

	//--------------------------------------------------------------------------------
	// Recv, Send ���
	//--------------------------------------------------------------------------------
	bool			RecvPost(SESSION *pSession, bool bAcceptRecv = false);
	bool			SendPost(SESSION *pSession);

	//--------------------------------------------------------------------------------
	// Recv, Send �Ϸ�
	//--------------------------------------------------------------------------------
	void			CompleteRecv(SESSION *pSession, DWORD dwTransferred);
	void			CompleteSend(SESSION *pSession, DWORD dwTransferred);

	//--------------------------------------------------------------------------------
	// ���� ����
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
	// ����͸� ������
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
	// Session�� �ο��� ID(Interlocked)
	////////////////////////////////////////////////////////////////////////
	__int64				_iSessionID;

	////////////////////////////////////////////////////////////////////////
	// ������ ���� flag
	////////////////////////////////////////////////////////////////////////
	bool				_bShutdown;

	////////////////////////////////////////////////////////////////////////
	// Nagle Option
	////////////////////////////////////////////////////////////////////////
	bool				_bNagle;

	////////////////////////////////////////////////////////////////////////
	// SESSION List�� ��
	////////////////////////////////////////////////////////////////////////
	CRITICAL_SECTION	_SessionListCS;
};


#endif