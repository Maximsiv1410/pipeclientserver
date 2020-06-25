#pragma once
#pragma warning( disable : 4018 )

#include <boost/assert.hpp>
#include "PipeCommon.h"		//for partial specialization
#include "data_bufer.h"

template< typename _TOwner, typename _TEnvStruct >
class COVEnvelopMaker{


};//COVEnvelopMaker

/////////////////////////////////////////////////////////////////////////////////////////

template< typename _TOwner >
class COVEnvelopMaker< _TOwner, SPS_ACTIONPACKET >{
private:
	_TOwner	*m_pOwner;

protected:
	COVEnvelopMaker()
	{
		m_pOwner = static_cast< _TOwner* >( this );
	}
	//
	~COVEnvelopMaker()
	{}
protected:
	void MakeEnvelop( SPS_ACTIONPACKET *pActionPacket,
					  unsigned char *pbViewportData,
					  unsigned long cbViewportData,
					  unsigned long cTransfered,
					  HANDLE hCurPipe,
					  HANDLE );
	void ClearEnvelop( SPS_ACTIONPACKET *pActionPacket );
	//
	void prepare_envelop_for_next_message( SPS_ACTIONPACKET *pActionPacket );
	DWORD get_stream_content_size( IStream *pIStmObj );
};

template< typename _TOwner >
DWORD
COVEnvelopMaker< _TOwner, SPS_ACTIONPACKET >::get_stream_content_size( IStream *pIStmObj )
{
	if( !pIStmObj ){
		return 0;
	}
	STATSTG stat;
	HRESULT hRes = pIStmObj->Stat(&stat, STATFLAG_NONAME);
	if( FAILED( hRes ) ){
		return 0;
	}
	return stat.cbSize.LowPart;
}

template< typename _TOwner >
void
COVEnvelopMaker< _TOwner, SPS_ACTIONPACKET >::ClearEnvelop( SPS_ACTIONPACKET *pActionPacket )
{
	if( !pActionPacket ){
		return;
	}
	if( pActionPacket->pIStmCompletionBuffer ){
		pActionPacket->pIStmCompletionBuffer->Release();
		pActionPacket->pIStmCompletionBuffer = 0;
	}
}

template< typename _TOwner >
void
COVEnvelopMaker< _TOwner, SPS_ACTIONPACKET >::prepare_envelop_for_next_message( SPS_ACTIONPACKET *pActionPacket )
{
	if( !pActionPacket ){
		return;
	}
	pActionPacket->nActionId = 0;
	pActionPacket->dwClientLabel = 0;
	pActionPacket->ftTimeStamp = FILETIME();
	pActionPacket->nMetadataSize = -1;
	if( pActionPacket->pIStmCompletionBuffer ){
		ULARGE_INTEGER ulEmpty = { 0 };
		pActionPacket->pIStmCompletionBuffer->Seek(LARGE_INTEGER(), STREAM_SEEK_SET, nullptr);
		pActionPacket->pIStmCompletionBuffer->SetSize( ulEmpty );
	}
}

template< typename _TOwner >
void
COVEnvelopMaker< _TOwner, SPS_ACTIONPACKET >::MakeEnvelop( SPS_ACTIONPACKET *pActionPacket,
														   unsigned char *pbViewportData,
														   unsigned long cbViewportData,
														   unsigned long cTransfered,
														   HANDLE hCurPipe,
														   HANDLE )
{
	if( !m_pOwner ){
		return;
	}
	HRESULT hRes( S_OK );
	BOOST_ASSERT( cbViewportData >= 12 );
	//paranoidal checking
	if( !pbViewportData ){
		return;
	}
	if( !cbViewportData ){
		return;
	}
	if( !cTransfered ){
		return;
	}
	if( INVALID_HANDLE_VALUE == hCurPipe ){
		return;
	}
	//initialize IStream if not yet
	if( !pActionPacket->pIStmCompletionBuffer ){
		hRes = ::CreateStreamOnHGlobal( 0, TRUE, &pActionPacket->pIStmCompletionBuffer );
		if( FAILED( hRes ) ){	//all bad
			return;
		}
	}
	//safe to data buffer
	binary_buffer_space::c_bindata_bufer auxBinBuffer;
	BOOL bOk = auxBinBuffer.Attach( pbViewportData, cTransfered );
	if( !bOk ){
		return;
	}

	do {
		// packet isn't init
		if (pActionPacket->nMetadataSize < 0) {
			const size_t packetHeaderSize(sizeof(SPS_ACTIONPACKET_HEADER));
			// not enough bytes for read header of packet. save to temporary stream of packet
			if (auxBinBuffer.get_avail_size() < packetHeaderSize) {
				ULONG written = 0;
				pActionPacket->pIStmCompletionBuffer->Write(auxBinBuffer.get_current_point(), auxBinBuffer.get_avail_size(), &written);
				pActionPacket->nMetadataSize -= written;
				break;
			}
			// restore early cached data from temporary stream of packet
			else if (pActionPacket->nMetadataSize < -1)
			{
				int remainSize = -(pActionPacket->nMetadataSize + 1);
				if (remainSize > 0) {
					std::vector<char> buffer;
					buffer.resize(remainSize);
					ULONG readBytes = 0;
					pActionPacket->pIStmCompletionBuffer->Seek(LARGE_INTEGER(), STREAM_SEEK_SET, nullptr);   // seek to begin
					pActionPacket->pIStmCompletionBuffer->Read(buffer.data(), remainSize, &readBytes);
					pActionPacket->pIStmCompletionBuffer->SetSize(ULARGE_INTEGER());  // set empty content
					pActionPacket->pIStmCompletionBuffer->Seek(LARGE_INTEGER(), STREAM_SEEK_SET, nullptr);   // after SetSize old position is stay
					auxBinBuffer.write(buffer.data(), readBytes);
					auxBinBuffer.write(pbViewportData, cTransfered);
					auxBinBuffer.reset();
					// we will have a big troubles with packet assembling without this...
					cTransfered = auxBinBuffer.get_bufer_size();
				}
			}
			auxBinBuffer.read(static_cast<SPS_ACTIONPACKET_HEADER *>(pActionPacket), packetHeaderSize);
		}
		if (!pActionPacket->nMetadataSize) { //no metadata with message
			//invoke needed handler
			m_pOwner->psPerfomAction(pActionPacket, 0, hCurPipe, 0);
			//prepare envelop for next message
			prepare_envelop_for_next_message(pActionPacket);
		}
		else if (!auxBinBuffer.eof()) { //it mean's we have some porsion of metadata
			ULONG written = 0;
			const void *pbMetaData = auxBinBuffer.get_current_point();
			//write them to stream if need
			int curBytesCount = get_stream_content_size(pActionPacket->pIStmCompletionBuffer);
			int completionSize = pActionPacket->nMetadataSize - curBytesCount;
			BOOST_ASSERT(completionSize > 0);
			//
			int nTail = cTransfered - auxBinBuffer.get_current_pos();
			if (!nTail) {
				break;
			}
			int writtenCount = (nTail >= completionSize) ? completionSize : nTail;
			hRes = pActionPacket->pIStmCompletionBuffer->Write(pbMetaData, writtenCount, &written);
			curBytesCount += written;
			auxBinBuffer.skip(written);
			//
			if (curBytesCount == pActionPacket->nMetadataSize)	//that is alls
			{
				//reposition stream to it's begin
				hRes = pActionPacket->pIStmCompletionBuffer->Seek(LARGE_INTEGER(), STREAM_SEEK_SET, nullptr);
				m_pOwner->psPerfomAction(pActionPacket, 0, hCurPipe, 0);
				//prepare envelop for next message
				prepare_envelop_for_next_message(pActionPacket);
			}
		}
	} while (!auxBinBuffer.eof());
}