#include "stdafx.h"

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
CNPacket::CNPacket()
{
	Initial();
}

CNPacket::CNPacket(int iBufferSize)
{
	Initial(iBufferSize);
}

CNPacket::CNPacket(const CNPacket &clSrcPacket)
{
	/*
	m_iBufferSize = clSrcPacket.m_iBufferSize;
	Initial(m_iBufferSize);
	
	for (int iCnt = clSrcPacket.m_chpReadPos - clSrcPacket.m_chpBuffer;
		iCnt < clSrcPacket.m_chpWritePos - clSrcPacket.m_chpBuffer;
		iCnt++)
	{
		m_chpBuffer[iCnt] = clSrcPacket.m_chpBuffer[iCnt];
	}

	m_chpDataFieldStart = clSrcPacket.m_chpDataFieldStart;
	m_chpDataFieldEnd = clSrcPacket.m_chpDataFieldEnd;

	m_chpReadPos = clSrcPacket.m_chpReadPos;
	m_chpWritePos = clSrcPacket.m_chpWritePos;

	m_iDataSize = clSrcPacket.m_iDataSize;
	*/
}

CNPacket::~CNPacket()
{
	Release();
}

//////////////////////////////////////////////////////////////////////////
// 패킷 초기화.
//
// 메모리 할당을 여기서 하므로, 함부로 호출하면 안된다. 
//
// Parameters: (int)BufferSize.
// Return: 없음.
//////////////////////////////////////////////////////////////////////////
void	CNPacket::Initial(int iBufferSize)
{
	InitializeSRWLock(&srwPacketLock);

	//////////////////////////////////////////////////////////////////////
	// 버퍼 동적 할당
	//////////////////////////////////////////////////////////////////////
	m_chpBuffer = new BYTE[iBufferSize];
	m_iBufferSize = iBufferSize;

	//////////////////////////////////////////////////////////////////////
	// 버퍼 데이터부분 지정
	//////////////////////////////////////////////////////////////////////
	m_chpDataFieldStart = m_chpBuffer + 5;
	m_chpDataFieldEnd = m_chpBuffer + 5;

	m_chpReadPos = m_chpDataFieldStart;
	m_chpWritePos = m_chpDataFieldStart;

	m_chpBufferExpansion = m_chpBuffer;

	m_iDataSize = 0;

	_iRefCnt = 0;
}

//////////////////////////////////////////////////////////////////////////
// 패킷  파괴.
//
// Parameters: 없음.
// Return: 없음.
//////////////////////////////////////////////////////////////////////////
void	CNPacket::Release(void)
{
	delete[] m_chpBuffer;
}



//////////////////////////////////////////////////////////////////////////
// 패킷 청소.
//
// Parameters: 없음.
// Return: 없음.
//////////////////////////////////////////////////////////////////////////
void	CNPacket::Clear(void)
{
	m_chpReadPos = m_chpDataFieldStart;
	m_chpWritePos = m_chpDataFieldStart;

	m_chpDataFieldEnd = m_chpDataFieldStart;

	m_iDataSize = 0;
}

//////////////////////////////////////////////////////////////////////////
// 버퍼 Pos 이동. (음수이동은 안됨)
// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
//
// Parameters: (int) 이동 사이즈.
// Return: (int) 이동된 사이즈.
//////////////////////////////////////////////////////////////////////////
int		CNPacket::MoveWritePos(int iSize)
{
	//////////////////////////////////////////////////////////////////////
	// 이동 시 버퍼크기 보다 커질 경우
	//////////////////////////////////////////////////////////////////////
	if (m_chpWritePos - m_chpDataFieldStart + iSize > m_iBufferSize)
		return 0;

	m_chpWritePos += iSize;

	return iSize;
}

int		CNPacket::MoveReadPos(int iSize)
{
	//////////////////////////////////////////////////////////////////////
	// 이동 시 ReadPos보다 커질 경우
	//////////////////////////////////////////////////////////////////////
	if (m_chpReadPos + iSize > m_chpWritePos)
		return 0;

	m_chpReadPos += iSize;

	return iSize;
}

/* ============================================================================= */
// 연산자 오퍼레이터.
/* ============================================================================= */
/*
CNPacket	&operator = (CNPacket &clSrcPacket)
{

}
*/
void CNPacket::operator << (char chValue){	Put(chValue);	}
void CNPacket::operator << (unsigned char byValue){	Put(byValue);}
void CNPacket::operator << (WCHAR wchValue){	Put(wchValue);}
void CNPacket::operator << (short shValue){	Put(shValue);}
void CNPacket::operator << (int iValue){	Put(iValue);}
void CNPacket::operator << (unsigned int iValue){	Put(iValue);}
void CNPacket::operator << (float fValue){	Put(fValue);}
void CNPacket::operator << (__int64 i64Value){	Put(i64Value);}
void CNPacket::operator << (unsigned __int64 i64Value){	Put(i64Value);}
void CNPacket::operator << (WCHAR *szString){	Put(szString);}

void CNPacket::operator >> (char &chValue){ Get(chValue); }
void CNPacket::operator >> (unsigned char &byValue){ Get(byValue); }
void CNPacket::operator >> (WCHAR &wchValue){ Get(wchValue); }
void CNPacket::operator >> (short &shValue){ Get(shValue); }
void CNPacket::operator >> (int &iValue){ Get(iValue); }
void CNPacket::operator >> (unsigned int &iValue){ Get(iValue); }
void CNPacket::operator >> (float &fValue){ Get(fValue); }
void CNPacket::operator >> (__int64 &i64Value){ Get(i64Value); }
void CNPacket::operator >> (unsigned __int64 &i64Value){ Get(i64Value); }
void CNPacket::operator >> (WCHAR *szString){ Get(szString, (int)wcslen(szString)); }

//////////////////////////////////////////////////////////////////////////
// 넣기.	각 변수 타입마다 모두 만듬.
//////////////////////////////////////////////////////////////////////////
int CNPacket::Put(char chValue)				// C# - sbyte	// signed 1Byte
{
	return PutData((BYTE *)&chValue, eSBYTE);
}

int CNPacket::Put(unsigned char byValue)		// C# - byte	// unsigned 1Byte
{
	return PutData((BYTE *)&byValue, eBYTE);
}

int CNPacket::Put(WCHAR wchValue)			// C# - char	// unicode char 2Byte
{
	return PutData((BYTE *)&wchValue, eWCHAR);
}

int CNPacket::Put(short shValue)				// C# - short	// signed 2Byte
{
	return PutData((BYTE *)&shValue, eSHORT);
}

int CNPacket::Put(unsigned short wValue)		// C# - ushort	// unsigned 2Byte
{
	return PutData((BYTE *)&wValue, eUSHORT);
}

int CNPacket::Put(int iValue)				// C# - int		// signed 4Byte
{
	return PutData((BYTE *)&iValue, eINT);
}

int CNPacket::Put(unsigned int iValue)		// C# - uint	// unsigned 4Byte
{
	return PutData((BYTE *)&iValue, eUINT);
}

int CNPacket::Put(float fValue)				// C# - float	// float 4Byte
{
	return PutData((BYTE *)&fValue, eFLOAT);
}

int CNPacket::Put(__int64 i64Value)			// C# - long	// signed 8Byte
{
	return PutData((BYTE *)&i64Value, eINT64);
}

int CNPacket::Put(unsigned __int64 i64Value)	// C# - ulong	// unsigned 8Byte
{
	return PutData((BYTE *)&i64Value, eUINT64);
}

/*
int	CNPacket::Put(Proud::String &String)		// 문자열 넣기
{

}
*/

int	CNPacket::Put(WCHAR *szString)
{
	return PutData((BYTE*)szString, (int)((wcslen(szString) + 1) * 2));
}

int CNPacket::Put(char *Lump, int iSize)
{
	return PutData((BYTE*)Lump, iSize);
}
//////////////////////////////////////////////////////////////////////////
// 빼기.	각 변수 타입마다 모두 만듬.
//////////////////////////////////////////////////////////////////////////

int CNPacket::Get(unsigned char &byValue) const
{
	return GetData((BYTE *)&byValue, eSBYTE);
}

int CNPacket::Get(char &chValue) const
{
	return GetData((BYTE *)&chValue, eBYTE);
}

int CNPacket::Get(WCHAR &wchValue) const
{
	return GetData((BYTE *)&wchValue, eWCHAR);
}

int CNPacket::Get(short &shValue) const
{
	return GetData((BYTE *)&shValue, eSHORT);
}

int CNPacket::Get(unsigned short &wValue) const
{
	return GetData((BYTE *)&wValue, eUSHORT);
}

int CNPacket::Get(int &iValue) const
{
	return GetData((BYTE *)&iValue, eINT);
}

int CNPacket::Get(unsigned int &iValue) const
{
	return GetData((BYTE *)&iValue, eUINT);
}

int CNPacket::Get(float &fValue) const
{
	return GetData((BYTE *)&fValue, eFLOAT);
}

int CNPacket::Get(__int64 &i64Value) const			// C# - long
{
	return GetData((BYTE *)&i64Value, eINT64);
}

int CNPacket::Get(unsigned __int64 &i64Value) const	// C# - ulong
{
	return GetData((BYTE *)&i64Value, eUINT64);
}

/*
int	CNPacket::Get(Proud::String &String) const		// 문자열 빼기
{

}
*/

int	CNPacket::Get(WCHAR *szString, int iBuffer) const
{
	return GetData((BYTE *)szString, iBuffer);
}

int CNPacket::Get(char *Lump, int iSize)
{
	return GetData((BYTE*)Lump, iSize);
}
//////////////////////////////////////////////////////////////////////////
// 데이타 얻기.
//
// Parameters: (unsigned char *)Dest 포인터. (int)Size.
// Return: (int)복사한 사이즈.
//////////////////////////////////////////////////////////////////////////
int		CNPacket::GetData(unsigned char *bypDest, int iSize) const
{
	int iCnt;

	for (iCnt = 0; iCnt < iSize; iCnt++)
	{
		bypDest[iCnt] = *m_chpReadPos;
		m_chpReadPos++;
		m_iDataSize--;
	}

	return iCnt;
}

//////////////////////////////////////////////////////////////////////////
// 데이타 삽입.
//
// Parameters: (unsigned char *)Src 포인터. (int)SrcSize.
// Return: (int)복사한 사이즈.
//////////////////////////////////////////////////////////////////////////
int		CNPacket::PutData(unsigned char *bypSrc, int iSrcSize)
{
	int iCnt;

	for (iCnt = 0; iCnt < iSrcSize; iCnt++)
	{
		*m_chpWritePos = bypSrc[iCnt];
		m_chpWritePos++;
		m_iDataSize++;
	}

	return iCnt;
}

//////////////////////////////////////////////////////////////////////////
// 패킷 헤더 셋팅
//
// Parameters: (unsigned char *)Src 포인터. (int)SrcSize.
// Return: (int)복사한 사이즈.
//////////////////////////////////////////////////////////////////////////
void		CNPacket::SetHeader(char *pHeader)
{
	memcpy(m_chpBufferExpansion, pHeader, 5);
	m_iDataSize += 5;
}

//////////////////////////////////////////////////////////////////////////
// 패킷 헤더 셋팅(Custom)
//
// Parameters: (char *) 넣을 헤더, (int) 헤더 사이즈
// Return: 없음.
//////////////////////////////////////////////////////////////////////////
void	CNPacket::SetCustomHeader(char *pHeader, int iCustomHeaderSize)
{
	m_chpBufferExpansion = m_chpBuffer + 5 - iCustomHeaderSize;
	memcpy(m_chpBufferExpansion, pHeader, iCustomHeaderSize);
	m_iDataSize += iCustomHeaderSize;
}

//////////////////////////////////////////////////////////////////////////
// 패킷 헤더 셋팅(2byte Echo 전용)
//
// Parameters: (unsigned short) 넣을 헤더.
// Return: 없음.
//////////////////////////////////////////////////////////////////////////
void	CNPacket::SetCustomShortHeader(unsigned short Header)
{
	m_chpBufferExpansion += 3;
	*((unsigned short *)m_chpBufferExpansion) = Header;
	m_iDataSize += 2;
}

//////////////////////////////////////////////////////////////////////////
// 패킷 메모리 할당.
//
// Parameters: 없음.
// Return: (CNPacket *)패킷 포인터.
//////////////////////////////////////////////////////////////////////////
CNPacket *CNPacket::Alloc()
{
	CNPacket *pPacket = new CNPacket();

	//new (pPacket)CNPacket;

	pPacket->addRef();
	pPacket->Clear();

	return pPacket;
}


//////////////////////////////////////////////////////////////////////////
// 패킷 메모리 해제.
//
// Parameters: (CNPacket *)패킷 포인터.
// Return: 없음.
//////////////////////////////////////////////////////////////////////////
void CNPacket::Free()
{
	int retval = InterlockedDecrement64((LONG64 *)&_iRefCnt);
	if (0 == retval)
	{
		delete this;
	}

	//else if (retval < 0)
		//CCrashDump::Crash();
}


//////////////////////////////////////////////////////////////////////////
// 패킷 락.
//
// Parameters: 없음.
// Return: 없음.
//////////////////////////////////////////////////////////////////////////
void CNPacket::Lock()
{
	AcquireSRWLockExclusive(&srwPacketLock);
}

//////////////////////////////////////////////////////////////////////////
// 패킷 락 해제.
//
// Parameters: 없음.
// Return: 없음.
//////////////////////////////////////////////////////////////////////////
void CNPacket::Unlock()
{
	ReleaseSRWLockExclusive(&srwPacketLock);
}