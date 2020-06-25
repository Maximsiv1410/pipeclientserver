#pragma once

//CRT headers
#include <process.h>	//for threads
//common tools
#include "Locker.h"			//for locker
#include "PipeCommon.h"
//STL headers
#include <boost/assert.hpp>
#include <map>
#include <functional>
#include <algorithm>
#include <memory>


namespace ov_pipe_client_manager{

enum ov_client_sent_result{
	csr_all_ok					= 0,
	csr_bad_buffer				= 1,
	csr_bad_count_of_buffer		= 2,
	csr_invalid_pipe_handle		= 3,
	csr_server_unloaded			= 4,
	csr_buffer_overflow			= 5,
	csr_invalid_user_buffer		= 6,
	csr_no_more_memory			= 7,
	csr_unexpected_err			= 8,
	csr_fatal_all_bad			= 9
};//ov_client_sent_result


template< typename _TClient, typename _TMsgDataEnvelop >
class COVPipeClientManager {
private:
	typedef struct _tag_CLIENT_THREAD_ADDITIONAL
	{
		COVPipeClientManager *pThis;
		HANDLE	hPipeHandle;
		HANDLE	hReadingEvent;
		DWORD	dwBufferSize;
		std::shared_ptr<void> pvReserved;
		//
		_tag_CLIENT_THREAD_ADDITIONAL() : pThis(nullptr), hPipeHandle(INVALID_HANDLE_VALUE), hReadingEvent(0), dwBufferSize(0)
		{}
	} CLIENT_THREAD_ADDITIONAL;

	typedef OV_IO< _TMsgDataEnvelop > PIPE_OVERLAPPED;

	struct CLIENT_OVERLAPPED : PIPE_OVERLAPPED
	{
		CLIENT_OVERLAPPED(): pThis(0),
							 dwBufferSize(0) {}
		COVPipeClientManager *	pThis;
		DWORD					dwBufferSize;
	};
	//
protected:
	typedef bool ( _TClient::*OV_PIPEMSG_HANDLER )( _TMsgDataEnvelop*, unsigned long );
	// the map of actions handlers
	typedef std::function< bool( _TClient&, _TMsgDataEnvelop*, unsigned long ) >			CLIENT_MSG_ACTION;
	typedef std::map< DWORD, CLIENT_MSG_ACTION >											CLIENT_ACTIONS_MAP;
	//
	typedef void( _TClient::*OV_SPIT_OUT_HANDLER )( _TMsgDataEnvelop*,
												   unsigned char*,
												   unsigned long,
												   unsigned long,
												   HANDLE,
												   HANDLE );
	typedef std::function< void( _TClient&,
								 _TMsgDataEnvelop*,
								 unsigned char*,
								 unsigned long,
								 unsigned long,
								 HANDLE,
								 HANDLE ) >														CLIENT_PIPE_SPIT_OUT_HANDLER;
	//
	typedef std::function< void( _TClient&, _TMsgDataEnvelop* ) >								CLIENT_ENVELOP_CLEANER;
	typedef void( _TClient::*OV_CLI_ENVELOP_CLEANER )( _TMsgDataEnvelop* );
	//
	typedef void ( _TClient::*OV_CLI_SERVER_LOST )();
	typedef std::function< void( _TClient& ) >													CLIENT_SERVER_LOST;
	//
	struct ThreadData
	{
		HANDLE hReadingThread;
		HANDLE hPipeClientSide;
		DWORD  dwThreadId;
		DWORD  dwOutputBuffSize;

		ThreadData(): hReadingThread(nullptr),
					  hPipeClientSide(INVALID_HANDLE_VALUE),
					  dwThreadId(0),
					  dwOutputBuffSize(SPS_OV_DEFAULT_OUTPUT_SIZE) {}
	};
	//
    typedef locker_space::CObjectLocker< const ThreadData >				CLockerThreadData;
	typedef locker_space::CObjectLocker< ThreadData >					LockerThreadData;
	typedef locker_space::CObjectLocker< CLIENT_ACTIONS_MAP >			LockerActionMap;
	typedef locker_space::CObjectLocker< CLIENT_PIPE_SPIT_OUT_HANDLER >	LockerSpitOutHandler;
	typedef locker_space::CObjectLocker< CLIENT_SERVER_LOST >			LockerServerLost;
private:
	typedef locker_space::CObjectLocker< CLIENT_ENVELOP_CLEANER >		LockerEnvelopCleaner;
protected:
	/*std::wstring					m_wstrDefPipeName;
	locker_space::c_aux_locker		m_auxLockDefPipeName;*/
	HANDLE							m_hEventRead;
	HANDLE							m_hEventWrite;
	long							m_ShutDown;
	locker_space::CInterThreadObject< ThreadData >						m_threadData;
	locker_space::CInterThreadObject< CLIENT_ACTIONS_MAP >				m_action_handlers_map;
	locker_space::CInterThreadObject< CLIENT_PIPE_SPIT_OUT_HANDLER >	m_cli_spit_out_handler;
	//int	m_dwLastError;
private:
	_TClient						* const m_pClientObj;
	CLIENT_OVERLAPPED				m_ovlWrite;
	locker_space::CInterThreadObject< CLIENT_ENVELOP_CLEANER >			m_client_envelop_cleaner;
	locker_space::CInterThreadObject< CLIENT_SERVER_LOST >				m_svr_lost_handler;
protected:
	COVPipeClientManager() : /* m_wstrDefPipeName( SPS_DEFAULT_PIPE_NAME ),*/
							 m_hEventRead( ::CreateEventW( 0, TRUE, FALSE, 0 ) ),
							 m_hEventWrite( ::CreateEventW( 0, TRUE, FALSE, 0 ) ),
							 m_ShutDown( false ),
							 m_cli_spit_out_handler( nullptr ),
							 m_client_envelop_cleaner( nullptr ),
							 m_pClientObj(static_cast< _TClient* >( this ))
	{
		m_ovlWrite.pThis = this;
	}
	/*virtual*/ ~COVPipeClientManager()
	{
		pcDisconnect(false);
		pcWait();
		if( m_hEventRead ){
			::CloseHandle( m_hEventRead );
		}
		if( m_hEventWrite ){
			::CloseHandle( m_hEventWrite );
		}
	}
private:	//avoid making copies
	COVPipeClientManager( COVPipeClientManager const& );
///////////////////////////////////////////// Class interface /////////////////////////////////////////////
protected:
	//connect/disconnect
	int pcConnect( wchar_t const *lpwcPipeName, DWORD dwWaitTimeOut = NMPWAIT_WAIT_FOREVER );
	bool pcDisconnect(bool wait = true);
	//
	bool pcSetOutputBufferSize( DWORD dwSize );
    DWORD pcGetOutputBufferSize() const;
	ov_client_sent_result pcSend( unsigned char const *pbcContent, DWORD dwContentSize, DWORD *pdwSysError = 0 );

protected:
	bool pcLinkActionHandler( unsigned short nActionId, CLIENT_MSG_ACTION aHandler );
	void pcLinkSpitOutHandler( CLIENT_PIPE_SPIT_OUT_HANDLER spitHandler );
	void pcLinkEnvelopCleaner( CLIENT_ENVELOP_CLEANER envCleaner );
	void pcLinkServerLostHandler( CLIENT_SERVER_LOST svrLost );
	//
	void pcResetHandlers();

public/*protected*/:
	en_perfom_action_result psPerfomAction( _TMsgDataEnvelop *pbContent, unsigned long cbContent, HANDLE hCurPipe, HANDLE );
	void pcWait();
private:
	//the client thread proc
	static unsigned int __stdcall OutputThreadProc( void *pvData ) {
		CLIENT_THREAD_ADDITIONAL *pArg = reinterpret_cast<CLIENT_THREAD_ADDITIONAL *>(pvData);
		BOOST_ASSERT(pArg != nullptr);
		BOOST_ASSERT(pArg->pThis != nullptr);
		std::unique_ptr< CLIENT_THREAD_ADDITIONAL > spuAdditional( reinterpret_cast< CLIENT_THREAD_ADDITIONAL* >( pArg ) );
		return pArg->pThis->OutputThreadProc(pArg);
	}
	template<class Fn> static void CompleteIOProcWrapper( DWORD dwErrFlag, DWORD dwBytesTransfered, const CLIENT_OVERLAPPED *pCliOvl, Fn callback ) {
		BOOST_ASSERT(pCliOvl != nullptr);
		BOOST_ASSERT(pCliOvl->pThis != nullptr);
		(pCliOvl->pThis->*callback)(dwErrFlag, dwBytesTransfered, static_cast<const PIPE_OVERLAPPED *>(pCliOvl));
	}
	static void __stdcall CompleteOutputProcWrapper( DWORD dwErrFlag, DWORD dwBytesTransfered, OVERLAPPED *povl ) {
		CompleteIOProcWrapper(dwErrFlag, dwBytesTransfered, static_cast<CLIENT_OVERLAPPED *>(povl), &COVPipeClientManager::CompleteOutputProc);
	}
	static void __stdcall CompleteInputProcWrapper( DWORD dwErrFlag, DWORD dwBytesTransfered, OVERLAPPED *povl ) {
		CompleteIOProcWrapper(dwErrFlag, dwBytesTransfered, static_cast<CLIENT_OVERLAPPED *>(povl), &COVPipeClientManager::CompleteInputProc);
	}

	unsigned int OutputThreadProc( CLIENT_THREAD_ADDITIONAL *pAdditional );
	//the IO complition APC proc
	void CompleteOutputProc( DWORD dwErrFlag, DWORD dwBytesTransfered, const PIPE_OVERLAPPED *pIO );
	void CompleteInputProc( DWORD dwErrFlag, DWORD dwBytesTransfered, const PIPE_OVERLAPPED *pIO );
	void ClearThreadInfo();

	// CRTP
	void SetExtraData(std::shared_ptr<void> &data) {}

	_TClient *GetThis() {
		return static_cast<_TClient *>(this);
	}
};//COVPipeClientManager


template< typename _TClient, typename _TMsgDataEnvelop >
unsigned int
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::OutputThreadProc( CLIENT_THREAD_ADDITIONAL *pAdditional )
{
	//allocate output buffer
	unsigned char *pbOutputBuffer = new unsigned char[ pAdditional->dwBufferSize ];
	std::unique_ptr< unsigned char[] > spuOutputBuffer( pbOutputBuffer );
	ZeroMemory( pbOutputBuffer, pAdditional->dwBufferSize );
	//prepare IO struct
	CLIENT_OVERLAPPED ovio;
	_TMsgDataEnvelop ActionPacket;
	//
	ovio.hCurPipe = pAdditional->hPipeHandle;
	ovio.dwBufferSize = pAdditional->dwBufferSize;
	ovio.pbCurrentReaded = pbOutputBuffer;
	ovio.pActionInfo = &ActionPacket;
	ovio.pThis = this;

	//main loop
	BOOL bRead( FALSE );
	bool error = false;
	DWORD dwReadErr( 0 );
	while( false == m_ShutDown ){
		BOOL bComplete( TRUE ); //the flag for reading
		while( bComplete ){
			bRead = ::ReadFileEx( /*m_hPipeClientSide*/pAdditional->hPipeHandle,
									   spuOutputBuffer.get(),
									   pAdditional->dwBufferSize,
									   static_cast< OVERLAPPED* >( &ovio ),
									   CompleteOutputProcWrapper );
			if( !bRead ){
				dwReadErr = ::GetLastError();
				if( ERROR_PIPE_NOT_CONNECTED == dwReadErr ||
					ERROR_NO_DATA == dwReadErr ||
					ERROR_BROKEN_PIPE == dwReadErr)
				{	//server is over
					//exchange main loop condition
					::InterlockedExchange( reinterpret_cast< long* >( &m_ShutDown ), true );
					bComplete = false;
					error = true;
					break;
				}
			}//!bRead
			DWORD dwWaitRes = ::WaitForSingleObjectEx( m_hEventRead, INFINITE, TRUE );
			switch( dwWaitRes ){
			case WAIT_IO_COMPLETION:{
					DWORD dwErr = ovio.dwErrCode;
					/*int xx( 1 );*/
				}
				break;
			case WAIT_OBJECT_0:{
					bComplete = FALSE;
				}
				break;
			}//switch
		}//while

	}//main while
	CLIENT_ENVELOP_CLEANER callback(*LockerEnvelopCleaner( m_client_envelop_cleaner ));
	if (callback){
		callback( *m_pClientObj, &ActionPacket );
	}
	// disconnect pipe
	::CloseHandle( pAdditional->hPipeHandle );
	pAdditional->hPipeHandle = INVALID_HANDLE_VALUE;
	//
	if (error){	//reading from pipe failed
		CLIENT_SERVER_LOST callback(*LockerServerLost( m_svr_lost_handler ));
		if (callback){
			callback( *m_pClientObj );
		}
	}
	//ClearThreadInfo();
	return 0;
}

template< typename _TClient, typename _TMsgDataEnvelop >
void COVPipeClientManager< _TClient, _TMsgDataEnvelop >::ClearThreadInfo()
{
	LockerThreadData lock( m_threadData );
	if( lock->hReadingThread ){
		::CloseHandle( lock->hReadingThread );
		lock->hReadingThread = nullptr;
		lock->dwThreadId = 0;
	}
}

template< typename _TClient, typename _TMsgDataEnvelop >
void
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::CompleteOutputProc( DWORD dwErrFlag, DWORD dwBytesTransfered, const PIPE_OVERLAPPED *pIO )
{
	LockerSpitOutHandler lock( m_cli_spit_out_handler );
	if ( *lock ) {
		( *lock )( *m_pClientObj,
					pIO->pActionInfo,
					pIO->pbCurrentReaded,
					static_cast<const CLIENT_OVERLAPPED *>(pIO)->dwBufferSize,
					dwBytesTransfered,
					pIO->hCurPipe,
					pIO->hEvent );
	}
}

template< typename _TClient, typename _TMsgDataEnvelop >
void
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::CompleteInputProc( DWORD dwErrFlag, DWORD dwBytesTransfered, const PIPE_OVERLAPPED *pIO )
{
#ifdef _DEBUG
	int nn( 0 );
#endif
}

template< typename _TClient, typename _TMsgDataEnvelop >
void
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::pcResetHandlers()
{
	LockerActionMap lock( m_action_handlers_map );
	lock->clear();
}

template< typename _TClient, typename _TMsgDataEnvelop >
bool
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::pcLinkActionHandler( unsigned short nActionId, CLIENT_MSG_ACTION aHandler )
{
	LockerActionMap lock( m_action_handlers_map );
	CLIENT_ACTIONS_MAP::iterator _itrItem = lock->find( nActionId );
	if( std::end( *lock ) != _itrItem ){
		if (aHandler)
			( *_itrItem ).second = aHandler;
		else
			lock->erase(_itrItem);
		return true;
	}
	bool bIns = ( lock->insert( CLIENT_ACTIONS_MAP::value_type( nActionId, aHandler ) ) ).second;
	return bIns;
}

template< typename _TClient, typename _TMsgDataEnvelop >
void
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::pcLinkSpitOutHandler( CLIENT_PIPE_SPIT_OUT_HANDLER spitHandler )
{
	*LockerSpitOutHandler( m_cli_spit_out_handler ) = spitHandler;
}

template< typename _TClient, typename _TMsgDataEnvelop >
void
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::pcLinkServerLostHandler( CLIENT_SERVER_LOST svrLost )
{
	*LockerServerLost( m_svr_lost_handler ) = svrLost;
}

template< typename _TClient, typename _TMsgDataEnvelop >
void
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::pcLinkEnvelopCleaner( CLIENT_ENVELOP_CLEANER envCleaner )
{
	*LockerEnvelopCleaner( m_client_envelop_cleaner ) = envCleaner;
}

template< typename _TClient, typename _TMsgDataEnvelop >
en_perfom_action_result
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::psPerfomAction( _TMsgDataEnvelop *pbContent, unsigned long cbContent, HANDLE hCurPipe, HANDLE )
{
	CLIENT_MSG_ACTION ActionHandler;
	if( !pbContent ){
		return par_bad_argument;
	}
	{/*lock*/
		LockerActionMap lock( m_action_handlers_map );
		CLIENT_ACTIONS_MAP::iterator _itrHandler = lock->find( pbContent->nActionId );
		if( lock->end() == _itrHandler ){
			return par_action_not_found;
		}
		ActionHandler = ( *_itrHandler ).second;
	}
	bool bResult = ActionHandler( *m_pClientObj, pbContent, cbContent );
	en_perfom_action_result p_res = (true == bResult) ? par_all_ok : par_incorrect_result;
	return p_res;
}
//
template< typename _TClient, typename _TMsgDataEnvelop >
int
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::pcConnect( wchar_t const *lpwcPipeName, DWORD dwWaitTimeOut )
{
	if( !lpwcPipeName ){
		return OV_CLIENT_SIDE_INVALID_PIPE_NAME;
	}
	DWORD dwPipeWaitTimeout = dwWaitTimeOut ? dwWaitTimeOut : NMPWAIT_WAIT_FOREVER;
	/*{
		locker_space::CLocker locker( m_auxLockDefPipeName );
		m_wstrDefPipeName = lpwcPipeName ? lpwcPipeName : SPS_DEFAULT_PIPE_NAME;
	}*/
	DWORD dwThreadExit( 0 );

	LockerThreadData lock( m_threadData );
	if( lock->hReadingThread ){
		BOOL bOk = ::GetExitCodeThread( lock->hReadingThread, &dwThreadExit );
		if( bOk && STILL_ACTIVE == dwThreadExit ){	//thread is working
			return OV_CLIENT_SIDE_CONNECTION_ALREADY_EXISTS;
		}
		::CloseHandle(lock->hPipeClientSide);
		lock->hPipeClientSide = INVALID_HANDLE_VALUE;
		::CloseHandle(lock->hReadingThread);
		lock->hReadingThread = nullptr;
	}
	::ResetEvent( m_hEventRead );
	::ResetEvent( m_hEventWrite );
	//try to connect
	DWORD dwWait = ::WaitNamedPipeW(/* m_wstrDefPipeName.c_str()*/lpwcPipeName, dwPipeWaitTimeout/*NMPWAIT_WAIT_FOREVER*/ );
	if( !dwWait ){
		DWORD dwErr = ::GetLastError();
		if( ERROR_SEM_TIMEOUT == dwErr ){
			return OV_CLIENT_SIDE_ILLEGAL_WAIT_TIMEOUT;
		}
		int nRet = OV_CLIENT_SIDE_NO_PIPE == dwErr ? OV_CLIENT_SIDE_NO_PIPE : OV_CLIENT_SIDE_NO_CONNECTION;
		return nRet;
	}
	//connection
	lock->hPipeClientSide = ::CreateFileW( lpwcPipeName,
											GENERIC_READ | FILE_WRITE_DATA,
											0,
											0,
											OPEN_EXISTING,
											FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
											0 );
	if( INVALID_HANDLE_VALUE == lock->hPipeClientSide ){
		return OV_CLIENT_SIDE_NO_CONNECTION;
	}
	//
	DWORD dwPipeMode( PIPE_READMODE_MESSAGE );
	BOOL bMode = ::SetNamedPipeHandleState( lock->hPipeClientSide, &dwPipeMode, nullptr, nullptr );
	//
	CLIENT_THREAD_ADDITIONAL *pThreadDataAdditional = new CLIENT_THREAD_ADDITIONAL;
	std::unique_ptr< CLIENT_THREAD_ADDITIONAL > spuDataAdditional( pThreadDataAdditional );
	ZeroMemory( pThreadDataAdditional, sizeof( CLIENT_THREAD_ADDITIONAL ) );
	pThreadDataAdditional->pThis = this;
	pThreadDataAdditional->dwBufferSize = lock->dwOutputBuffSize;
	//pThreadDataAdditional->hPipeHandle = m_hPipeClientSide;
	pThreadDataAdditional->pvReserved = nullptr;
	BOOL bOk = ::DuplicateHandle( ::GetCurrentProcess(),
								  lock->hPipeClientSide,
								  ::GetCurrentProcess(),
								  &spuDataAdditional->hPipeHandle,
								  PROCESS_DUP_HANDLE,
								  TRUE,
								  DUPLICATE_SAME_ACCESS );
	if( !bOk ){
		return OV_CLIENT_SIDE_NO_CONNECTION;
	}
	::InterlockedExchange( reinterpret_cast< long* >( &m_ShutDown ), false );
	//
	GetThis()->SetExtraData(spuDataAdditional->pvReserved);

	//run reading thread
	lock->hReadingThread = reinterpret_cast< HANDLE >( ::_beginthreadex(0,
																		0,
																		OutputThreadProc,
																		spuDataAdditional.get(),
																		0,
																		reinterpret_cast<unsigned *>(&lock->dwThreadId) ) );
	if( !lock->hReadingThread ){ //error during the thread creation
		//delete pThreadDataAdditional;
		::InterlockedExchange( reinterpret_cast< long* >( &m_ShutDown ), true );
		return OV_CLIENT_SIDE_CONNECTION_FAILED;
	}
	spuDataAdditional.release();
	return OV_CLIENT_SIDE_CONNECTED;
}

template< typename _TClient, typename _TMsgDataEnvelop >
bool
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::pcDisconnect(bool wait)
{
	{/*lock*/
		LockerThreadData lock( m_threadData );
		// pipe not connected
		if( INVALID_HANDLE_VALUE == lock->hPipeClientSide ){
			return false;
		}
		// disconnect pipe
		if( INVALID_HANDLE_VALUE != lock->hPipeClientSide ){
			DWORD dwCancel = ::CancelIo( lock->hPipeClientSide );
			::CloseHandle( lock->hPipeClientSide );
			lock->hPipeClientSide = INVALID_HANDLE_VALUE;
		}
		//exchange main loop condition
		::InterlockedExchange( reinterpret_cast< long* >( &m_ShutDown ), true );
		//event to signaled state
		::SetEvent( m_hEventRead );
		::SetEvent( m_hEventWrite );
	}
	if (wait)
		pcWait();
	//
	return true;
}

template< typename _TClient, typename _TMsgDataEnvelop >
void COVPipeClientManager< _TClient, _TMsgDataEnvelop >::pcWait()
{
	//wait for end of reading thread
	HANDLE hThread;
	{/*lock*/
		LockerThreadData lock( m_threadData );
		if( !lock->hReadingThread || GetCurrentThreadId() == lock->dwThreadId ) {
			return;
		}
		hThread = lock->hReadingThread;
	}
	BOOST_VERIFY(WAIT_OBJECT_0 == ::WaitForSingleObject( hThread, INFINITE ));
	ClearThreadInfo();
}


template< typename _TClient, typename _TMsgDataEnvelop >
bool
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::pcSetOutputBufferSize( DWORD dwSize )
{
	//check output thread state
	LockerThreadData lock( m_threadData );
	DWORD dwThreadExit( 0 );
	if( lock->hReadingThread ){
		BOOL bOk = ::GetExitCodeThread( lock->hReadingThread, &dwThreadExit );
		if( bOk && STILL_ACTIVE == dwThreadExit ){	//thread is working
			return false;
		}
	}
	if( dwSize < SPS_OV_MINIMUM_OUTPUT_SIZE ){
		return false;
	}
	lock->dwOutputBuffSize = dwSize;
	return true;
}


template< typename _TClient, typename _TMsgDataEnvelop >
DWORD COVPipeClientManager< _TClient, _TMsgDataEnvelop >::pcGetOutputBufferSize() const
{
    return CLockerThreadData( m_threadData )->dwOutputBuffSize;
}


template< typename _TClient, typename _TMsgDataEnvelop >
ov_client_sent_result
COVPipeClientManager< _TClient, _TMsgDataEnvelop >::pcSend( unsigned char const *pbcContent, DWORD dwContentSize, DWORD *pdwSysError )
{
	if( !pbcContent ){
		return csr_bad_buffer;
	}
	if( !dwContentSize ){
		return csr_bad_count_of_buffer;
	}
	ov_client_sent_result sent_result( csr_all_ok );
	DWORD dwFaultReason(ERROR_SUCCESS);
	{/*lock*/
		LockerThreadData lock( m_threadData );
		if( INVALID_HANDLE_VALUE == lock->hPipeClientSide ){
			return csr_invalid_pipe_handle;
		}
		ZeroMemory( &m_ovlWrite, sizeof( OVERLAPPED ) );
		DWORD dwRetErr( 0 );
		BOOL bCancelOk( FALSE );
		BOOL bWritten( TRUE );
		//bWritten = ::WriteFileEx( lock->hPipeClientSide, pbcContent, dwContentSize, &m_ovlWrite, CompleteInputProcWrapper);
		bWritten = ::WriteFile( lock->hPipeClientSide, pbcContent, dwContentSize, nullptr, &m_ovlWrite );
		// check error
		if( !bWritten )
		{
			dwFaultReason = ::GetLastError();
			switch( dwFaultReason )
			{
				case ERROR_IO_PENDING:
					break;  // it's ok
				case ERROR_INVALID_USER_BUFFER:{
					sent_result = csr_invalid_user_buffer;
					::CancelIo( lock->hPipeClientSide );
				}
				break;
				case ERROR_NOT_ENOUGH_MEMORY:{
					sent_result = csr_no_more_memory;
					::CancelIo( lock->hPipeClientSide );
				}
				break;
				case ERROR_PIPE_NOT_CONNECTED:{
					sent_result = csr_server_unloaded;
				}
				break;
				default:
					sent_result = csr_unexpected_err;
			}//switch
		}//( !bWritten )
	}
	if( pdwSysError ){
		*pdwSysError = dwFaultReason;
	}
	return sent_result;
}

}//ov_pipe_client_manager
