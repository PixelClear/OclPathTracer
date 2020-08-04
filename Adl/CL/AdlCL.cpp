/*
		2011 Takahiro Harada
*/

#include <Adl/Adl.h>
#include <algorithm>

#define max2 std::max
#define min2 std::min

namespace adl
{
DeviceCL::DeviceCL() : Device(TYPE_CL), m_context(0), m_commandQueue(0), m_deviceIdx(0), m_kernelManager(0)
{

}

bool DeviceCL::init()
{
	s_refCount++;
	int e = clewInit_tahoe();
	return e == CLEW_SUCCESS;
}

void DeviceCL::quit()
{
	s_refCount--;
	if( s_refCount == 0 )
		clewExit_tahoe();
}

inline
int getPlatforms( cl_platform_id* platforms, const int maxPlatforms )
{
	cl_uint nPlatforms;
	cl_int status;
	status = clGetPlatformIDs(0, NULL, &nPlatforms);
	debugPrintf("CL %d platforms\n", nPlatforms);
	ADLASSERT( nPlatforms < maxPlatforms, TH_ERROR_PARAMETER );
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

	status = clGetPlatformIDs(nPlatforms, platforms, NULL);
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

	{
		for(cl_uint i=0; i<nPlatforms; i++)
		{
			char buff[512];
			cl_platform_id p = platforms[i];
			status = clGetPlatformInfo( p, CL_PLATFORM_VENDOR, 512, buff, 0 );
			ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

			DeviceVendor deviceVendor = DeviceCL::getVendorType( buff );

			if( deviceVendor == VD_AMD )
			{
				cl_platform_id tmp = platforms[0];
				platforms[0] = platforms[i];
				platforms[i] = tmp;
				break;
			}
		}
	}

	return nPlatforms;
}

void DeviceCL::initialize(const Config& cfg)
{
	debugPrintf("> CL Initialization\n");
	m_procType = cfg.m_type;
	{
		DeviceCL* deviceData = (DeviceCL*)this;

		cl_device_type deviceType = (cfg.m_type== Config::DEVICE_GPU)? CL_DEVICE_TYPE_GPU: CL_DEVICE_TYPE_CPU;
		bool enableProfiling = false;
#if defined(_DEBUG) || defined(PROF_BUILD)
    enableProfiling = true;
    defaultlog("OpenCL device profiling enabled.");
#endif
		cl_int status;

//===
		int nDIds = 0;
		cl_device_id deviceIds[ MAX_NUM_DEVICES ];
		cl_platform_id platformOfDevice[ MAX_NUM_DEVICES ];
		{
			cl_uint nPlatforms;
			const int maxPlatforms = 16;
			cl_platform_id platforms[ maxPlatforms ];
			cl_uint nDevices[ maxPlatforms ] = {0};
			nPlatforms = getPlatforms( platforms, maxPlatforms );

			bool platformAMDfound = false;

			for(cl_uint i=0; i<nPlatforms; i++)
			{
				char buff[512];
				status = clGetPlatformInfo( platforms[i], CL_PLATFORM_VENDOR, 512, buff, 0 );
				ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

				//skip the platform if there are no devices available
				status = clGetDeviceIDs( platforms[i], deviceType, 0, NULL, &nDevices[i] );

				DeviceVendor deviceVendor = getVendorType( buff );

				if( !(cfg.m_vendor & deviceVendor) )
				{
					nDevices[i] = 0;
				}



				if ( deviceVendor == VD_AMD )
				{
					if ( !platformAMDfound ) 
					{
						platformAMDfound = true;
					}
					else
					{
						// FIR-841 - sometimes, because of driver install issue.
						// we will have 2 identical AMD platform installed.
						// so if the system has 2 real GPUs then we have 4 GPUs seen by  Api::getDeviceInfo
						// in order to workaround that, we will only take the 1st AMD platform
						// search  FIR-841  in the source code to find every places where this workaround is.
						nDevices[i] = 0;
					}
				}



				debugPrintf("CL: %d device (%s)\n", nDevices[i], buff );
			}

			//	get device ids
			for(cl_uint i=0; i<nPlatforms; i++)
			{
				status = clGetDeviceIDs( platforms[i], deviceType, nDevices[i], &deviceIds[nDIds], NULL );
				for(cl_uint j=0; j<nDevices[i]; j++)
				{
					platformOfDevice[nDIds+j] = platforms[i];
				}
//				ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL );
				nDIds += nDevices[i];
			}
		}
		if( nDIds == 0 )
		{
			return;
		}
		{
			deviceData->m_context = 0;
			m_deviceIdx = deviceIds[min2( (int)nDIds-1, cfg.m_deviceIdx )];
			cl_platform_id platform = platformOfDevice[ min2( (int)nDIds-1, cfg.m_deviceIdx ) ];

			cl_context_properties fullProps[32] = {0};
			if( cfg.m_clContextProperties )
			{	//	for gl cl interop
				cl_context_properties* props = (cl_context_properties*)cfg.m_clContextProperties;
				int i =0;
				while( props[i] )
				{
					fullProps[i] = props[i]; i++; // property name
					fullProps[i] = props[i]; i++; // value
				}
//#if !defined(__APPLE__)
				fullProps[i] = CL_CONTEXT_PLATFORM;
				fullProps[i+1] = (cl_context_properties)platform;
//#endif
				// if there are more than 1 gpus in a system, the device index should be the device index which gl context is created
//				for(cl_uint idx=0; idx<numDevice; idx++)


				cl_device_id glDeviceId = m_deviceIdx;

				//	this was necessary for FMP-84, to use 4GPUs in Maya plugin
#if defined(WIN32)
				// static clGetGLContextInfoKHR_fn clGetGLContextInfoKHR = 0; <-- don't make it static, otherwise we may have undefined pointer when creating context several times.
				if (!clGetGLContextInfoKHR)
				{
					clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)clGetExtensionFunctionAddress("clGetGLContextInfoKHR");
				}
				if( clGetGLContextInfoKHR )
				{
					glDeviceId = 0;
					status = clGetGLContextInfoKHR( fullProps, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &glDeviceId, 0 );
//					ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
				}
#else
//                status = clGetGLContextInfoAPPLE( fullProps, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &glDeviceId, 0 );
//                ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL );
#endif
				if( glDeviceId == m_deviceIdx )
				{
					deviceData->m_context = clCreateContext( fullProps, 1, &m_deviceIdx, NULL, NULL, &status );
					if( deviceData->m_context != 0 )
						deviceData->m_interopAvailable = 1;
//						break;
//					clReleaseContext( deviceData->m_context );
				}
			}
			if( deviceData->m_context == 0 )
			{
				deviceData->m_context = clCreateContext( 0, 1, &deviceData->m_deviceIdx, NULL, NULL, &status );
			}
			ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

			char buff[512];
			status = clGetDeviceInfo( deviceData->m_deviceIdx, CL_DEVICE_NAME, sizeof(buff), &buff, NULL );
			ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

			debugPrintf("[%s]\n", buff);

			deviceData->m_commandQueue = clCreateCommandQueue( deviceData->m_context, deviceData->m_deviceIdx, (enableProfiling)?CL_QUEUE_PROFILING_ENABLE:0, &status );

			if( status == CL_OUT_OF_HOST_MEMORY )
				ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL_OUT_OF_HOST_MEMORY); 
			ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
			ADLASSERT( deviceData->m_commandQueue != 0, TH_ERROR_OPENCL);
			
		//	status = clSetCommandQueueProperty( commandQueue, CL_QUEUE_PROFILING_ENABLE, CL_TRUE, 0 );
		//	CLASSERT( status == CL_SUCCESS );

			if(0)
			{
				cl_bool image_support;
				clGetDeviceInfo(deviceData->m_deviceIdx, CL_DEVICE_IMAGE_SUPPORT, sizeof(image_support), &image_support, NULL);
				debugPrintf("	CL_DEVICE_IMAGE_SUPPORT : %s\n", image_support?"Yes":"No");
			}
		}
	}

	m_kernelManager = new KernelManager;

	{
		getDeviceName( m_deviceName );
		clGetDeviceInfo( m_deviceIdx, CL_DRIVER_VERSION, 256, &m_driverVersion, NULL);
        
        
		DeviceCL* deviceData = (DeviceCL*)this;
        size_t wgSize[3];
        clGetDeviceInfo( deviceData->m_deviceIdx, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(wgSize), wgSize, 0 );
        m_maxLocalWGSize[0] = (int)wgSize[0];
        m_maxLocalWGSize[1] = (int)wgSize[1];
        m_maxLocalWGSize[2] = (int)wgSize[2];

        size_t ww;
        clGetDeviceInfo( deviceData->m_deviceIdx, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(ww), &ww, 0 );
        
        
        cl_uint maxWorkDim;
        clGetDeviceInfo( deviceData->m_deviceIdx, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(maxWorkDim), &maxWorkDim, 0 );

        debugPrintf("CL_DEVICE_MAX_WORK_ITEM_SIZES  : %d, %d, %d\n", m_maxLocalWGSize[0], m_maxLocalWGSize[1], m_maxLocalWGSize[2]);
        debugPrintf("CL_DEVICE_MAX_WORK_GROUP_SIZE  : %d\n", (int)ww);
        debugPrintf("CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS : %d\n", (int)maxWorkDim);
	}

	{
		char c[128];
		getDeviceVendor(c);
		m_vendor = getVendorType( c );
	}
	{
		clGetDeviceInfo( m_deviceIdx, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(m_maxAllocSize), &m_maxAllocSize, 0 );
	}
	m_memoryTotal = getMemSize();

	debugPrintf("< CL Initialization\n");
}

void DeviceCL::release()
{
	ADLASSERT( m_memoryUsage == 0, TH_ERROR_PARAMETER );
	clReleaseCommandQueue( m_commandQueue );
	clReleaseContext( m_context );

	if( m_kernelManager ) delete m_kernelManager;
}

void DeviceCL::waitForCompletion() const
{
	clFinish( m_commandQueue );
}

void DeviceCL::waitForCompletion( const SyncObject* syncObj ) const
{
	if( syncObj )
	{
		ADLASSERT( syncObj->m_device->getType() == TYPE_CL, TH_ERROR_OPENCL);
		cl_event* e = (cl_event*)syncObj->m_ptr;
		if( *e )
		{
			cl_int status = clWaitForEvents( 1, e );
			ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
			status = clReleaseEvent( *e );
			ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
			*e = 0;
		}
	}
}
    
bool DeviceCL::isComplete( const SyncObject* syncObj ) const
{
    if( syncObj )
    {
        ADLASSERT( syncObj->m_device->getType() == TYPE_CL, TH_ERROR_OPENCL);
        cl_event* e = (cl_event*)syncObj->m_ptr;
        if( *e )
        {
            int status;
            int er = clGetEventInfo( *e, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(int), (void*)&status, 0);
            ADLASSERT( er == CL_SUCCESS, TH_ERROR_OPENCL);
            
            if( status == CL_COMPLETE )
            {
                er = clReleaseEvent( *e );
                ADLASSERT( er == CL_SUCCESS, TH_ERROR_OPENCL);
                *e = 0;
                return 1;
            }
            return 0;
        }
    }
    return 1;
}

void DeviceCL::flush() const
{
	clFlush( m_commandQueue );
}

int DeviceCL::getNDevices( Config::DeviceType device, DeviceVendor vendor )
{
	int nDevices = 0;
	{
		const int MAX_PLATFORM = 16;

		cl_int status;
		cl_platform_id pIdx[MAX_PLATFORM];
		cl_uint nPlatforms = nPlatforms = getPlatforms( pIdx, MAX_PLATFORM );

		bool platformAMDfound = false;

		for(cl_uint i=0; i<nPlatforms; i++)
		{
			char buff[512];
			cl_platform_id p = pIdx[i];
			status = clGetPlatformInfo( p, CL_PLATFORM_VENDOR, 512, buff, 0 );
			ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

			DeviceVendor deviceVendor = getVendorType( buff );

			if( !(vendor & deviceVendor) )
				continue;

			if ( deviceVendor == VD_AMD )
			{
				if ( !platformAMDfound ) 
				{
					platformAMDfound = true;
				}
				else
				{
					// FIR-841 - sometimes, because of driver install issue.
					// we will have 2 identical AMD platform installed.
					// so if the system has 2 real GPUs then we have 4 GPUs seen by  Api::getDeviceInfo
					// in order to workaround that, we will only take the 1st AMD platform
					// search  FIR-841  in the source code to find every places where this workaround is.
					ADL_LOG("WARNING : the system has more than 1 AMD platform installed\n");
					continue;
				}
			}

			cl_uint n;
			status = clGetDeviceIDs( p, device==Config::DEVICE_GPU?CL_DEVICE_TYPE_GPU:CL_DEVICE_TYPE_CPU, 0, NULL, &n );
			nDevices += n;
		}
	}

	return nDevices;
}

int DeviceCL::getNCUs() const
{
	unsigned int nCUs;
	clGetDeviceInfo( m_deviceIdx, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(unsigned int), &nCUs, 0 );
	return nCUs;
}

unsigned long long DeviceCL::getMemSize() const
{
	cl_ulong size;
	clGetDeviceInfo( m_deviceIdx, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(size), &size, 0 );
	return size;
}

unsigned long long DeviceCL::getMaxAllocationSize() const
{
	return m_maxAllocSize;
}

void DeviceCL::getDeviceName( char nameOut[128] ) const
{
	cl_int status;
	nameOut[0] = 0; // in case of failure, ensure at least to have null terminated
	status = clGetDeviceInfo( m_deviceIdx, CL_DEVICE_NAME, sizeof(char)*128, nameOut, NULL );

	{
		int i = 0;
		for(; i<128; i++)
		{
			if( nameOut[i] != ' ' )
				break;
		}
		if( i!= 0 )
			sprintf( nameOut, "%s", nameOut+i );
/*
		i = 0;
		for(; i<128; i++)
		{
			if( nameOut[i] == ' ' )
			{
				nameOut[i] = '\0';
				break;
			}
		}
*/
	}
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
}

void DeviceCL::getBoardName( char nameOut[128] ) const
{
	cl_int status;
	status = clGetDeviceInfo( m_deviceIdx, 0x4038 /*CL_DEVICE_BOARD_NAME_AMD*/, sizeof(char)*128, nameOut, NULL );

	if( status == CL_SUCCESS )
	{
		int i = 0;
		for(; i<128; i++)
		{
			if( nameOut[i] != ' ' )
				break;
		}
		if( i!= 0 )
			sprintf( nameOut, "%s", nameOut+i );
	}
	else
	{
		nameOut[0] = 0;
	}
}

void DeviceCL::getDeviceVendor( char nameOut[128] ) const
{
	cl_int status;
	status = clGetDeviceInfo( m_deviceIdx, CL_DEVICE_VENDOR, sizeof(char)*128, nameOut, NULL );
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
}

void DeviceCL::getDeviceVersion( char nameOut[128] ) const
{
	cl_int status;
	status = clGetDeviceInfo( m_deviceIdx, CL_DEVICE_VERSION, sizeof(char)*128, nameOut, NULL );

	if( status == CL_SUCCESS )
	{
		int i = 0;
		for(; i<128; i++)
		{
			if( nameOut[i] != ' ' )
				break;
		}
		if( i!= 0 )
			sprintf( nameOut, "%s", nameOut+i );
	}
	else
	{
		nameOut[0] = 0;
	}
}

bool DeviceCL::isKernelBuilt(const char* fileName, const char* funcName, const char* option )const
{
	return m_kernelManager->find( this, fileName, funcName, option );
}

Kernel* DeviceCL::getKernel(const char* fileName, const char* funcName, const char* option, const char** srcList, int nSrc, const char** depsList, int nDeps, bool cacheKernel )const
{
	return m_kernelManager->query( this, fileName, funcName, option, srcList, nSrc, depsList, nDeps, cacheKernel );
}

DeviceVendor DeviceCL::getVendorType( const char* name )
{
	if( strcmp( name, "NVIDIA Corporation" )==0 ) 
		return VD_NV;
	if( strcmp( name, "Advanced Micro Devices, Inc." )==0 ) 
		return VD_AMD;
	if( strcmp( name, "Intel(R) Corporation" )==0 ) 
		return VD_INTEL;
	if( strcmp( name, "Apple" )==0 ) 
		return VD_APPLE;
	return VD_UNKNOWN;
}

uint64_t DeviceCL::getExecutionTimeNanoseconds(const SyncObject * syncObj) const
{
	uint64_t time;
	cl_ulong start, end;
	cl_event* event = (cl_event*)(syncObj->m_ptr);
	int err = clGetEventProfilingInfo(*event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), (void*)&start, nullptr);
	err = clGetEventProfilingInfo(*event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), (void*)&end, nullptr);
	time = (end - start);
	return time;
}

};
