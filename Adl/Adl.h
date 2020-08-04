/*
		2011 Takahiro Harada
*/

#ifndef ADL_H
#define ADL_H

#pragma warning( disable : 4996 )
#include <Adl/AdlConfig.h>
#include <Adl/AdlError.h>
#include <algorithm>
#include <cstdint>
#include <limits.h>

#if defined (ADL_ENABLE_METAL)
#define OPTIMIZE_OUT_NEEDLESS_WAITS 1
#else
#define OPTIMIZE_OUT_NEEDLESS_WAITS 0
#endif

namespace adl
{	
// Callback for compile indication, triggered when compilation starts and ends. Parameters are a flag wether the compile started or ended and a pointer to user defined custom data.
typedef void (*CompileCallbackType)(bool, void*);

struct CompileCallback
{
	CompileCallbackType callback = nullptr;
	void* userData = nullptr;
};

typedef unsigned long long adlu64;

struct adlf4
{
	float x,y,z,w;
	adlf4( float ix, float iy, float iz, float iw = 0.f ) : x(ix), y(iy), z(iz), w(iw){}
};

struct adli2
{
	int x,y;
	adli2( int ix, int iy ) : x(ix), y(iy){}
};

extern
char s_cacheDirectory[128];

#define ADL_SUCCESS 0
#define ADL_FAILURE 1

enum DeviceType
{
	TYPE_CL = 0,
	TYPE_DX11 = 1,
	TYPE_METAL = 2,
	TYPE_VULKAN = 3,
	TYPE_HOST,
};

enum DeviceVendor
{
	VD_AMD		= 1 << 0,
	VD_INTEL	= 1 << 1,
	VD_NV		= 1 << 2,
	VD_APPLE	= 1 << 3,
	VD_UNKNOWN	= 1 << 31,
	VD_ALL		= 0xffffffff,
};

struct Device;
struct SyncObject;

struct BufferBase
{
	enum BufferType
	{
		BUFFER,

		//	for dx
		BUFFER_CONST,
		BUFFER_STAGING,
		BUFFER_APPEND,
		BUFFER_RAW,
		BUFFER_W_COUNTER,
		BUFFER_INDEX,
		BUFFER_VERTEX,

		//	for cl
		BUFFER_ZERO_COPY,

	};
};


bool init( DeviceType type );

void quit( DeviceType type );

class DeviceUtils
{
	public:
		struct Config
		{
			enum DeviceType
			{
				DEVICE_GPU,
				DEVICE_CPU,
			};

			Config() : m_type(DEVICE_GPU), m_deviceIdx(0), m_vendor(VD_ALL), m_clContextProperties(0), m_compileCallback() {}

			DeviceType m_type;
			int m_deviceIdx;
			DeviceVendor m_vendor;
			void* m_clContextProperties;
			// Compile callback that is executed before and after kernel compilation. Can be a nullptr
			CompileCallback m_compileCallback;
		};

		static
		int getNDevices( DeviceType type, Config::DeviceType device = Config::DEVICE_GPU, DeviceVendor vendor = VD_ALL );
		static
		int getNCUs( const Device* device );
		static Device* allocate( DeviceType type, Config cfg = Config() );
		static void deallocate( Device* deviceData );
		static void waitForCompletion( const Device* deviceData );
		static void waitForCompletion( const SyncObject* syncObj );
        static bool isComplete( const SyncObject* syncObj );
		static void flush( const Device* deviceData );
};


//==========================
//	DeviceData
//==========================
struct Kernel;

struct Device
{
	typedef DeviceUtils::Config Config;

	enum ProfileType
	{
		PROFILE_NON = 0,
		PROFILE_RETURN_TIME = 1<<1,
		PROFILE_WRITE_FILE = 1<<2,
	};

	Device( DeviceType type ) : m_type( type ), m_memoryUsage(0), m_memoryPeak(0), m_enableProfiling(0), m_binaryFileVersion(0){}
	virtual ~Device(){}
	
	virtual bool isValid() const { return false; }
	virtual void* getContext() const { return 0; }
	virtual void initialize(const Config& cfg) = 0;
	virtual void release() = 0;
	virtual void waitForCompletion() const = 0;
	virtual void waitForCompletion( const SyncObject* syncObj ) const = 0;
	virtual bool isComplete( const SyncObject* syncObj ) const = 0;
	virtual void flush() const = 0;
	virtual void getDeviceName( char nameOut[128] ) const { nameOut[0] = 0; }
	virtual void getBoardName( char nameOut[128] ) const { nameOut[0] = 0; }
	virtual void getDeviceVendor( char nameOut[128] ) const { nameOut[0] = 0; }
  	virtual void getDeviceVersion( char nameOut[128] ) const { nameOut[0] = 0; }
	virtual bool isKernelBuilt( const char* fileName, const char* funcName, const char* option = NULL ) const { return false; }
	virtual Kernel* getKernel(const char* fileName, const char* funcName, const char* option = NULL, 
		const char** srcList = NULL, int nSrc = 0, const char** depsList = NULL, int nDeps = 0, bool cacheKernel = false ) const { ADLASSERT(0, TH_ERROR_INTERNAL); return 0;}
	virtual adlu64 getUsedMemory() const { return m_memoryUsage; }
	virtual adlu64 getTotalMemory() const { return m_memoryTotal; }
	virtual adlu64 getPeakMemory() const { return m_memoryPeak; }
	void toggleProfiling( ProfileType type ) { m_enableProfiling |= type; if( type == PROFILE_NON ) m_enableProfiling = PROFILE_NON; }
	void setBinaryFileVersion( unsigned int ver ) { m_binaryFileVersion = ver; }
	unsigned int getBinaryFileVersion() const { return m_binaryFileVersion; }
	DeviceType getType() const { return m_type; }
	Config::DeviceType getProcType() const { return m_procType; }
	virtual adlu64 getMemSize() const { return 0; }
	virtual adlu64 getMaxAllocationSize() const { return ULLONG_MAX; }
	virtual void setThreadPriority( bool isHigh ) { }
	virtual uint64_t getExecutionTimeNanoseconds( const SyncObject* syncObj ) const = 0;
	void setCompileCallback( CompileCallback compileCallback ) { m_compileCallback = compileCallback; };

	DeviceType m_type;
	Config::DeviceType m_procType;
	adlu64 m_memoryUsage;
	adlu64 m_memoryTotal;
	adlu64 m_memoryPeak;
	bool m_interopAvailable;
	unsigned int m_enableProfiling;
	unsigned int m_binaryFileVersion;
	bool m_ownedByMiRender = false;

	// This is set during the context creation and is used as get an indication whenever a recompilation of a kernel is started/stopped.
	CompileCallback m_compileCallback;
};

//==========================
//	Buffer
//==========================

template<typename T>
struct HostBuffer;
//	overload each deviceDatas
template<typename T>
struct Buffer : public BufferBase
{
	__inline
	Buffer();
	__inline
	Buffer(const Device* device, adlu64 nElems, BufferType type = BUFFER );
	__inline
	virtual ~Buffer();
	
	__inline
	void setRawPtr( const Device* device, T* ptr, adlu64 size, BufferType type = BUFFER );
	__inline
	void allocate(const Device* device, adlu64 nElems, BufferType type = BUFFER );
	__inline
	void write(const T* hostSrcPtr, adlu64 nElems, adlu64 dstOffsetNElems = 0, SyncObject* syncObj = 0);
	__inline
	void read(T* hostDstPtr, adlu64 nElems, adlu64 srcOffsetNElems = 0, SyncObject* syncObj = 0) const;
	__inline
	void write(const Buffer<T>& src, adlu64 nElems, adlu64 dstOffsetNElems = 0, SyncObject* syncObj = 0);
	__inline
	void read(Buffer<T>& dst, adlu64 nElems, adlu64 offsetNElems = 0, SyncObject* syncObj = 0) const;
    __inline
	void clear();
	__inline
	void fill(void* pattern, int patternSize);
	__inline
	T* getHostPtr(adlu64 size = -1, bool blocking = false) const;
	__inline
	void returnHostPtr(T* ptr) const;
	__inline
	void setSize( adlu64 size );
	__inline
	void* getInternalObject();

//	__inline
//	Buffer<T>& operator = (const Buffer<T>& buffer);
	__inline
	adlu64 getSize() const { return m_size; }

	DeviceType getType() const { ADLASSERT( m_device != 0, TH_ERROR_PARAMETER); return m_device->m_type; }


	const Device* m_device;
	adlu64 m_size;
	T* m_ptr = nullptr;
	//	for DX11
	union
	{
		struct 
		{
			void* m_uav;
			void* m_srv;
		} m_dx11;

		struct
		{
			char* m_hostPtr;
		}m_cl;
	};
	bool m_allocated;	//	todo. move this to a bit
	void* m_implData = nullptr;
};

class BufferUtils
{
public:
	//	map and unmap, buffer will be allocated if necessary
	template<DeviceType TYPE, bool COPY, typename T>
	__inline
	static
	typename adl::Buffer<T>* map(const Device* device, const Buffer<T>* in, int copySize = -1);

	template<bool COPY, typename T>
	__inline
	static
	void unmap( Buffer<T>* native, const Buffer<T>* orig, int copySize = -1 );

	//	map and unmap to preallocated buffer
	template<DeviceType TYPE, bool COPY, typename T>
	__inline
	static
	typename adl::Buffer<T>* mapInplace(const Device* device, Buffer<T>* allocatedBuffer, const Buffer<T>* in, int copySize = -1);

	template<bool COPY, typename T>
	__inline
	static
	void unmapInplace( Buffer<T>* native, const Buffer<T>* orig, int copySize = -1 );
};

//==========================
//	HostBuffer
//==========================
struct DeviceHost;

template<typename T>
struct HostBuffer : public Buffer<T>
{
	__inline
	HostBuffer():Buffer<T>(){}
	__inline
	HostBuffer(const Device* device, int nElems, BufferBase::BufferType type = BufferBase::BUFFER ) : Buffer<T>(device, nElems, type) {}
//	HostBuffer(const Device* deviceData, T* rawPtr, int nElems);


	__inline
	T& operator[](int idx);
	__inline
	const T& operator[](int idx) const;
	__inline
	T* begin() { return Buffer<T>::m_ptr; }

	__inline
	HostBuffer<T>& operator = (const Buffer<T>& device);
};

};

#include <Adl/AdlKernel.h>
#if defined(ADL_ENABLE_CL)
	#include <Adl/CL/AdlCL.inl>
#endif
#if defined(ADL_ENABLE_DX11)
	#include <Adl/DX11/AdlDX11.inl>
#endif
#if defined (ADL_ENABLE_METAL)
#include <Adl/Metal/AdlMetal.h>
#endif
#if defined(ADL_ENABLE_VULKAN)
#include <Adl/Vulkan/AdlVulkan.h>
#endif

#include <Adl/Host/AdlHost.inl>
#include <Adl/Host/AdlStopwatchHost.inl>
#include <Adl/AdlKernel.inl>
#include <Adl/Adl.inl>

#endif
