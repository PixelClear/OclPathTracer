/*
		2011 Takahiro Harada
*/

#ifdef ADL_ENABLE_CL
	#include <Adl/CL/AdlKernelUtilsCL.inl>
#endif
#ifdef ADL_ENABLE_DX11
	#include <Adl/DX11/AdlKernelUtilsDX11.inl>
#endif
#ifdef ADL_ENABLE_METAL
    #include <Adl/Metal/AdlKernelUtilsMetal.h>
#endif
#if defined ADL_ENABLE_VULKAN
	#include <Adl/Vulkan/AdlKernelUtilsVulkan.h>
#endif

#include <Adl/Adl.h>

namespace adl
{

//==========================
//	Launcher
//==========================

#if defined(ADL_ENABLE_DX11)
	#if defined(ADL_ENABLE_CL)
	#define SELECT_DEVICEDATA( type, func ) \
		switch( type ) \
		{ \
		case TYPE_CL: ((DeviceCL*)m_device)->func; break; \
		case TYPE_DX11: ((DeviceDX11*)m_device)->func; break; \
		case TYPE_HOST: ((DeviceHost*)m_device)->func; break; \
		default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
		}
	#define SELECT_LAUNCHER( type, func ) \
		switch( type ) \
		{ \
		case TYPE_CL: LauncherCL::func; break; \
		case TYPE_DX11: LauncherDX11::func; break; \
		default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
		};
	#else
	#define SELECT_DEVICEDATA( type, func ) \
		switch( type ) \
		{ \
		case TYPE_DX11: ((DeviceDX11*)m_device)->func; break; \
		case TYPE_HOST: ((DeviceHost*)m_device)->func; break; \
		default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
		}
	#define SELECT_LAUNCHER( type, func ) \
		switch( type ) \
		{ \
		case TYPE_DX11: LauncherDX11::func; break; \
		default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
		};
	#endif
#else
	#if defined(ADL_ENABLE_CL)
		#if defined(ADL_ENABLE_METAL)
			#define SELECT_DEVICEDATA( type, func ) \
				switch( type ) \
				{ \
				case TYPE_CL: ((DeviceCL*)m_device)->func; break; \
				case TYPE_METAL: ((DeviceMetal*)m_device)->func; break; \
				case TYPE_HOST: ((DeviceHost*)m_device)->func; break; \
				default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
				}
			#define SELECT_LAUNCHER( type, func ) \
				switch( type ) \
				{ \
				case TYPE_CL: LauncherCL::func; break; \
				case TYPE_METAL: LauncherMetal::func; break; \
				default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
				};
		#elif defined(ADL_ENABLE_VULKAN)
			#define SELECT_DEVICEDATA( type, func ) \
				switch( type ) \
				{ \
				case TYPE_CL: ((DeviceCL*)m_device)->func; break; \
				case TYPE_VULKAN: ((DeviceVulkan*)m_device)->func; break; \
				case TYPE_HOST: ((DeviceHost*)m_device)->func; break; \
				default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
				}
			#define SELECT_LAUNCHER( type, func ) \
				switch( type ) \
				{ \
				case TYPE_CL: LauncherCL::func; break; \
				case TYPE_VULKAN: LauncherVulkan::func; break; \
				default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
				};
		#else
			#define SELECT_DEVICEDATA( type, func ) \
				switch( type ) \
				{ \
				case TYPE_CL: ((DeviceCL*)m_device)->func; break; \
				case TYPE_HOST: ((DeviceHost*)m_device)->func; break; \
				default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
				}
			#define SELECT_LAUNCHER( type, func ) \
				switch( type ) \
				{ \
				case TYPE_CL: LauncherCL::func; break; \
				default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
				};
		#endif
	#else
	#define SELECT_DEVICEDATA( type, func ) \
		switch( type ) \
		{ \
		case TYPE_HOST: ((DeviceHost*)m_device)->func; break; \
		default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
		}
	#define SELECT_LAUNCHER( type, func ) \
		switch( type ) \
		{ \
		default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
		};
	#endif
#endif

__inline
SyncObject::SyncObject( const Device* device ) : m_device( device )
{
//	m_device->allocateSyncObj( m_ptr );
	SELECT_DEVICEDATA( m_device->m_type, allocateSyncObj( m_ptr ) );
}

__inline
SyncObject::~SyncObject()
{
//	m_device->deallocateSyncObj( m_ptr );
	SELECT_DEVICEDATA( m_device->m_type, deallocateSyncObj( m_ptr ) );
}
Launcher::Launcher(const Device *dd, const char *fileName, const char *funcName, const char *option)
{
	m_kernel = dd->getKernel( fileName, funcName, option );
	m_deviceData = dd;
	m_idx = 0;
	m_idxRw = 0;
}

Launcher::Launcher(const Device* dd, const Kernel* kernel)
{
	m_kernel = kernel;
	m_deviceData = dd;
	m_idx = 0;
	m_idxRw = 0;
}

void Launcher::setBuffers( BufferInfo* buffInfo, int n )
{
	SELECT_LAUNCHER( m_deviceData->m_type, setBuffers( this, buffInfo, n ) );
}

template<typename T>
void Launcher::setConst( Buffer<T>& constBuff, const T& consts )
{
	SELECT_LAUNCHER( m_deviceData->m_type, setConst( this, constBuff, consts ) );
}

template<typename T>
void Launcher::setConst( const T& consts )
{
	SELECT_LAUNCHER( m_deviceData->m_type, setConst( this, consts ) );
}

void Launcher::setConst( const void* consts, size_t byteCount )
{
	SELECT_LAUNCHER( m_deviceData->m_type, setConst( this, consts, byteCount ) );
}

void Launcher::setLocalMemory( size_t size )
{
	SELECT_LAUNCHER( m_deviceData->m_type, setLocalMemory( this, size ) );
}

float Launcher::launch1D( int numThreads, int localSize, SyncObject* syncObj )
{
	float t;
	SELECT_LAUNCHER( m_deviceData->m_type, launch2D( this, numThreads, 1, localSize, 1, syncObj, t ) );
	return t;
}

float Launcher::launch2D(  int numThreadsX, int numThreadsY, int localSizeX, int localSizeY, SyncObject* syncObj )
{
	float t;
	SELECT_LAUNCHER( m_deviceData->m_type, launch2D( this, numThreadsX, numThreadsY, localSizeX, localSizeY, syncObj, t ) );
	return t;
}

float Launcher::launch( ThreadsToExec& threadsToExec, SyncObject* syncObj )
{
	float t;
#if SUPPORT_INDIRECT_DISPATCH
    if(threadsToExec.m_indirectBuffer)
    {
        SELECT_LAUNCHER( m_deviceData->m_type, launchIndirect2D( this, *threadsToExec.m_indirectBuffer, threadsToExec.m_localSize[0], threadsToExec.m_localSize[1], syncObj, t ) );
    }
    else
#endif
    {
        SELECT_LAUNCHER( m_deviceData->m_type, launch2D( this, threadsToExec.m_numThreads[0], threadsToExec.m_numThreads[1], threadsToExec.m_localSize[0], threadsToExec.m_localSize[1], syncObj, t ) );
    }
	return t;
}

void Launcher::serializeToFile( const char* filePath, const Launcher::ExecInfo& info )
{
	SELECT_LAUNCHER( m_deviceData->m_type, serializeToFile( this, filePath, info ) );
}

Launcher::ExecInfo Launcher::deserializeFromFile( const char* filePath, int buffCap, Buffer<char>** buffsOut, int* nBuffs )
{
	Launcher::ExecInfo info;
	SELECT_LAUNCHER( m_deviceData->m_type, deserializeFromFile( this, filePath, buffCap, buffsOut, nBuffs, info ) );
	return info;
}


#undef SELECT_LAUNCHER
#undef SELECT_DEVICEDATA
};
