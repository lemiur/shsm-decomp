// Names and layout: Gormiti link map and DWARF (SpiritFrameWork/FileSystem/ZStream.cpp)
#ifndef CLIMAX_ZSTREAM_H
#define CLIMAX_ZSTREAM_H

#include <rwsdk/plcore/bastream.h>
#include <zlib/zlib.h>

/*
====================
ZStream

A RenderWare custom stream that inflates a zlib-compressed stream as it is read. The stream's
callbacks (the IF members) are RenderWare's custom stream interface; its data is the ZStream.
====================
*/
class ZStream {
public:
    RwStream*   m_SrcStream;            // 0x00: the compressed stream
    int         m_SrcStreamData;        // 0x04: its compressed bytes not yet read
    char        m_ReadBuffer[8192];     // 0x08
    int         m_ReadBufferPos;        // 0x2008
    int         m_ReadBufferData;       // 0x200C
    int         m_DstDataTotal;         // 0x2010: the inflated size
    int         m_StreamPos;            // 0x2014
    z_stream    m_ZStream;              // 0x2018

    ZStream( RwStream* stream, int cmpSize, int deCmpSize ) {
        m_SrcStream = stream;
        m_SrcStreamData = cmpSize;
        m_DstDataTotal = deCmpSize;
        m_ReadBufferPos = 0;
        m_ReadBufferData = 0;
        m_StreamPos = 0;
        m_ZStream.zalloc = IF_ZAlloc;
        m_ZStream.zfree = IF_ZFree;
        m_ZStream.opaque = Z_NULL;
        m_ZStream.avail_in = 0;
        m_ZStream.next_in = Z_NULL;
        inflateInit( &m_ZStream );
    }

    ~ZStream() {
        inflateEnd( &m_ZStream );
    }

    static void*        IF_ZAlloc( void* opaque, unsigned int items, unsigned int size );
    static void         IF_ZFree( void* opaque, void* address );
    static RwBool       IFStreamClose( void* data );
    static RwUInt32     IFStreamRead( void* data, void* buffer, RwUInt32 length );
    static RwBool       IFStreamWrite( void* data, const void* buffer, RwUInt32 length );
    static RwBool       IFStreamSkip( void* data, RwUInt32 offset );

    // No copy in retail (inlined into the IF callbacks); Gormiti has out-of-line copies.
    inline void         FillReadBuffer();
    inline RwUInt32     StreamRead( void* pBuffer, RwUInt32 Length );
    inline int          StreamSkip( RwUInt32 Offset );
};

// A stream that inflates the cmpSize bytes of the stream into deCmpSize bytes; NULL on a bad argument.
RwStream* CreateZStream( RwStream* stream, int cmpSize, int deCmpSize );

#endif
