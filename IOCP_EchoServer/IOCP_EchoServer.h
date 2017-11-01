#ifndef __IOCP_ECHOSERVER__H__
#define	__IOCP_ECHOSERVER__H__

enum
{
	MAX_THREAD = 10,
	MAX_SESSION = 200
};

typedef struct st_SESSION
{
	__int64			_iSessionID;

	SOCKET			_socket;

	CAyaStreamSQ	_RecvQ;
	CAyaStreamSQ	_SendQ;

	OVERLAPPED		_RecvOverlapped;
	OVERLAPPED		_SendOverlapped;

	LONG			_lIOCount;
	bool			_bSendFlag;
} SESSION;


#define dfSERVER_PORT			6000
#define dfSERVER_IP				L"127.0.0.1"

bool					initIOCPServer();

unsigned __stdcall		AcceptThread(LPVOID acceptArg);
unsigned __stdcall		WorkerThread(LPVOID workerArg);

void					CompleteRecv(SESSION *pSession, DWORD dwTransferred);
void					CompleteSend(SESSION *pSession, DWORD dwTransferred);

bool					SendPacket(__int64 iSessionID, __int64 iPacket);

SESSION					*CreateSession(SOCKET socket);

bool					RecvPost(SESSION *pSession);
bool					SendPost(SESSION *pSession);

void					DisconnectSession(SESSION *pSession);
void					ReleaseSession(SESSION *pSession);
void					SocketClose(SOCKET socket);

#endif

// result == false && pOverlapped == NULL
// ->IOCP Error -> WorkerThread를 꺼야함

// transferrend == 0
// => 세션종료

