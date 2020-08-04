/*
		2011 Takahiro Harada
*/

#if defined(TH_OCL_STATIC_LINK)
#pragma comment(lib,"OpenCL.lib")
#if defined(__APPLE__)
	#include <OpenCL/cl.h>
	#include <OpenCL/cl_ext.h>
	#include <OpenCL/cl_gl_ext.h>
	#include <OpenCL/cl_platform.h>
#else
	#include <CL/cl.h>
	#include <CL/cl_ext.h>
	#include <CL/cl_gl.h>
	#include <CL/cl_platform.h>
#endif
inline
int clewInit_tahoe() { return 0; }
inline
void clewExit_tahoe() {}
#define CLEW_SUCCESS 0
#else
#include <Adl/CL/clew/clew.h>
#endif
#include <string.h>

namespace adl
{

struct DeviceCL : public Device
{
	typedef DeviceUtils::Config Config;

	static
	bool init();

	static
	void quit();

	DeviceCL();

	bool isValid() const { return m_context != 0; }

	void* getContext() const { return m_context; }

	void initialize(const Config& cfg);

	void release();

	template<typename T>
	__inline
	void allocate(Buffer<T>* buf, adlu64 nElems, BufferBase::BufferType type);

	template<typename T>
	__inline
	void deallocate(Buffer<T>* buf);

	template<typename T>
	__inline
	void copy(Buffer<T>* dst, const Buffer<T>* src, adlu64 nElems, adlu64 dstOffsetNElems = 0, SyncObject* syncObj = 0);

	template<typename T>
	__inline
	void copy(T* dst, const Buffer<T>* src, adlu64 nElems, adlu64 srcOffsetNElems = 0, SyncObject* syncObj = 0);

	template<typename T>
	__inline
	void copy(Buffer<T>* dst, const T* src, adlu64 nElems, adlu64 dstOffsetNElems = 0, SyncObject* syncObj = 0);

    template<typename T>
	__inline
	void clear(const Buffer<T>* const src) const;

	template<typename T>
	__inline
	void fill(const Buffer<T>* const src, void* pattern, int patternSize) const;

	template<typename T>
	__inline
	void getHostPtr(const Buffer<T>* const src, adlu64 size, T*& ptr, bool blocking = false) const;

	template<typename T>
	__inline
	void returnHostPtr(const Buffer<T>* const src, T* ptr) const;

	void waitForCompletion() const;

	void waitForCompletion( const SyncObject* syncObj ) const;

	bool isComplete( const SyncObject* syncObj ) const;
    
	void flush() const;

	__inline
	void allocateSyncObj( void*& ptr ) const;

	__inline
	void deallocateSyncObj( void* ptr ) const;

	void getDeviceName( char nameOut[128] ) const;

	void getBoardName( char nameOut[128] ) const;

	void getDeviceVendor( char nameOut[128] ) const;

	void getDeviceVersion( char nameOut[128] ) const;

	__inline
	cl_event* extract( SyncObject* syncObj )
	{
		if( syncObj == 0 ) return 0;
		ADLASSERT( syncObj->m_device->getType() == TYPE_CL, TH_ERROR_PARAMETER );
		cl_event* e = (cl_event*)syncObj->m_ptr;
		if( *e )
		{
			cl_int status = clReleaseEvent( *e );
			ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL );
		}
		return e;
	}

	static
	int getNDevices( Config::DeviceType device, DeviceVendor vendor );

	int getNCUs() const;

	adlu64 getMemSize() const;

	adlu64 getMaxAllocationSize() const;

	virtual bool isKernelBuilt( const char* fileName, const char* funcName, const char* option = NULL ) const;

	Kernel* getKernel(const char* fileName, const char* funcName, const char* option = NULL, const char** srcList = NULL, int nSrc = 0, const char** depsList = NULL, int nDeps = 0, bool cacheKernel = true )const;

	static
	DeviceVendor getVendorType( const char* name );

	virtual void setThreadPriority( bool isHigh );

	uint64_t getExecutionTimeNanoseconds(const SyncObject * syncObj) const;

	enum
	{
		MAX_NUM_DEVICES = 32,
	};

	cl_context m_context;
	cl_command_queue m_commandQueue;
	cl_device_id m_deviceIdx;

	KernelManager* m_kernelManager;

	char m_deviceName[256];
	char m_driverVersion[256];
    int m_maxLocalWGSize[3];

	DeviceVendor m_vendor;
	unsigned long long m_maxAllocSize;

	static 
	int s_refCount;
};

//===
//===



template<typename T>
void DeviceCL::allocate(Buffer<T>* buf, adlu64 nElems, BufferBase::BufferType type)
{
	buf->m_device = this;
	buf->m_size = nElems;
	buf->m_ptr = 0;

	if( type == BufferBase::BUFFER_CONST ) return;

#if defined(ADL_CL_DUMP_MEMORY_LOG)
//	char deviceName[256];
//	getDeviceName( deviceName );
//   	printf( "adlCLMemoryLog	%s : %3.2fMB	Allocation: %3.2fKB ", deviceName, m_memoryUsage/1024.f/1024.f, sizeof(T)*nElems/1024.f );
//	fflush( stdout );
#endif
	const adlu64 buffSize = sizeof(T)*buf->m_size;
//	if( m_vendor != VD_AMD )
	{
		const adlu64 gb = 1024*1024*1024;
		const adlu64 currentUse = 0.25f*gb;
		if( (buffSize + getUsedMemory() > m_memoryTotal - currentUse) || buffSize > m_maxAllocSize )
		{
			ADL_LOG("CL Memory Allocation Failure: %3.2fMB, Total used memory: %3.2fMB\n",
				buf->m_size * sizeof(T) / 1024.f / 1024.f, m_memoryUsage / 1024.f / 1024.f);
			buf->m_size = 0;
			buf->m_ptr = 0;
			return;
		}
	}

	nElems = (nElems == 0)?1:nElems;
	cl_int status = 0;
	if( buffSize >= m_maxAllocSize )
	{
		char* ptr = new char[sizeof(T)*nElems];
		buf->m_ptr = (T*)clCreateBuffer( m_context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, sizeof(T)*nElems, ptr, &status );
		buf->m_cl.m_hostPtr = ptr;
	}
	else
	{
	if( type == BufferBase::BUFFER_ZERO_COPY )
		buf->m_ptr = (T*)clCreateBuffer( m_context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, sizeof(T)*nElems, 0, &status );
	else if( type == BufferBase::BUFFER_RAW )
		buf->m_ptr = (T*)clCreateBuffer( m_context, CL_MEM_WRITE_ONLY, sizeof(T)*nElems, 0, &status );
	else
		buf->m_ptr = (T*)clCreateBuffer( m_context, CL_MEM_READ_WRITE, sizeof(T)*nElems, 0, &status );
	}

//	printf( "clCreateBuffer: %s (%3.2fMB)	%dB\n", (status==CL_SUCCESS)? "O": "X", m_memoryUsage/1024.f/1024.f, buf->m_size*sizeof(T) );
	if( status != CL_SUCCESS )
	{
		ADL_LOG("CL Memory Allocation Failure: %3.2fMB, Total used memory: %3.2fMB\n", 
			buf->m_size*sizeof(T)/1024.f/1024.f, m_memoryUsage/1024.f/1024.f);

		cl_ulong size;
		clGetDeviceInfo( m_deviceIdx, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(size), &size, 0 );
		ADL_LOG("CL Max Memory Allocation Size: %3.2fMB\n", size/1024.f/1024.f);

//		printf("  Error %d (%3.2fMB allocation, %3.2fMB used)\n", status, buf->m_size*sizeof(T)/1024.f/1024.f, m_memoryUsage/1024.f/1024.f);
//		if( status == CL_MEM_OBJECT_ALLOCATION_FAILURE )
//			printf("	Can be out of memory");
//		exit(0);
		buf->m_size = 0;
		buf->m_ptr = 0;
		return;
	}

	m_memoryUsage += buf->m_size*sizeof(T);
	m_memoryPeak = (m_memoryPeak>m_memoryUsage)?m_memoryPeak:m_memoryUsage;
#if defined(_DEBUG)
	if( status == CL_INVALID_BUFFER_SIZE )
	{
		cl_ulong size;
		clGetDeviceInfo( m_deviceIdx, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(size), &size, 0 );
		printf("CL_DEVICE_MAX_MEM_ALLOC_SIZE: %3.2f MB\n", size/1024.f/1024.f);
		printf("Trying to allocate %3.2fMB\n", sizeof(T)*nElems/sizeof(char)/1024.f/1024.f);
	}
#endif
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
}

template<typename T>
void DeviceCL::deallocate(Buffer<T>* buf)
{
	if( buf->m_ptr )
	{
		ADLASSERT( m_memoryUsage >= buf->m_size*sizeof(T), TH_ERROR_PARAMETER );
		m_memoryUsage -= buf->m_size*sizeof(T);
		clReleaseMemObject( (cl_mem)buf->m_ptr );
	}
	buf->m_device = 0;
	buf->m_size = 0;
	buf->m_ptr = 0;

	if( buf->m_cl.m_hostPtr )
	{
		delete [] buf->m_cl.m_hostPtr;
	}
}

template<typename T>
void DeviceCL::copy(Buffer<T>* dst, const Buffer<T>* src, adlu64 nElems, adlu64 dstOffsetNElems, SyncObject* syncObj )
{
	cl_event* e = extract( syncObj );

	if( dst->m_device->m_type == TYPE_CL && src->m_device->m_type == TYPE_CL )
	{
		cl_int status = 0;
		status = clEnqueueCopyBuffer( m_commandQueue, (cl_mem)src->m_ptr, (cl_mem)dst->m_ptr, 0, sizeof(T)*dstOffsetNElems, sizeof(T)*nElems, 0, 0, e );
		ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
	}
	else if( src->m_device->m_type == TYPE_HOST )
	{
		ADLASSERT( dst->getType() == TYPE_CL, TH_ERROR_OPENCL);
		dst->write( src->m_ptr, nElems, dstOffsetNElems, syncObj );
	}
	else if( dst->m_device->m_type == TYPE_HOST )
	{
		ADLASSERT( src->getType() == TYPE_CL, TH_ERROR_OPENCL);
		src->read( dst->m_ptr, nElems, dstOffsetNElems, syncObj );
	}
	else
	{
		ADLASSERT( 0, TH_ERROR_INTERNAL );
	}
}

template<typename T>
void DeviceCL::copy(T* dst, const Buffer<T>* src, adlu64 nElems, adlu64 srcOffsetNElems, SyncObject* syncObj )
{
	cl_event* e = extract( syncObj );
	cl_int status = 0;
/*	{
		void* ptr = clEnqueueMapBuffer( m_commandQueue, (cl_mem)src->m_ptr, 1, CL_MAP_READ, sizeof(T)*srcOffsetNElems, sizeof(T)*nElems, 
			0,0,0,&status );
		ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL );
		memcpy( dst, ptr, sizeof(T)*nElems );
		status = clEnqueueUnmapMemObject( m_commandQueue, (cl_mem)src->m_ptr, ptr, 0, 0, 0 );
		ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL );
	}
*/
	status = clEnqueueReadBuffer( m_commandQueue, (cl_mem)src->m_ptr, 0, sizeof(T)*srcOffsetNElems, sizeof(T)*nElems,
		dst, 0,0,e );
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
#if defined(_DEBUG)
	waitForCompletion();
#endif
}

template<typename T>
void DeviceCL::copy(Buffer<T>* dst, const T* src, adlu64 nElems, adlu64 dstOffsetNElems, SyncObject* syncObj )
{
	cl_event* e = extract( syncObj );
	cl_int status = 0;
/*	{
		void* ptr = clEnqueueMapBuffer( m_commandQueue, (cl_mem)dst->m_ptr, 1, CL_MAP_WRITE, sizeof(T)*dstOffsetNElems, sizeof(T)*nElems,
			0,0,0,&status );
		ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL );
		memcpy( ptr, src, sizeof(T)*nElems );
		status = clEnqueueUnmapMemObject( m_commandQueue, (cl_mem)dst->m_ptr, ptr, 0, 0, 0 );
		ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL );
	}
*/
	status = clEnqueueWriteBuffer( m_commandQueue, (cl_mem)dst->m_ptr, 0, sizeof(T)*dstOffsetNElems, sizeof(T)*nElems,
		src, 0,0,e );
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
#if defined(_DEBUG)
	waitForCompletion();
#endif
}

template<typename T>
void DeviceCL::clear(const Buffer<T>* const src) const
{
//	todo. cannot have the same name for the embedded kernels. 
	adlu64 byteCount = src->getSize() * sizeof(T);

	const char* source = "";
	adlu64 elementCount = 0;

	Kernel* kernel;
	if (byteCount % (4 * sizeof(unsigned int)) == 0)
	{
		source = "__kernel void _memclear_u4(__global uint4* mem, ulong size)"
			"{"
			"    if (get_global_id(0) < size)"
			"        mem[get_global_id(0)] = (uint4)(0,0,0,0); "
			"}";

		elementCount = byteCount / (4 * sizeof(unsigned int));
		kernel = getKernel(0, "_memclear_u4", 0, &source, 1 );
	}
	else
	{
		source = "__kernel void _memclear_c(__global char* mem, ulong size)"
			"{"
			"    if (get_global_id(0) < size)"
			"        mem[get_global_id(0)] = 0; "
			"}";

		elementCount = byteCount;
		kernel = getKernel(0, "_memclear_c", 0, &source, 1 );
	}

    Launcher launcher(this, kernel);

    BufferInfo bInfo = BufferInfo( src );
    launcher.setBuffers(&bInfo, sizeof(bInfo)/sizeof(BufferInfo));

    launcher.setConst( elementCount );

    launcher.launch1D( elementCount, 256 );

//    clFinish( m_commandQueue );
}

template<typename T>void DeviceCL::fill(const Buffer<T>* const src, void* pattern, int patternSize) const
{
	if( patternSize == sizeof(char) )
	{
		char value = *((char*)pattern);
		const char* source =  "__kernel void fillC(__global char* mem, uint size, char value)"
			"{"
			"    if (get_global_id(0) < size)"
			"        mem[get_global_id(0)] = value; "
			"}";
		int size = src->getSize()*sizeof(T);
		Kernel* kernel = getKernel(0, "fillC", 0, &source, 1 );
		Launcher launcher(this, kernel);
		BufferInfo bInfo = BufferInfo( src );
		launcher.setBuffers(&bInfo, sizeof(bInfo)/sizeof(BufferInfo));
		launcher.setConst( size );
		launcher.setConst( value );
		launcher.launch1D( src->getSize()*sizeof(T)/sizeof(char) );
	}
	else if( patternSize == sizeof(int) )
	{
		int value = *((int*)pattern);
		const char* source =  "__kernel void fillI(__global int* mem, uint size, int value)"
			"{"
			"    if (get_global_id(0) < size)"
			"        mem[get_global_id(0)] = value; "
			"}";
		int size = src->getSize()*sizeof(T);
		Kernel* kernel = getKernel(0, "fillI", 0, &source, 1 );
		Launcher launcher(this, kernel);
		BufferInfo bInfo = BufferInfo( src );
		launcher.setBuffers(&bInfo, sizeof(bInfo)/sizeof(BufferInfo));
		launcher.setConst( size );
		launcher.setConst( value );
		launcher.launch1D( src->getSize()*sizeof(T)/sizeof(int) );
	}
	else
	{
#if defined(__APPLE__)
		ADLASSERT(0, TH_ERROR_INTERNAL);// todo. broken
#endif
		cl_int status = clEnqueueFillBuffer( m_commandQueue, (cl_mem)src->m_ptr, pattern, patternSize, 0, src->getSize()*sizeof(T), 0, 0, 0 );

		ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
	}
}


template<typename T>
void DeviceCL::getHostPtr(const Buffer<T>* const src, adlu64 size, T*& ptr, bool blocking) const
{
	size = (size == -1)? src->getSize():size;
	cl_int e;
	ptr = (T*)clEnqueueMapBuffer( m_commandQueue, (cl_mem)src->m_ptr, blocking, CL_MAP_READ | CL_MAP_WRITE, 0,
		sizeof(T)*size, 0,0,0,&e );
	ADLASSERT( e == CL_SUCCESS, TH_ERROR_OPENCL);
#if defined(_DEBUG)
	waitForCompletion();
#endif
}

template<typename T>
void DeviceCL::returnHostPtr(const Buffer<T>* const src, T* ptr) const
{
	cl_int e = clEnqueueUnmapMemObject( m_commandQueue, (cl_mem)src->m_ptr, ptr, 0,0,0 );
	ADLASSERT( e == CL_SUCCESS, TH_ERROR_OPENCL);
#if defined(_DEBUG)
	waitForCompletion();
#endif
}


void DeviceCL::allocateSyncObj( void*& ptr ) const
{
	cl_event* e = (cl_event*)new cl_event;
	*e = 0;
	ptr = e;
}

void DeviceCL::deallocateSyncObj( void* ptr ) const
{
	cl_event* e = (cl_event*)ptr;
	if( *e )
	{
		cl_int status = clReleaseEvent( *e );
		ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
	}
	delete e;
}

#undef max2

};
