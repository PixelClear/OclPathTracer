/*
		2011 Takahiro Harada
*/

#ifndef ADL_ERROR_H
#define ADL_ERROR_H

#if defined(ADL_DUMP_DX11_ERROR)
	#include <windows.h>
#endif
#ifdef _DEBUG
	#include <assert.h>
	#include <stdarg.h>
	#include <stdio.h>
#endif

#if defined(ADL_USE_EXTERNAL_ERROR)
#include <Tahoe/Math/Error.h>
#include <Tahoe/TahoeErrorCodes.h>
#define ADL_LOG Tahoe::TH_LOG_ERROR
#else
namespace adl
{
	enum TahoeErrorCodes
	{
		TH_NO_ERROR = 0,
		TH_SUCCESS = 0,
		TH_FAILURE = 1,
		TH_ERROR_MEMORY = 2,
		TH_ERROR_IO = 3,
		TH_ERROR_PARAMETER = 4,
		TH_ERROR_INTERNAL = 5,
		TH_ERROR_UNSUPPORTED_IMAGE_FORMAT = 6,
		TH_ERROR_MATERIAL_STACK_OVERFLOW = 10,
		TH_NOT_SUPPORTED = 11,
		TH_ERROR_OPENGL = 12,
		TH_ERROR_OPENCL = 13,
		TH_ERROR_NULLPTR = 14,
		TH_ERROR_NODETYPE = 15,
		TH_ERROR_OPENCL_OUT_OF_HOST_MEMORY = 16,
	};

#ifdef _DEBUG
	#ifdef _WIN32
	#define ADLASSERT(x, code) if(!(x)){__debugbreak(); }
	#else
	#define ADLASSERT(x, code) if(!(x)){assert(0); }
	#endif
	#define ADLWARN(x) { debugPrintf(x); printf(x);}
#else
	#define ADLASSERT(x, code) if(x){}
	#define ADLWARN(x) {x;}
#endif

#ifdef _DEBUG
	#define COMPILE_TIME_ASSERT(x) {int compileTimeAssertFailed[x]; compileTimeAssertFailed[0];}
#else
	#define COMPILE_TIME_ASSERT(x)
#endif

#ifdef _DEBUG
	__inline
	void debugPrintf(const char *fmt, ...)
	{
		va_list arg;
		va_start(arg, fmt);
#if defined(ADL_DUMP_DX11_ERROR)
		const int size = 1024*10;
		char buf[size];
		vsprintf_s( buf, size, fmt, arg );
#ifdef UNICODE
		WCHAR wbuf[size];
		int sizeWide = MultiByteToWideChar(0,0,buf,-1,wbuf,0);
		MultiByteToWideChar(0,0,buf,-1,wbuf,sizeWide);

//		swprintf_s( wbuf, 256, L"%s", buf );
		OutputDebugString( wbuf );
#else
		OutputDebugString( buf );
#endif
#else
		vprintf(fmt, arg);
#endif
		va_end(arg);
	}
#else
	__inline
	void debugPrintf(const char *fmt, ...)
	{
	}
#endif
#define ADL_LOG debugPrintf
};
#endif

namespace adl
{
void defaultlog( const char* txt );

typedef void (*LogFunc)(const char*);

static
LogFunc s_logCallback = defaultlog;
};
#endif

