#pragma once
#include <stdint.h>
#include <Windows.h>

#define	SPS_DEFAULT_PIPE_NAME						L"\\\\.\\pipe\\ovpipe"

#define SPS_BAD_CONNECTION_RESULT					0xFFFFFFEC
#define SPS_TIMEOUT_CONNECTION_RESULT				0xFFFFFFE6
#define SPS_FATAL_ERROR_RESULT						0xFFFFFFE7
#define SPS_CONNECTION_RESULT_OK					0x0
//
#define SPS_EXIT_CODE_ZERO_PTR						0x3
#define SPS_EXIT_CODE_CONNECTION_FAILED				0xa
//
#define SPS_OV_CONNECT_TIMEOUT						1500

//
#define SPS_OV_DEFAULT_OUTPUT_SIZE					1024
#define SPS_OV_MINIMUM_OUTPUT_SIZE					0x14

//definitions for action format errors
#define AFE_OV_FORMAT_OK							0x0
#define AFE_OV_CLIENTID_MISSING						0x1
#define AFE_OV_RAWDATA_MISSING						0x2
//for client connection
#define OV_CLIENT_SIDE_CONNECTED					0x00
#define OV_CLIENT_SIDE_NO_PIPE						0x02
#define OV_CLIENT_SIDE_NO_CONNECTION				0x03
#define OV_CLIENT_SIDE_CONNECTION_ALREADY_EXISTS	0x04
#define OV_CLIENT_SIDE_CONNECTION_FAILED			0x05
#define OV_CLIENT_SIDE_ILLEGAL_WAIT_TIMEOUT			0x06
#define OV_CLIENT_SIDE_INVALID_PIPE_NAME			0x07
#define OV_CLIENT_SIDE_INVALID_CONNECTION_HANDLE	0x08

////////////definitions for actions////////////
#define CMD_TEST											0x01
//notification from server side about processing completion
#define OV_NOTIFICATION_PROCESSING_COMPLETE					0x02		//S->C
//notification from client side - new video frame is ready (with metadata block probably)
#define OV_NOTIFICATION_NEW_VIDEO_FRAME						0x03		//C->S
//notification from client side - memory mapping file name passing
#define OV_NOTIFICATION_MAPPING_FILE_NAME					0x04		//C->S
//notification from client side - change resolution - we must close and unmap old file and reopen and map new file object with same name
#define OV_NOTIFICATION_CHANGE_RESOLUTION					0x05		//C->S
//notification from client side - indicates the state of moution detector - on/off
#define OV_NOTIFICATION_MOUTION_DETECTOR_STATE				0x06		//C->S
//notification from client side - passing set of moution objects coordinates
#define OV_NOTIFICATION_MOUTION_DETECTOR_METADATA			0x07		//C->S
//notification from server side about moution detector metadata processing completion
#define OV_NOTIFICATION_MOUTION_DETECTOR_METADATA_COMPLETE	0x08		//S->C
//notification from server side about change resolution action complete	//S->C
#define OV_NOTIFICATION_CHANGE_RESOLUTION_EVENT_COMPLETE	0x09
//notification from client side about setting connection				//C->S
#define OV_NOTIFICATION_CLIENT_CONNECTED					0x10
//notification from server side about dual mode switching				//S->C
#define OV_NOTIFICATION_SWITCH_MODE							0x11
//notification from client side about dual mode switching is complete	//C->S
#define OV_NOTIFICATION_SWITCH_MODE_COMPLETE				0x12
/*
This actions added for Direct3D9 resources
*/
#define OV_NOTIFICATION_CLIENT_CONNECTED_XVA				0x13		//C->S
#define OV_NOTIFICATION_CHANGE_RESOLUTION_XVA				0x14		//C->S
#define OV_NOTIFICATION_NEW_VIDEO_FRAME_XVA					0x15		//C->S	
// compressed frame
#define OV_NEW_COMPRESSED_FRAME								22			//C->S

////////////////////////////////////////////////////////
enum en_perfom_action_result{
	par_timeout				= -2,
	par_bad_argument		= -1,
	par_incorrect_result	= 0,
	par_action_not_found	= 1,
	par_all_ok				= 2
};//en_perfom_action_result

struct SPS_ACTIONPACKET_HEADER
{
	uint32_t	nActionId;					//the action id
	uint32_t	dwClientLabel;				//the client's Id
	FILETIME	ftTimeStamp;
	int32_t		nMetadataSize;				//size of action's metadata
};

struct PktDXVA2ChgResol
{
	union {
		HANDLE    hSurface;
		uint32_t  reserved[2];   // допускаем, что нужна совместимость с 32 разрядными серверами
	};
	uint32_t  width;
	uint32_t  height;
};

struct PktDXVA2Connected
{
	uint32_t  adapter;
	uint32_t  decoderID;
	uint32_t  outputFormat;
};

enum PktFrameType : int32_t
{
	pkt_unknown = 0,
	pkt_compressed_video = 1,
	pkt_compressed_audio = 2
};

struct PktFrame
{
	uint16_t		size;
	uint16_t		channel;
	///uint32_t		codecFourcc;
	uint32_t		frameSize;
	PktFrameType	frameType;
	uint32_t		reserved1;
	int64_t			pts;      // relative timestamp
	int64_t			time;     // absolute... (time_t)
	int32_t			timeNsec; // nanoseconds part of time ()
	uint16_t		reserved2;
	uint16_t		extraHeaderSize;
	/*uint8_t		extraHeader[extraHeaderSize];*/
	/*uint8_t		content[frameSize];*/
};

struct PktAudioInfo
{
	uint32_t codecFourcc;
	uint32_t sampleRate;
	uint16_t bitsPerSample;
	uint16_t channels;
};

struct PktVideoInfo
{
	uint32_t codecFourcc;
	uint32_t width;
	uint32_t height;
	uint16_t reserved;
	uint16_t extraDataSize;
	/*uint8_t extraData[extraDataSize];*/
};

struct SPS_ACTIONPACKET : SPS_ACTIONPACKET_HEADER
{
	IStream			*pIStmCompletionBuffer;		//metadata content
	//
	SPS_ACTIONPACKET() :
		SPS_ACTIONPACKET_HEADER(),
		pIStmCompletionBuffer( nullptr )
	{
		nMetadataSize = -1;
	}
};

template< typename _TEnv >
struct OV_IO : OVERLAPPED {
	HANDLE									hCurPipe;
	_TEnv									*pActionInfo;
	unsigned char							*pbCurrentReaded;	//current block of data
	DWORD									dwErrCode;
	//
	OV_IO() : OVERLAPPED(),
			  hCurPipe( INVALID_HANDLE_VALUE ),
			  pActionInfo( 0 ),
			  pbCurrentReaded( 0 ),
			  dwErrCode( 0 )
	{
	}
};//OV_IO


template</*class CondFn, */class PostFn>
bool WaitEmulationForBoolFunctor(HANDLE event, /*CondFn condition,*/ PostFn postFunction, unsigned timeout = INFINITE)
{
	::ResetEvent(event);
	postFunction();
	switch (::WaitForSingleObject(event, timeout)) {
		case WAIT_OBJECT_0:	return true;
		case WAIT_TIMEOUT:	return false;
	}
	API_BREAK_IF_FALSE(false, "Wait pipe event failed");
}

#define DEBUG_LOG_LEVEL_ERROR    (1)
#define DEBUG_LOG_LEVEL_WARNING  (DEBUG_LOG_LEVEL_ERROR  +1)
#define DEBUG_LOG_LEVEL_INFO     (DEBUG_LOG_LEVEL_WARNING+1)
#define DEBUG_LOG_LEVEL_DEBUG    (DEBUG_LOG_LEVEL_INFO   +1)
#define DEBUG_LOG_LEVEL_TRACE    (DEBUG_LOG_LEVEL_DEBUG  +1)

#define DEBUG_LOG_LEVEL(level, x, ...)
#define DEBUG_LOG(x, ...)
#define DEBUG_LOG_ERROR(x, ...)
#define DEBUG_LOG_WARNING(x, ...)
//
#define DEBUG_LOG_HR(hr, x, ...)
#define DEBUG_LOG_FUNC(x, ...)
#define DEBUG_LOG_CURRENT_FUNC