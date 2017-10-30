#ifndef __IOCP_ECHOSERVER__H__
#define	__IOCP_ECHOSERVER__H__

enum
{
	MAX_THREAD = 10,
	MAX_SESSION = 200
};

typedef struct st_SESSION
{
	SOCKET _socket;

	CAyaStreamSQ _RecvQ;
	CAyaStreamSQ _SendQ;

	OVERLAPPED _RecvOverlapped;
	OVERLAPPED _SendOverlapped;

	LONG _lIOCount;
	bool _bSendFlag;
} SESSION;


#define dfSERVER_PORT			5000
#define dfSERVER_IP				L"127.0.0.1"

#endif

// result == false && pOverlapped == NULL
// ->IOCP Error -> WorkerThread를 꺼야함

// transferrend == 0
// => 세션종료

