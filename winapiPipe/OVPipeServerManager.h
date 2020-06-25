#pragma once
//#pragma warning( disable : 4309 )
//#pragma warning( disable : 4083 )
#pragma warning( disable : 4005 )

//CRT headers
#include <process.h>						//for threads
#include <assert.h>
//common tools
#include "Locker.h"			//for locker
//
#include "PipeCommon.h"
//STL headers
#include <boost/assert.hpp>
#include <boost/thread/tss.hpp>
#include <map>
#include <string>
#include <vector>
#include <exception>
#include <functional>
#include <algorithm>
#include <memory>

//class iov_message_deserializer;
#pragma push_macro("DEBUG_LOG_PREFIX")
#define DEBUG_LOG_PREFIX "(Pipe server) "


namespace ov_pipe_svr_manager
{

template< typename _TOwner, typename _TMsgDataEnvelop >
class COVPipeServerManager{
private:
	//
	typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, BOOL (WINAPI *)(HANDLE)> HandlePtr;

	class c_wait_and_free_handle : public std::unary_function< HandlePtr&, void >{
		public:
			result_type operator()( argument_type at )
			{
				if( at )
				{
					auto res = ::WaitForSingleObject(at.get(), INFINITE);
					if (res != WAIT_OBJECT_0) {
						DEBUG_LOG_ERROR("Wait for thread failed: %u", res);
					}
					//::CloseHandle( at );
					//at = 0;
				}
			}

	};//c_wait_and_free_handle
	//
	struct TWOHANDLES{
		const HANDLE hEventOutput;
		const HANDLE hEventInput;
		//;
		TWOHANDLES() : hEventOutput( ::CreateEventW( 0, TRUE, FALSE, 0 ) ),
					   hEventInput( ::CreateEventW( 0, TRUE, FALSE, 0 ) )
		{}
		~TWOHANDLES() {
			BOOST_VERIFY(::CloseHandle(hEventInput));
			BOOST_VERIFY(::CloseHandle(hEventOutput));
		}
		void SignalEvents() {
			BOOST_VERIFY(::SetEvent( hEventOutput ));
			BOOST_VERIFY(::SetEvent( hEventInput ));
		}
	};
	//
	typedef bool ( _TOwner::*OV_PIPEMSG_HANDLER )( _TMsgDataEnvelop*, unsigned long, HANDLE, HANDLE );
	//
	struct THRDSVRCONTEXT
	{
		THRDSVRCONTEXT(COVPipeServerManager *parent, HANDLE pipe):
			pThis(parent),
			hCurNamedPipe(pipe),
			bShutDown(true)
		{
		}
		~THRDSVRCONTEXT() {
			if (hCurNamedPipe) {
				//disconnect from pipe
				DEBUG_LOG("Disconnect and close pipe: %u", ::GetCurrentThreadId());
				BOOST_VERIFY(::FlushFileBuffers(hCurNamedPipe));
				BOOST_VERIFY(::DisconnectNamedPipe(hCurNamedPipe) || ::GetLastError() == ERROR_PIPE_NOT_CONNECTED);
				BOOST_VERIFY(::CloseHandle(hCurNamedPipe));
			}
		}
		void Shutdown()
		{
			BOOL bCancel = ::CancelIo( hCurNamedPipe );
			twoEvents.SignalEvents();
			bShutDown = false;
		}
		bool IsRunning() const {
			return bShutDown;
		}
		HANDLE GetPipe() const {
			return hCurNamedPipe;
		}
		HANDLE GetOutputEvent() const {
			return twoEvents.hEventOutput;
		}
		COVPipeServerManager *GetParent() const {
			return pThis;
		}

	private:
		COVPipeServerManager* pThis;
		HANDLE	hCurNamedPipe;
		bool	bShutDown;
		TWOHANDLES twoEvents;
		//void	*pvAdditional;
	};

	struct PIPE_THREAD_INFO
	{
		PIPE_THREAD_INFO(HANDLE h, THRDSVRCONTEXT *ctx) :
			thread(h, ::CloseHandle),
			context(ctx)
		{
		}
		PIPE_THREAD_INFO(PIPE_THREAD_INFO &&obj) :
			thread(INVALID_HANDLE_VALUE, ::CloseHandle),
			context(nullptr)
		{
			thread.swap(obj.thread);
			std::swap(context, obj.context);
		}
		~PIPE_THREAD_INFO()
		{
		}
		HandlePtr       thread;
		THRDSVRCONTEXT *context;
	};

	struct PIPE_SERVER_OVERLAPPED : OV_IO< _TMsgDataEnvelop >
	{
		PIPE_SERVER_OVERLAPPED() :
			OV_IO< _TMsgDataEnvelop >(), pThis(nullptr) {}
		COVPipeServerManager *pThis;
	};
	//
	typedef std::map< DWORD, PIPE_THREAD_INFO >																	SVR_THRD_POOL_MAP;

protected:
	//
	typedef OV_PIPEMSG_HANDLER																					SVR_MSG_ACTION;

	typedef std::map< DWORD, SVR_MSG_ACTION >																	SVR_ACTIONS_MAP;
	//
	typedef void( _TOwner::*OV_SPIT_OUT_HANDLER )( _TMsgDataEnvelop*,
												   unsigned char*,
												   unsigned long,
												   unsigned long,
												   HANDLE,
												   HANDLE );
	typedef OV_SPIT_OUT_HANDLER																					SVR_PIPE_SPIT_OUT_HANDLER;
	//
	typedef void( _TOwner::*OV_ENVELOP_CLEANER )( _TMsgDataEnvelop* );
	typedef OV_ENVELOP_CLEANER																					SVR_PIPE_ENVELOP_CLEANER;
	//
	typedef void( _TOwner::*OV_ON_CLIENT_DISCONNECT )( HANDLE );
	typedef OV_ON_CLIENT_DISCONNECT																				SVR_CLIENT_DISCONNECT;

protected:
	_TOwner									*m_pOwnerObj;
	//
	SVR_THRD_POOL_MAP						m_pool_map;
	//
	locker_space::c_aux_locker 				m_auxLockObject;			//for lock operations
	locker_space::c_aux_locker				m_auxLockPerformHandler;	//
	locker_space::c_aux_locker				m_auxLockSpitOut;
	locker_space::c_aux_locker				m_auxLockCleaner;
	locker_space::c_aux_locker				m_auxLockOnDisconnect;
	boost::thread_specific_ptr<OVERLAPPED>	m_writeOverlapped;

private:
	DWORD									m_dwOutputSize;
	//
protected:
	SVR_ACTIONS_MAP							m_svr_actions_map;
	std::wstring							m_wstrPipeId;				//pipe's name as \\\\.\\pipe\\ovpipe
	SVR_PIPE_SPIT_OUT_HANDLER				m_svr_spit_out_handler;
	//
	SVR_PIPE_ENVELOP_CLEANER				m_svr_envelop_cleaner;
	//
	SVR_CLIENT_DISCONNECT					m_svr_client_disconnect;
protected:
	COVPipeServerManager() : m_wstrPipeId( SPS_DEFAULT_PIPE_NAME ),
							 m_dwOutputSize( SPS_OV_DEFAULT_OUTPUT_SIZE ),
							 m_auxLockPerformHandler(),
							 m_auxLockObject(),
							 m_svr_spit_out_handler(nullptr),
							 m_svr_envelop_cleaner(nullptr),
							 m_svr_client_disconnect(nullptr)
	{
		m_pOwnerObj = static_cast< _TOwner* >( this );
	}
	/*virtual*/ ~COVPipeServerManager()
	{
		__destroy();
	}
private:	//avoid making copies
	COVPipeServerManager( COVPipeServerManager const& );
	COVPipeServerManager& operator=( COVPipeServerManager const& );

	//////////////////////////////// class interface ////////////////////////////////
protected:
	// link msg handler
	bool psLinkAction( unsigned short nAction, OV_PIPEMSG_HANDLER msgHandler );
	void psSetSpitOutHandler( OV_SPIT_OUT_HANDLER spitoutHandler );
	void psLinkEnvelopCleaner( OV_ENVELOP_CLEANER envelopCleaner );
	void psLinkOnDisconnectHandler( OV_ON_CLIENT_DISCONNECT ondisconnectCBF );
	// invoke action
public:
	en_perfom_action_result
	psPerfomAction( _TMsgDataEnvelop* pActionInfo, unsigned long dwActionFormatErr, HANDLE hPipe, HANDLE hSyncEvent ) /*const*/;
	//
protected:
	std::size_t get_threads_count();
protected:
	// set pipe's name
	void psSetPipeId( wchar_t const *lpwcszPipeName );
	//determine size of output buffer
	bool psSetOutputBufferSize( DWORD dwSizeInBytes );
    DWORD psGetOutputBufferSize() const;
	//
	int psRun(PSECURITY_DESCRIPTOR sec_desc = nullptr);
	bool psStop( unsigned int nId );
	size_t psStopOverall();
	//
	bool SendAnswer( unsigned char *pbAnswer, DWORD dwAnswer, HANDLE hCurPipe, HANDLE hSyncEvent, OVERLAPPED *ovl = nullptr );
	bool disconnect_pipe(HANDLE namedPipe);
	//////////////////////////////// helpers and aux functions ////////////////////////////////
private:
	int check_connection_result( HANDLE hPipe, OVERLAPPED *pOvl );
private:
	//the server thread proc
	static unsigned int __stdcall ServerThreadProc(void *pvData) {
		const unsigned thread_id( ::GetCurrentThreadId() );
		DEBUG_LOG_LEVEL(DEBUG_LOG_LEVEL_DEBUG, "Start thread: %u", thread_id);
		BOOST_ASSERT(pvData);
		//preparing context
		std::unique_ptr< THRDSVRCONTEXT > pSvrContext(static_cast<THRDSVRCONTEXT *>(pvData));
		auto result = pSvrContext->GetParent()->ServerThreadProc(*pSvrContext);
		pSvrContext.reset();
		DEBUG_LOG_LEVEL(DEBUG_LOG_LEVEL_DEBUG, "End thread: %u", thread_id);
		return result;
	}

	unsigned int ServerThreadProc( const THRDSVRCONTEXT &context );
	//the IO complition APC proc
	void CompleteIOProc( DWORD dwErrFlag, DWORD dwBytesTransfered, const PIPE_SERVER_OVERLAPPED &povl );
	//
	static void WINAPI CompleteIOProc(DWORD dwErrFlag, DWORD dwBytesTransfered, OVERLAPPED *povl) {
		BOOST_ASSERT(povl);
		auto context = static_cast<PIPE_SERVER_OVERLAPPED *>(povl);
		context->pThis->CompleteIOProc(dwErrFlag, dwBytesTransfered, *context);
	}
	static void WINAPI WriteCompleteIOProc(DWORD dwErrFlag, DWORD dwBytesTransfered, OVERLAPPED *povl) {
		// no impls
	}
	void __destroy();
};//COVPipeServerManager


template< typename _TOwner, typename _TMsgDataEnvelop >
bool
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::SendAnswer( unsigned char *pbAnswer, DWORD dwAnswer, HANDLE hCurPipe, HANDLE hSyncEvent, OVERLAPPED *ovl )
{
	if( INVALID_HANDLE_VALUE == hCurPipe ){
		return false;
	}
	/*if( !hSyncEvent ){
		return false;
	}*/
	if( !pbAnswer ){
		return false;
	}
	if( !dwAnswer ){
		return false;
	}
	//BOOL bWrite = ::WriteFileEx( hCurPipe, pbAnswer, dwAnswer, &ovl, m_svr_write_completeio_proc_thunk.CallbackImpl );
	DWORD dwHasWritten( 0 );
	DWORD dwErrFlag( 0 );

	if (!ovl) {
		// (12.03.2019)
		// WARNING!
		//-------------------------------------------------------------------------------------------------------------
		// According to https://docs.microsoft.com/en-us/windows/desktop/api/fileapi/nf-fileapi-writefile
		// WriteFile should use OVERLAPPED struct for pipe created with FILE_FLAG_OVERLAPPED flag.
		// Otherwise application potentialy may be crashed in random places of code and no one will find this error!
		// We spent 3 days to find it...
		if (!m_writeOverlapped.get()) {
			m_writeOverlapped.reset(new OVERLAPPED());
		}
		ovl = m_writeOverlapped.get();
	}
	BOOL bWrite = ::WriteFile( hCurPipe, pbAnswer, dwAnswer, ovl ? nullptr : &dwHasWritten, ovl );
	if( !bWrite ){
		dwErrFlag = ::GetLastError();
		if (dwErrFlag != ERROR_IO_PENDING)
		{
			DEBUG_LOG_HR(dwErrFlag, "Write to pipe error");
			//
			if ((ERROR_INVALID_USER_BUFFER == dwErrFlag) || (ERROR_NOT_ENOUGH_MEMORY == dwErrFlag)) {
				//BOOL bIOMarkCanceling = ::CancelSynchronousIo( ::GetCurrentThread() );
				// ???
				//BOOL bIOMarkCanceling = ::CancelIo(hCurPipe);
			}
			return false;
		}
	}
	return true;
}

template< typename _TOwner, typename _TMsgDataEnvelop >
int
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::check_connection_result( HANDLE hPipe, OVERLAPPED *pOvl )
{
	if( INVALID_HANDLE_VALUE == hPipe ){
		return SPS_BAD_CONNECTION_RESULT;
	}
	if( !hPipe ){
		return SPS_BAD_CONNECTION_RESULT;
	}
	DWORD dwErr( 0 );
	//
	BOOL bConnectClient = ::ConnectNamedPipe( hPipe, pOvl );
	if( !bConnectClient ){
		dwErr = ::GetLastError();
		if( ERROR_PIPE_CONNECTED != dwErr ){
			DWORD dwWaitRes = ::WaitForSingleObject( pOvl->hEvent, SPS_OV_CONNECT_TIMEOUT );
			switch( dwWaitRes ){
				case WAIT_TIMEOUT:{
					::ResetEvent( pOvl->hEvent );
					return SPS_TIMEOUT_CONNECTION_RESULT;
				}
				break;
				case WAIT_FAILED:{
					::ResetEvent( pOvl->hEvent );
					return SPS_EXIT_CODE_CONNECTION_FAILED;
				}
				break;
				case WAIT_OBJECT_0:{
					::ResetEvent( pOvl->hEvent );
					return SPS_CONNECTION_RESULT_OK;
				}
			}//switch
		}//ERROR_PIPE_CONNECTED
	}
	::ResetEvent( pOvl->hEvent );
	return SPS_CONNECTION_RESULT_OK;
}

template< typename _TOwner, typename _TMsgDataEnvelop >
void
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::CompleteIOProc( DWORD dwErrFlag, DWORD dwBytesTransfered, const PIPE_SERVER_OVERLAPPED &pIO )
{
	SVR_PIPE_SPIT_OUT_HANDLER complete_io_proc;
	{/*lock*/
		locker_space::CLocker locker( m_auxLockSpitOut );
		if( !m_svr_spit_out_handler ){	//if forget to set handler
			return;
		}
		complete_io_proc = m_svr_spit_out_handler;
	}
	//run handler
	(m_pOwnerObj->*complete_io_proc)(
							pIO.pActionInfo,
							pIO.pbCurrentReaded,
							m_dwOutputSize,
							dwBytesTransfered,
							pIO.hCurPipe,
							pIO.hEvent );
}

template< typename _TOwner, typename _TMsgDataEnvelop >
bool
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::disconnect_pipe(HANDLE namedPipe)
{
	DEBUG_LOG("Disconnect pipe: 0x%p", namedPipe);
	BOOL bDisconnect = ::DisconnectNamedPipe( namedPipe );
	//
	SVR_CLIENT_DISCONNECT disconnect_callback;
	{/*lock*/
		locker_space::CLocker locker( m_auxLockOnDisconnect );
		disconnect_callback = m_svr_client_disconnect;
	}
	if( disconnect_callback ) {
		(m_pOwnerObj->*disconnect_callback)( namedPipe );
	}
	return !!bDisconnect;
}

template< typename _TOwner, typename _TMsgDataEnvelop >
unsigned int
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::ServerThreadProc(const THRDSVRCONTEXT &pSvrContext)
{
	const unsigned thread_id( ::GetCurrentThreadId() );
	const auto pipeBufferSize( m_dwOutputSize );
	//allocate output buffer
	std::unique_ptr< unsigned char[] > pbOutputBuffer( new unsigned char[ pipeBufferSize ] );
	ZeroMemory( pbOutputBuffer.get(), pipeBufferSize );

	//for overlapped connection
	OVERLAPPED ovlConnect;
	ZeroMemory( &ovlConnect, sizeof( OVERLAPPED ) );
	HandlePtr hConnEvent(::CreateEventW( 0, TRUE, FALSE, 0 ), ::CloseHandle);
	ovlConnect.hEvent = hConnEvent.get();
	//
	//initialize info struct
	PIPE_SERVER_OVERLAPPED io;
	_TMsgDataEnvelop ActionPacket;
	//
	io.pThis = this;
	io.pActionInfo = &ActionPacket;
	io.pbCurrentReaded = pbOutputBuffer.get();
	io.hCurPipe = pSvrContext.GetPipe();
	while( pSvrContext.IsRunning() ){
		int nConnection = check_connection_result( io.hCurPipe, &ovlConnect );
		if( SPS_CONNECTION_RESULT_OK/*SPS_TIMEOUT_CONNECTION_RESULT*/ != nConnection ){
			DEBUG_LOG_LEVEL(DEBUG_LOG_LEVEL_TRACE, "Await connection (thread = %u)...", thread_id);
			continue;
		}
		//if we are here, we are ready for reading from pipe
		BOOL bComplete( TRUE ); //the flag for reading
		DWORD dwErr( 0 );
		while( bComplete ){
			BOOL bRead = ::ReadFileEx( io.hCurPipe, pbOutputBuffer.get(), pipeBufferSize, static_cast< OVERLAPPED* >( &io ), &COVPipeServerManager::CompleteIOProc );
			if( !bRead ){
				dwErr = ::GetLastError();
				if( ERROR_BROKEN_PIPE == dwErr ||
					ERROR_PIPE_NOT_CONNECTED == dwErr ||
					ERROR_NO_DATA == dwErr )
				{
					bComplete = FALSE;
					DEBUG_LOG_LEVEL(DEBUG_LOG_LEVEL_DEBUG, "Stop read from pipe. Pipe is closed (%u)", dwErr);
					disconnect_pipe(io.hCurPipe);
				}
				else {
					DEBUG_LOG_HR(dwErr, "Read from pipe error");
				}
				continue;
			}
			DWORD dwWaitRes = ::WaitForSingleObjectEx( pSvrContext.GetOutputEvent(), INFINITE, TRUE );
			switch( dwWaitRes ){
			case WAIT_IO_COMPLETION:{
					dwErr = io.dwErrCode;
					/*int xx( 1 );*/
				}
				break;
			case WAIT_OBJECT_0:{
					disconnect_pipe(io.hCurPipe);
					bComplete = FALSE;
				}
				break;
			case WAIT_TIMEOUT:{
				disconnect_pipe(io.hCurPipe);
				bComplete = FALSE;
				continue;
				}
			}//switch
		}//while
	}//main loop
	SVR_PIPE_ENVELOP_CLEANER env_cleaner;
	{/*lock*/
		locker_space::CLocker locker( m_auxLockCleaner );
		env_cleaner = m_svr_envelop_cleaner;
	}
	if (env_cleaner) {
		(m_pOwnerObj->*env_cleaner)(&ActionPacket );
	}
	{/*lock*/
		locker_space::CLocker locker(m_auxLockObject);
		m_pool_map.erase(thread_id);
	}
	return 0;
}

template< typename _TOwner, typename _TMsgDataEnvelop >
bool
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::psSetOutputBufferSize( DWORD dwSizeInBytes )
{
	if( dwSizeInBytes < SPS_OV_MINIMUM_OUTPUT_SIZE ){
		DEBUG_LOG_ERROR("Set output buffer required size >= %d", SPS_OV_MINIMUM_OUTPUT_SIZE);
		return false;
	}
	{/*lock*/
		locker_space::CLocker locker(m_auxLockObject);
		if (false == m_pool_map.empty()) {	//thread pool not empty
			DEBUG_LOG_ERROR("Buffer size changing is denied since server has at least one thread!");
			return false;
		}
	}
	DEBUG_LOG("Set output buffer size: %u", dwSizeInBytes);
	m_dwOutputSize = dwSizeInBytes;
	return true;
}

template< typename _TClient, typename _TMsgDataEnvelop >
DWORD COVPipeServerManager< _TClient, _TMsgDataEnvelop >::psGetOutputBufferSize() const
{
    return m_dwOutputSize;
}

template< typename _TOwner, typename _TMsgDataEnvelop >
bool
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::psLinkAction( unsigned short nAction, OV_PIPEMSG_HANDLER msgHandler )
{
	bool bRet( false );
	locker_space::CLocker locker( m_auxLockPerformHandler );
	SVR_ACTIONS_MAP::iterator _itrHandler = m_svr_actions_map.find( nAction );
	if( std::end( m_svr_actions_map ) != _itrHandler ){
		if (msgHandler) {
			( *_itrHandler ).second = std::move(msgHandler);
		} else {
			m_svr_actions_map.erase(_itrHandler);
		}
		return true;
	}
	bRet = m_svr_actions_map.insert( SVR_ACTIONS_MAP::value_type( nAction, std::move(msgHandler) ) ).second;
	return bRet;
}

template< typename _TOwner, typename _TMsgDataEnvelop >
void
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::psSetSpitOutHandler( OV_SPIT_OUT_HANDLER spitoutHandler )
{
	locker_space::CLocker locker( m_auxLockSpitOut );
	m_svr_spit_out_handler = spitoutHandler;
}

template< typename _TOwner, typename _TMsgDataEnvelop >
void
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::psLinkEnvelopCleaner( OV_ENVELOP_CLEANER envelopCleaner )
{
	locker_space::CLocker locker( m_auxLockCleaner );
	m_svr_envelop_cleaner = envelopCleaner;
}

template< typename _TOwner, typename _TMsgDataEnvelop >
void
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::psLinkOnDisconnectHandler( OV_ON_CLIENT_DISCONNECT ondisconnectCBF )
{
	locker_space::CLocker locker( m_auxLockOnDisconnect );
	m_svr_client_disconnect = ondisconnectCBF;
}

template< typename _TOwner, typename _TMsgDataEnvelop >
en_perfom_action_result
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::psPerfomAction( _TMsgDataEnvelop* pActionInfo, unsigned long dwActionFormatErr, HANDLE hPipe, HANDLE hSyncEvent )/*const*/
{
	SVR_MSG_ACTION actionHandler;
	{/*lock*/
		locker_space::CLocker locker( m_auxLockPerformHandler );
		SVR_ACTIONS_MAP::iterator  _itrAction = m_svr_actions_map.find( pActionInfo->nActionId );
		if( m_svr_actions_map.end() == _itrAction ){
			return par_action_not_found;
		}
		actionHandler = ( *_itrAction ).second;
	}
	//
	bool bAction = (m_pOwnerObj->*actionHandler)( pActionInfo, dwActionFormatErr, hPipe, hSyncEvent );
	en_perfom_action_result retval = true == bAction ? par_all_ok : par_incorrect_result;
	//
	return retval;
}

template< typename _TOwner, typename _TMsgDataEnvelop >
void
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::psSetPipeId( wchar_t const *lpwcszPipeName )
{
	m_wstrPipeId = lpwcszPipeName ? lpwcszPipeName : SPS_DEFAULT_PIPE_NAME;
	DEBUG_LOG("Set pipe id = %s", CW2A(m_wstrPipeId.c_str()).m_psz);
}

template< typename _TOwner, typename _TMsgDataEnvelop >
void
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::__destroy()
{
	psStopOverall();
}

template< typename _TOwner, typename _TMsgDataEnvelop >
size_t
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::get_threads_count()
{
	//guard the map
	locker_space::CLocker locker( m_auxLockObject );
	return m_pool_map.size();
}


template< typename _TOwner, typename _TMsgDataEnvelop >
bool
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::psStop( unsigned int nId )
{
	if( !nId ){
		return false;
	}
	DEBUG_LOG_LEVEL(DEBUG_LOG_LEVEL_DEBUG, "Stop server thread: %d", nId);

	locker_space::CLocker locker(m_auxLockObject);
	//find needed item
	SVR_THRD_POOL_MAP::iterator _itrItem = m_pool_map.find(nId);
	if (std::end(m_pool_map) == _itrItem) {
		DEBUG_LOG_ERROR("No such thread: %d", nId);
		return false;
	}
	if (nullptr == _itrItem->second.context) {
		DEBUG_LOG_ERROR("Thread doesn't have context: %d", nId);
		return false;
	}
	if (!_itrItem->second.context->IsRunning()) {	//paranoidal!!
		DEBUG_LOG_WARNING("Thread already is stopped: %d", nId);
		return false;
	}
	DEBUG_LOG("Start shutdown server thread: %d", nId);
	// трюк с синхронизацией, чтобы корректно всё выгрузить
	// (lock cleaner используется при выгрузке потока)
	locker_space::CLocker pipeThreadSync(m_auxLockCleaner);
	//
	//stop thread loop
	_itrItem->second.context->Shutdown();
	_itrItem->second.context = nullptr;
	//m_pool_map.erase(_itrItem);

	DEBUG_LOG_LEVEL(DEBUG_LOG_LEVEL_DEBUG, "Stop server thread = %d END", nId);
	return true;
}

template< typename _TOwner, typename _TMsgDataEnvelop >
size_t
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::psStopOverall()
{
	std::vector< HandlePtr > defered_handles;
	size_t connections = 0;
	{/*lock*/
		locker_space::CLocker locker(m_auxLockObject);
		if (m_pool_map.empty()) {
			return 0;
		}
		connections = m_pool_map.size();
		DEBUG_LOG("Stop overall for %s", CW2A(m_wstrPipeId.c_str()).m_psz);
		//
		defered_handles.reserve(m_pool_map.size());
		std::for_each(m_pool_map.begin(), m_pool_map.end(), [&defered_handles](SVR_THRD_POOL_MAP::value_type &val)
		{
			if (val.second.context) {
				DEBUG_LOG("Enable shutdown: %p", val.second.context);
				val.second.context->Shutdown();
				val.second.context = nullptr;
				defered_handles.push_back(std::move(val.second.thread));
			}
		});
		m_pool_map.clear();
	}
	// wait threads
	std::for_each( defered_handles.begin(), defered_handles.end(), c_wait_and_free_handle() );
	defered_handles.clear();
	//
	return connections;
}

template< typename _TOwner, typename _TMsgDataEnvelop >
int
COVPipeServerManager< _TOwner, _TMsgDataEnvelop >::psRun(PSECURITY_DESCRIPTOR sd)
{
	{/*lock*/
		locker_space::CLocker locker( m_auxLockObject );
		if( m_pool_map.size() == PIPE_UNLIMITED_INSTANCES ){
			DEBUG_LOG_ERROR("Reach connections limit: %d", static_cast<int>(m_pool_map.size()));
			return -2;
		}
	}
	if( true == m_wstrPipeId.empty() ){
		DEBUG_LOG_ERROR("Empty pipe name!");
		return -1;
	}
	// Set security descriptor in security attributes
	SECURITY_ATTRIBUTES sa = {0};
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = FALSE;
	sa.lpSecurityDescriptor = sd;

	//turn on loop flag
	//::InterlockedExchange( reinterpret_cast< long* >( &m_bShutDown ), true );
	//create instance of pipe
	HANDLE hMainPipe = ::CreateNamedPipeW( m_wstrPipeId.c_str(),
										   PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
										   /*PIPE_TYPE_BYTE*/PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE/*PIPE_READMODE_BYTE*/ /*| PIPE_NOWAIT*/,
										   PIPE_UNLIMITED_INSTANCES,
										   0,
										   0,
										   INFINITE,
										   sd ? &sa : nullptr );
	if( INVALID_HANDLE_VALUE == hMainPipe ){
		DWORD error = ::GetLastError();
		DEBUG_LOG_HR(error, "Create named pipe error");
		return -1;
	}
	//
	THRDSVRCONTEXT *pCurContext = new THRDSVRCONTEXT(this, hMainPipe);
	UINT nCurThreadId( 0 );
	//create server's thread
	HANDLE hNewThread = reinterpret_cast< HANDLE >( ::_beginthreadex( 0,
																	  0,
																	  &COVPipeServerManager::ServerThreadProc,
																	  pCurContext,
																	  CREATE_SUSPENDED,
																	  &nCurThreadId ) );
	if( !hNewThread ){
		DEBUG_LOG_ERROR("Start thread for pipe connection error");
		delete pCurContext;
		return -1;
	}
	{/*lock*/
		locker_space::CLocker locker( m_auxLockObject );
		m_pool_map.insert( SVR_THRD_POOL_MAP::value_type( nCurThreadId, PIPE_THREAD_INFO(hNewThread, pCurContext) ) );
	}
	DWORD dwResume = ::ResumeThread( hNewThread );
	return nCurThreadId/*nVacantIndex*/;
}

}//ov_pipe_svr_manager

#pragma pop_macro("DEBUG_LOG_PREFIX")