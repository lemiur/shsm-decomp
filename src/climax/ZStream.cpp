// Unit name and boundary: Gormiti link map (ZStream.obj).
// Names and layout: Gormiti link map and DWARF (SpiritFrameWork/FileSystem/ZStream.cpp).
// ZStream::ZStream and ZStream::~ZStream are inline in retail (CreateZStream and IFStreamClose hold
// their code). StreamRead, FillReadBuffer, StreamSkip and GetZStreamPos have no copy in retail
// (inlined); their statements follow Gormiti's Debug build, where FillReadBuffer hands the buffer to zlib
// and StreamRead reads the available count back from the zlib stream. Retail's IFStreamRead opens with the
// check that a NULL buffer is only accepted with a zero length.

#include <climax/ZStream.h>
#include <climax/Heap.h>

// The heap and flags the stream is allocated with; meaning unknown. Names invented.
#define ZSTREAM_HEAP 2
#define ZSTREAM_HEAP_FLAGS 0x10040000

// The flags of the allocations zlib makes; meaning unknown. Name invented.
#define ZSTREAM_ZALLOC_FLAGS 0x10000000

/*
====================
CreateZStream

A custom RenderWare stream that inflates the cmpSize bytes of stream into deCmpSize bytes.
====================
*/
// 0x801B4464
RwStream* CreateZStream( RwStream* Stream, int CmpSize, int DeCmpSize ) {
    if ( Stream == NULL ) {
        return NULL;
    }
    if ( CmpSize == 0 ) {
        return NULL;
    }
    if ( DeCmpSize == 0 ) {
        return NULL;
    }

    ZStream* zStream = new ( ZSTREAM_HEAP, ZSTREAM_HEAP_FLAGS ) ZStream( Stream, CmpSize, DeCmpSize );

    RwStreamCustom CustomStreamData;
    CustomStreamData.sfnclose = ZStream::IFStreamClose;
    CustomStreamData.sfnread = ZStream::IFStreamRead;
    CustomStreamData.sfnwrite = ZStream::IFStreamWrite;
    CustomStreamData.sfnskip = ZStream::IFStreamSkip;
    CustomStreamData.data = zStream;
    return RwStreamOpen( rwSTREAMCUSTOM, rwSTREAMREAD, &CustomStreamData );
}

/*
====================
ZStream::IF_ZAlloc

zlib's allocator.
====================
*/
// 0x801B4588
void* ZStream::IF_ZAlloc( void* opaque, unsigned int items, unsigned int size ) {
    return RWSMalloc( items * size, ZSTREAM_ZALLOC_FLAGS );
}

/*
====================
ZStream::IF_ZFree

zlib's deallocator.
====================
*/
// 0x801B4594
void ZStream::IF_ZFree( void* opaque, void* address ) {
    fn_801AD1BC( address );
}

/*
====================
ZStream::IFStreamClose

Closes the stream: ends the inflation and frees the ZStream.
====================
*/
// 0x801B459C
RwBool ZStream::IFStreamClose( void* data ) {
    delete (ZStream*)data;
    return TRUE;
}

/*
====================
ZStream::FillReadBuffer

No copy in retail (inlined).

Tops up the read buffer from the compressed stream: moves the unconsumed bytes to the front once the read
position has passed the first 4 KB, then reads as much as fits, and gives zlib the unconsumed bytes.
====================
*/
inline void ZStream::FillReadBuffer() {
    if ( m_ReadBufferPos >= 0x1000 && m_ReadBufferData > 0 ) {
        char* src = &m_ReadBuffer[m_ReadBufferPos];
        char* dst = m_ReadBuffer;
        int count = m_ReadBufferData;
        if ( ( ( m_ReadBufferPos | m_ReadBufferData ) & 3 ) != 0 ) {
            for ( ; count > 0; count-- ) {
                *dst = *src;
                dst++;
                src++;
            }
        } else {
            unsigned int* src32 = (unsigned int*)src;
            unsigned int* dst32 = (unsigned int*)dst;
            count = count / 4;
            while ( count > 0 ) {
                *dst32++ = *src32++;
                count--;
            }
        }
        m_ReadBufferPos = 0;
    } else if ( m_ReadBufferData == 0 ) {
        m_ReadBufferPos = 0;
    }

    int sparedataspace = sizeof( m_ReadBuffer ) - ( m_ReadBufferData + m_ReadBufferPos );
    if ( sparedataspace >= 0x1000 ) {
        char* ReadPoint = &m_ReadBuffer[m_ReadBufferPos + m_ReadBufferData];
        int ReadSize = sparedataspace;
        if ( ReadSize > m_SrcStreamData ) {
            ReadSize = m_SrcStreamData;
        }
        if ( ReadSize > 0 ) {
            int readdata = RwStreamRead( m_SrcStream, ReadPoint, ReadSize );
            m_ReadBufferData += readdata;
            m_SrcStreamData -= readdata;
        }
    }

    m_ZStream.avail_in = m_ReadBufferData;
    m_ZStream.next_in = (Bytef*)&m_ReadBuffer[m_ReadBufferPos];
}

/*
====================
ZStream::StreamRead

No copy in retail (inlined).

Inflates up to Length bytes into pBuffer and returns the count; 0 when zlib reports an error. A Length of
0 asks for the inflated bytes still to come.
====================
*/
inline RwUInt32 ZStream::StreamRead( void* pBuffer, RwUInt32 Length ) {
    int TotalOutput = 0;
    if ( Length == 0 ) {
        return m_DstDataTotal - m_ZStream.total_out;
    }

    char* pDst = (char*)pBuffer;
    m_ZStream.next_out = (Bytef*)pDst;
    m_ZStream.avail_out = Length;
    while ( m_ZStream.avail_out != 0 && (uLong)m_DstDataTotal > m_ZStream.total_out ) {
        FillReadBuffer();

        int CurrentAvailable = m_ZStream.avail_in;
        int CurrentOutput = m_ZStream.total_out;
        int ret = inflate( &m_ZStream, Z_NO_FLUSH );
        int UsedData = CurrentAvailable - m_ZStream.avail_in;
        int OutputData = m_ZStream.total_out - CurrentOutput;
        TotalOutput += OutputData;

        if ( UsedData != 0 ) {
            m_ReadBufferData -= UsedData;
            m_ReadBufferPos += UsedData;
        }
        if ( ret == Z_STREAM_END ) {
            break;
        }
        switch ( ret ) {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
        case Z_BUF_ERROR:
            return 0;
        }
    }
    m_StreamPos += TotalOutput;
    return TotalOutput;
}

/*
====================
ZStream::StreamSkip

No copy in retail (inlined).

Inflates and discards Offset bytes, a kilobyte at a time.
====================
*/
inline int ZStream::StreamSkip( RwUInt32 Offset ) {
    char TempBuf[1024];
    int RemainingSkip = Offset;
    while ( RemainingSkip > 0 ) {
        int readsize = RemainingSkip;
        if ( readsize > 1024 ) {
            readsize = 1024;
        }
        StreamRead( TempBuf, readsize );
        RemainingSkip -= readsize;
    }
    return TRUE;
}

/*
====================
ZStream::IFStreamRead

RenderWare's read callback: inflates length bytes into buffer.
====================
*/
// 0x801B45E0
RwUInt32 ZStream::IFStreamRead( void* data, void* buffer, RwUInt32 length ) {
    ZStream* This = (ZStream*)data;
    if ( buffer == NULL && length != 0 ) {
        return 0;
    }
    return This->StreamRead( buffer, length );
}

/*
====================
ZStream::IFStreamWrite

The stream is read-only.
====================
*/
// 0x801B48D4
RwBool ZStream::IFStreamWrite( void* data, const void* buffer, RwUInt32 length ) {
    return FALSE;
}

/*
====================
ZStream::IFStreamSkip

RenderWare's skip callback: inflates and discards offset bytes.
====================
*/
// 0x801B48DC
RwBool ZStream::IFStreamSkip( void* data, RwUInt32 offset ) {
    ZStream* This = (ZStream*)data;
    return This->StreamSkip( offset );
}
