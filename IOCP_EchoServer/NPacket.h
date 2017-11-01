/*---------------------------------------------------------------

	N Packet.

	네트워크 패킷용 클래스.
	간편하게 패킷에 순서대로 데이타를 In, Out 한다.

	플랫폼에 따라서 변수별 사이즈가 다르므로, 네트워크 패킷 전송시에 문제가 발생할 수 있다.
	그리하여 플랫폼별 차이점을 위한 고정사항은 다음과 같다.

	--------------------------------------------------------

		NPacket::_ValueSizeCheck()  
		
		함수를 최초 1회 호출하여 각 변수별 사이즈 확인을 필수로 해야한다.
		FALSE 리턴시 개발자에게 상의해라.


		아래 사이즈와 다를 경우 컴파일 시에러를 발생한다.

		C					C#				TYPE
		char				sbyte			signed 1Byte
		unsigned char		byte			unsigned 1Byte
		WCHAR				char			unicode char 2Byte

		short				short			signed 2Byte
		unsigned short		ushort			unsigned 2Byte

		int					int				signed 4Byte
		unsigned int		uint			unsigned 4Byte
		float				float			float 4Byte

		__int64				long			signed 8Byte
		unsigned __int64	ulong			unsigned 8Byte
		
	--------------------------------------------------------


	- 사용법.

	CNPacket cPacket;

	넣기.
	clPacket << 40030;	or	clPacket << iValue;		(int 넣기)
	clPacket << 3;		or	clPacket << byValue;	(BYTE 넣기)
	clPacket << 1.4;	or	clPacket << fValue;		(float 넣기)

	빼기.
	clPacket >> iValue;		(int 빼기)
	clPacket >> byValue;	(BYTE 빼기)
	clPacket >> fValue;		(float 빼기)

	!.	삽입되는 데이타 FIFO 순서로 관리된다.
		큐가 아니므로, 넣기(<<).빼기(>>) 를 혼합해서 사용하면 안된다.
		
----------------------------------------------------------------*/
#ifndef  __N_PACKET__
#define  __N_PACKET__

class CNPacket
{
public:
	
	/*---------------------------------------------------------------
	AyaPacket Enum.

	----------------------------------------------------------------*/
	enum enN_PACKET
	{
		eBUFFER_DEFAULT			= 5000,	// 패킷의 기본 버퍼 사이즈.
		eSTRING_MAX				= 1024,		// 문자열 입력시 최대길이

		eSBYTE					= 1,
		eBYTE					= 1,
		eWCHAR					= 2,

		eSHORT					= 2,
		eUSHORT					= 2,
		eINT					= 4,
		eUINT					= 4,
		eFLOAT					= 4,

		eINT64					= 8,		// C# 에선 long 임.
		eUINT64					= 8
	};

	//////////////////////////////////////////////////////////////////////////
	// 생성자, 파괴자.
	//
	// Parameters:
	//				---------------------------------------------------------
	//				(int) 버퍼 사이즈.
	//				---------------------------------------------------------
	//				(const CNPacket &) 패킷의 복사를 위한 패킷 레퍼런스.
	// Return:
	//////////////////////////////////////////////////////////////////////////
			CNPacket();
			CNPacket(int iBufferSize);
			CNPacket(const CNPacket &clSrcPacket);

	virtual	~CNPacket();


	//////////////////////////////////////////////////////////////////////////
	// 현 시스템의 변수 길이 체크.
	//
	// Parameters: 없음.
	// Return: (bool)true,false.
	//////////////////////////////////////////////////////////////////////////
	static bool	_ValueSizeCheck(void)
	{
		// 1 byte 변수 
		if ( eSBYTE != sizeof(char) || eBYTE != sizeof(unsigned char) ) return false;

		// 2 byte 변수 
		if ( eWCHAR != sizeof(WCHAR) || eSHORT != sizeof(short) || eUSHORT != sizeof(unsigned short)) return false;

		// 4 byte 변수 
		if ( eINT != sizeof(int) || eUINT != sizeof(unsigned int) || eFLOAT != sizeof(float)) return false;

		// 8 byte 변수 
		if ( eINT64 != sizeof(__int64) || eUINT64 != sizeof(unsigned __int64)) return false;
		return true;
	}


	//////////////////////////////////////////////////////////////////////////
	// 패킷 초기화.
	//
	// 메모리 할당을 여기서 하므로, 함부로 호출하면 안된다. 
	//
	// Parameters: (int)BufferSize.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void	Initial(int iBufferSize = eBUFFER_DEFAULT);
	//////////////////////////////////////////////////////////////////////////
	// 패킷  파괴.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void	Release(void);



	//////////////////////////////////////////////////////////////////////////
	// 패킷 청소.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void	Clear(void);


	//////////////////////////////////////////////////////////////////////////
	// 버퍼 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)패킷 버퍼 사이즈 얻기.
	//////////////////////////////////////////////////////////////////////////
	int		GetBufferSize(void) { return m_iBufferSize; }

	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 사이즈 얻기.(헤더 미포함)
	//
	// Parameters: 없음.
	// Return: (int)사용중인 데이타 사이즈.
	//////////////////////////////////////////////////////////////////////////
	const int		GetDataSize(void) const 
	{ 
		return m_iDataSize; 
	}

	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 사이즈 얻기.(헤더 포함)
	//
	// Parameters: 없음.
	// Return: (int)사용중인 데이타 사이즈.
	//////////////////////////////////////////////////////////////////////////
	const int		GetPacketSize(void) const { return m_iDataSize; }

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터 얻기.(데이터 부분)
	//
	// Parameters: 없음.
	// Return: (unsigned char *)버퍼 포인터.
	//////////////////////////////////////////////////////////////////////////
	unsigned char *GetBufferPtr(void) const { return m_chpDataFieldStart; }

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터 얻기.(헤더 시작 부분)
	//
	// Parameters: 없음.
	// Return: (unsigned char *)버퍼 포인터.
	//////////////////////////////////////////////////////////////////////////
	unsigned char *GetHeaderBufferPtr(void) const { return m_chpBufferExpansion; }

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 Pos 이동. (음수이동은 안됨)
	// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
	//
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		MoveWritePos(int iSize);
	int		MoveReadPos(int iSize);


	/* ============================================================================= */
	// 연산자 오퍼레이터.
	/* ============================================================================= */
	CNPacket	&operator = (CNPacket &clSrcPacket);

	void		operator << (char chValue);
	void		operator << (unsigned char byValue);
	void		operator << (WCHAR wchValue);
	void		operator << (short shValue);
	void		operator << (int iValue);
	void		operator << (unsigned int iValue);
	void		operator << (float fValue);
	void		operator << (__int64 i64Value);
	void		operator << (unsigned __int64 i64Value);
	void		operator << (WCHAR *szString);

	void		operator >> (char &chValue);
	void		operator >> (unsigned char &byValue);
	void		operator >> (WCHAR &wchValue);
	void		operator >> (short &shValue);
	void		operator >> (int &iValue);
	void		operator >> (unsigned int &iValue);
	void		operator >> (float &fValue);
	void		operator >> (__int64 &i64Value);
	void		operator >> (unsigned __int64 &i64Value);
	void		operator >> (WCHAR *szString);

	//////////////////////////////////////////////////////////////////////////
	// 넣기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	int Put(char chValue);				// C# - sbyte	// signed 1Byte
	int Put(unsigned char byValue);		// C# - byte	// unsigned 1Byte
	int Put(WCHAR wchValue);			// C# - char	// unicode char 2Byte

	int Put(short shValue);				// C# - short	// signed 2Byte
	int Put(unsigned short wValue);		// C# - ushort	// unsigned 2Byte

	int Put(int iValue);				// C# - int		// signed 4Byte
	int Put(unsigned int iValue);		// C# - uint	// unsigned 4Byte
	int Put(float fValue);				// C# - float	// float 4Byte

	int Put(__int64 i64Value);			// C# - long	// signed 8Byte
	int Put(unsigned __int64 i64Value);	// C# - ulong	// unsigned 8Byte

	//int	Put(Proud::String &String);		// 문자열 넣기
	int	Put(WCHAR *szString);

	int Put(char *Lump, int iSize);

	//////////////////////////////////////////////////////////////////////////
	// 빼기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////

	int Get(unsigned char &byValue) const;
	int Get(char &chValue) const;
	int Get(WCHAR &wchValue) const;

	int Get(short &shValue) const;
	int Get(unsigned short &wValue) const;

	int Get(int &iValue) const;
	int Get(unsigned int &iValue) const;
	int Get(float &fValue) const;

	int Get(__int64 &i64Value) const;			// C# - long
	int Get(unsigned __int64 &i64Value) const;	// C# - ulong

	//int	Get(Proud::String &String) const;		// 문자열 빼기
	int	Get(WCHAR *szString, int iBuffer) const;

	int Get(char *Lump, int iSize);

	//////////////////////////////////////////////////////////////////////////
	// 데이타 얻기.
	//
	// Parameters: (unsigned char *)Dest 포인터. (int)Size.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		GetData(unsigned char *bypDest, int iSize) const;

	//////////////////////////////////////////////////////////////////////////
	// 데이타 삽입.
	//
	// Parameters: (unsigned char *)Src 포인터. (int)SrcSize.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		PutData(unsigned char *bypSrc, int iSrcSize);


	//////////////////////////////////////////////////////////////////////////
	// 패킷 헤더 셋팅
	//
	// Parameters: (char *) 넣을 헤더.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void	SetHeader(char *pHeader);

	//////////////////////////////////////////////////////////////////////////
	// 패킷 헤더 셋팅(Custom)
	//
	// Parameters: (char *) 넣을 헤더, (int) 헤더 사이즈
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void	SetCustomHeader(char *pHeader, int iCustomHeaderSize);

	//////////////////////////////////////////////////////////////////////////
	// 패킷 헤더 셋팅(2byte Echo 전용)
	//
	// Parameters: (unsigned short) 넣을 헤더.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void	SetCustomShortHeader(unsigned short Header);


	//////////////////////////////////////////////////////////////////////////
	// 패킷 메모리 할당.
	//
	// Parameters: 없음.
	// Return: (CNPacket *)패킷 포인터.
	//////////////////////////////////////////////////////////////////////////
	static CNPacket	*Alloc();


	//////////////////////////////////////////////////////////////////////////
	// 패킷 메모리 해제.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void Free();

	//////////////////////////////////////////////////////////////////////////
	// RefCnt 증가
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void addRef(){ InterlockedIncrement64((LONG64 *)&_iRefCnt); }

	//////////////////////////////////////////////////////////////////////////
	// 패킷 락.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void Lock();

	//////////////////////////////////////////////////////////////////////////
	// 패킷 락 해제.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void Unlock();

protected:
	//------------------------------------------------------------
	// 할당 카운트
	//------------------------------------------------------------
	int								_iRefCnt;

	//------------------------------------------------------------
	// 패킷버퍼 / 버퍼 사이즈.
	//------------------------------------------------------------
	unsigned char					*m_chpBufferExpansion;

	unsigned char					*m_chpBuffer;
	int								m_iBufferSize;
	//------------------------------------------------------------
	// 패킷버퍼 시작 위치.	(본 클래스 에서는 사용하지 않지만, 확장성을 위해 사용)
	//------------------------------------------------------------
	unsigned char					*m_chpDataFieldStart;
	unsigned char					*m_chpDataFieldEnd;


	//------------------------------------------------------------
	// 버퍼의 읽을 위치, 넣을 위치.
	//------------------------------------------------------------
	mutable unsigned char			*m_chpReadPos;
	mutable unsigned char			*m_chpWritePos;


	//------------------------------------------------------------
	// 현재 버퍼에 사용중인 사이즈.
	//------------------------------------------------------------
	mutable int						m_iDataSize;

	//------------------------------------------------------------
	// SRWLock
	//------------------------------------------------------------
	SRWLOCK							srwPacketLock;
};

#endif
