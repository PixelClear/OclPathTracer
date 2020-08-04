/*
		2011 Takahiro Harada
*/

#include <fstream>
#include <sys/stat.h>
#include <Adl/Adl.h>

#if defined(WIN32)
#define NOMINMAX 
#include <Windows.h>
#endif

namespace adl
{

static
//char s_cLDefaultCompileOption[] = "-I ..\\..\\ -x clc++";
char s_cLDefaultCompileOption[] = "-I ..\\..\\";

struct KernelCL : public Kernel
{
	cl_kernel& getKernel() { return (cl_kernel&)m_kernel; }
};

static const char* strip(const char* name, const char* pattern)
{
    size_t const patlen = strlen(pattern);
  	size_t patcnt = 0;
    const char * oriptr;
    const char * patloc;
    // find how many times the pattern occurs in the original string
    for (oriptr = name; (patloc = strstr(oriptr, pattern)); oriptr = patloc + patlen)
    {
		patcnt++;
    }
    return oriptr;
}

class FileStat
{
#if defined(WIN32)
public:
	FileStat( const char* filePath )
	{
		m_file = 0;
		m_file = CreateFile(filePath,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
		if (m_file ==INVALID_HANDLE_VALUE)
		{
			DWORD errorCode;
			errorCode = GetLastError();
			switch (errorCode)
			{
			case ERROR_FILE_NOT_FOUND:
			{
				debugPrintf("File not found %s\n", filePath);
				break;
			}
			case ERROR_PATH_NOT_FOUND:
			{
				debugPrintf("File path not found %s\n", filePath);
				break;
			}
			default:
			{
				printf("Failed reading file with errorCode = %d\n", errorCode);
				printf("%s\n", filePath);
			}
			}
		} 

	}
	~FileStat()
	{
		if( m_file!=INVALID_HANDLE_VALUE )
			CloseHandle( m_file );
	}

	bool found() const
	{
		return ( m_file!=INVALID_HANDLE_VALUE );
	}

	unsigned long long getTime()
	{
		if( m_file==INVALID_HANDLE_VALUE )
			return 0;

		unsigned long long t = 0;
		FILETIME exeTime;
		if (GetFileTime(m_file, NULL, NULL, &exeTime)==0)
		{

		}
		else
		{
			unsigned long long u = exeTime.dwHighDateTime;
			t = (u<<32) | exeTime.dwLowDateTime;
		}
		return t;
	}

private:
	HANDLE m_file;
#else
public:
	FileStat( const char* filePath )
	{
		strcpy( m_file, filePath );
	}

	bool found() const
	{
		struct stat binaryStat;
		bool e = stat( m_file, &binaryStat );
		return e==0;
	}

	unsigned long long getTime()
	{
		struct stat binaryStat;
		bool e = stat( m_file, &binaryStat );
		if( e != 0 )
			return 0;
		unsigned long long t = binaryStat.st_mtime;
		return t;
	}

private:
	char m_file[512];
#endif
};

class KernelBuilderCLImpl
{
public:
	static 
	bool isFileUpToDate(const char* binaryFileName,const char* srcFileName);
	static
	cl_program loadFromCache(const Device* device, const char* binaryFileName, const char* option);
	static 
	void hashString(const char *ss, const size_t size, char buf[9]);
	static 
	unsigned int hashBin(const char *s, const size_t size);
	static 
	void getBinaryFileName(const Device* deviceData, const char* fileName, const char* option, char *binaryFileName);
	static
	void handleBuildError(cl_device_id deviceIdx, cl_program program);
	static
	cl_program loadFromSrc( const Device* deviceData, const char* src, const char* option, bool buildProgram = 1 );
	static
	void cacheBinaryToFile(const Device* device, cl_program& program, const char* binaryFileName);
	static
	bool setFromFile( const Device* deviceData, const char* fileName, const char* option, bool addExtension,
		bool cacheKernel, const char** depsList, int nDeps, cl_program& program);
	static
	cl_program setFromStringListsImpl( const Device* deviceData, const char** src, int nSrc, const char* option );
	static
	void createDirectory( const char* cacheDirName );
};

template<>
class KernelBuilder<TYPE_CL>
{
	private:
		enum
		{
			MAX_PATH_LENGTH = 260,
		};
		const Device* m_deviceData;
#ifdef UNICODE
		wchar_t m_path[MAX_PATH_LENGTH];
#else
		char m_path[MAX_PATH_LENGTH];
#endif
	public:
		void* m_ptr;

	private:
		int m_error;

	public:
		KernelBuilder(): m_ptr(0){}

		void setError(cl_int e) { m_error = (e == CL_SUCCESS)?0:1; }
		int getError() const { return m_error; }

		bool setFromFile( const Device* deviceData, const char* fileName, const char* option, bool addExtension,
			bool cacheKernel, const char** depsList, int nDeps )
		{
			m_deviceData = deviceData;
			return KernelBuilderCLImpl::setFromFile( deviceData, fileName, option, addExtension, cacheKernel, depsList, nDeps, (cl_program&)m_ptr );
		}

		bool isCacheUpToDate( const Device* deviceData, const char* fileName, const char* option ) 
		{
			bool upToDate = false;
			char binaryFileName[512];
			if( fileName )
			{
				char fileNameWithExtension[256];
				sprintf( fileNameWithExtension, "%s.cl", fileName );
				KernelBuilderCLImpl::getBinaryFileName(deviceData, fileName, option, binaryFileName);
				upToDate = KernelBuilderCLImpl::isFileUpToDate(binaryFileName,fileNameWithExtension);
			}
			return upToDate;
		}

	//	file name is used for time stamp check of cached binary
	void setFromStringLists( const Device* deviceData, const char** src, int nSrc, const char* option, const char* fileName, bool cacheBinary )
	{
		char fileNameWithExtension[256];

		sprintf( fileNameWithExtension, "%s.cl", fileName );

		bool upToDate = false;
	
		cl_program& program = (cl_program&)m_ptr;
		char binaryFileName[512];
		if( fileName )
		{
			KernelBuilderCLImpl::getBinaryFileName(deviceData, fileName, option, binaryFileName);
			upToDate = KernelBuilderCLImpl::isFileUpToDate(binaryFileName,fileNameWithExtension);
			KernelBuilderCLImpl::createDirectory( s_cacheDirectory );
		}
		if( cacheBinary && upToDate )
		{
			program = KernelBuilderCLImpl::loadFromCache( deviceData, binaryFileName, option );
		}

		if(!m_ptr )
		{
			program = KernelBuilderCLImpl::setFromStringListsImpl( deviceData, src, nSrc, option );
	//		if( cacheBinary )
			if( fileName )
			if( fileName[0] != 0 )
			{	//	write to binary
				KernelBuilderCLImpl::cacheBinaryToFile( deviceData, program, binaryFileName );
			}
		}
	}


	void setFromPrograms( const Device* device, cl_program* programs, int nPrograms, const char* option)
	{
	#if defined(OPENCL_10)
		printf("clLinkProgram is not available in OCL10\n");
		ADLASSERT(0, TH_ERROR_INTERNAL);
	#else
		ADLASSERT( device->m_type == TYPE_CL, TH_ERROR_OPENCL);
		const DeviceCL* cl = (const DeviceCL*) device;
		m_deviceData = device;
		cl_int e;
		cl_program& dst = (cl_program&)m_ptr;
		dst = clLinkProgram( cl->m_context, 1, &cl->m_deviceIdx, option, nPrograms, programs, 0, 0, &e );
		setError( e );
		if( e != CL_SUCCESS )
		{
			KernelBuilderCLImpl::handleBuildError( cl->m_deviceIdx, dst );
		}
		setError( e );
	#endif
	}

	~KernelBuilder()
	{
		cl_program program = (cl_program)m_ptr;
		clReleaseProgram( program );
	}

	void createKernel( const char* funcName, Kernel& kernelOut )
	{
		KernelCL* clKernel = (KernelCL*)&kernelOut;

		cl_program program = (cl_program)m_ptr;
		cl_int status = 0;
		clKernel->getKernel() = clCreateKernel(program, funcName, &status );
		ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
		setError( status );

		kernelOut.m_type = TYPE_CL;
	}

	static
	void deleteKernel( Kernel& kernel )
	{
		KernelCL* clKernel = (KernelCL*)&kernel;
		clReleaseKernel( clKernel->getKernel() );
	}

};



class LauncherCL
{
	public:
		static void setBuffers( Launcher* launcher, BufferInfo* buffInfo, int n );
		template<typename T>
		__inline
		static void setConst( Launcher* launcher, Buffer<T>& constBuff, const T& consts );
		__inline
		static void setLocalMemory( Launcher* launcher, size_t size );
		template<typename T>
		__inline
		static void setConst( Launcher* launcher, const T& consts );
		__inline
		static void setConst( Launcher* launcher, const void* consts, size_t byteCount );
		static void launch2D( Launcher* launcher, int numThreadsX, int numThreadsY, int localSizeX, int localSizeY, SyncObject* syncObj, float& t );
#if SUPPORT_INDIRECT_DISPATCH
		static void launchIndirect2D( Launcher* launcher, Buffer<int>& indirectBuffer, int localSizeX, int localSizeY, SyncObject* syncObj, float& t );
#endif

		static void serializeToFile( Launcher* launcher, const char* filePath, const Launcher::ExecInfo& info );
		static void deserializeFromFile( Launcher* launcher, const char* filePath, int buffCap, Buffer<char>** buffsOut, int* nBuffs, Launcher::ExecInfo& info );
};



template<typename T>
void LauncherCL::setConst( Launcher* launcher, Buffer<T>& constBuff, const T& consts )
{
	KernelCL* clKernel = (KernelCL*)launcher->m_kernel;
	cl_int status = clSetKernelArg( clKernel->getKernel(), launcher->m_idx++, sizeof(T), &consts );
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
}

void LauncherCL::setLocalMemory( Launcher* launcher, size_t size )
{
	KernelCL* clKernel = (KernelCL*)launcher->m_kernel;
	cl_int status = clSetKernelArg(clKernel->getKernel(), launcher->m_idx++, size, NULL);
	ADLASSERT(status == CL_SUCCESS, TH_ERROR_OPENCL);
}

template<typename T>
void LauncherCL::setConst( Launcher* launcher, const T& consts )
{
//    ADLASSERT(launcher->m_idx < Launcher::MAX_ARG_COUNT, TH_ERROR_INTERNAL);
    ADLASSERT(sizeof(T) <= Launcher::MAX_ARG_SIZE, TH_ERROR_INTERNAL);

	KernelCL* clKernel = (KernelCL*)launcher->m_kernel;
	cl_int status = clSetKernelArg( clKernel->getKernel(), launcher->m_idx++, sizeof(T), &consts );
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

	{
		Launcher::Args& arg = launcher->m_args[launcher->m_idx-1];
		arg.m_isBuffer = 0;
		arg.m_sizeInBytes = (int)sizeof(T);
		memcpy( arg.m_data, &consts, sizeof(T) );
	}
}

void LauncherCL::setConst( Launcher* launcher, const void* consts, size_t byteCount )
{
	KernelCL* clKernel = (KernelCL*)launcher->m_kernel;
	cl_int status = clSetKernelArg( clKernel->getKernel(), launcher->m_idx++, byteCount, consts );
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
}

};
