#include <Adl/Adl.h>

namespace adl
{

int DeviceCL::s_refCount = 0;
char s_cacheDirectory[128] = "cache";

#if !defined(CL_QUEUE_THREAD_HANDLE_AMD)
	#define CL_QUEUE_THREAD_HANDLE_AMD                  0x403E
#endif

void DeviceCL::setThreadPriority( bool isHigh )
{
	if( m_vendor != VD_AMD )
		return;
#if defined(WIN32)
	adlu64 value;
	size_t a = sizeof(value);
	size_t b = 0;
	cl_int e = clGetCommandQueueInfo( m_commandQueue, CL_QUEUE_THREAD_HANDLE_AMD, a, &value, &b );
	ADLASSERT( e == CL_SUCCESS, 0 );

	int error = SetThreadPriority( (HANDLE)value, (isHigh)?THREAD_PRIORITY_HIGHEST : THREAD_PRIORITY_NORMAL );
	if( error == 0 )
	{
		ADL_LOG("Set OCL queue priority failed\n");
	}
	else
	{
		ADL_LOG("Set OCL queue priority high\n");
	}

	adlu64 mask = 1 << 0;
	SetThreadAffinityMask( (HANDLE)value, mask );
#endif
}

bool init( DeviceType type )
{
	switch( type )
	{
#if defined(ADL_ENABLE_CL)
	case TYPE_CL:
		return DeviceCL::init();
#endif
#if defined(ADL_ENABLE_DX11)
	case TYPE_DX11:
		return false;
#endif
#if defined(ADL_ENABLE_METAL)
	case TYPE_METAL:
		return true;
#endif
	default:
		return false;
	};
}

void quit( DeviceType type )
{
	// \todo Fix me
	type = TYPE_VULKAN;
	switch( type )
	{
#if defined(ADL_ENABLE_CL)
	case TYPE_CL:
		return DeviceCL::quit();
#endif
#if defined(ADL_ENABLE_DX11)
	case TYPE_DX11:
		return;
#endif
#if defined(ADL_ENABLE_METAL)
	case TYPE_METAL:
		return;
#endif
	default:
		return;
	};
}

int DeviceUtils::getNDevices( DeviceType type, Config::DeviceType device, DeviceVendor vendor )
{
	// \todo Fix me
	type = TYPE_VULKAN;
	switch( type )
	{
#if defined(ADL_ENABLE_CL)
	case TYPE_CL:
		return DeviceCL::getNDevices( device, vendor );
#endif
#if defined(ADL_ENABLE_DX11)
	case TYPE_DX11:
		return DeviceDX11::getNDevices();
#endif
#if defined(ADL_ENABLE_METAL)
	case TYPE_METAL:
		return DeviceMetal::getNDevices( device, vendor );
#endif
	default:
		return 1;
	};
}

struct SIMDTable
{
	SIMDTable(const char* n0, const char* n1, int n):m_n(n){ m_names[0] = n0; m_names[1] = n1; }

	enum
	{
		MAX_DEVICE_TYPE = 2,
	};

	const char *m_names[MAX_DEVICE_TYPE];
	int m_n;
};

static
SIMDTable s_tables[] = {
	SIMDTable( "Cayman", "AMD Radeon HD 6900 Series", 24 ),
	SIMDTable( "Cypress", "ATI Radeon HD 5800 Series", 20 ),
};


int DeviceUtils::getNCUs( const Device* device )
{
	if( device->getType() == TYPE_CL )
	{
		return ((const DeviceCL*)device)->getNCUs();
	}
#if defined(ADL_ENABLE_METAL)
	else if( device->getType() == TYPE_METAL )
	{
		// TODO do what here?
	}
#endif

	char name[128];
	device->getDeviceName( name );

	//	todo. use clGetDeviceInfo, CL_DEVICE_MAX_COMPUTE_UNITS for OpenCL

	int n =sizeof(s_tables)/sizeof(SIMDTable);
	int nSIMD = -1;
	for(int i=0; i<n; i++)
	{
		for(int j=0; j<SIMDTable::MAX_DEVICE_TYPE; j++)
		{
			if( strstr( s_tables[i].m_names[j], name ) != 0 )
			{
				nSIMD = s_tables[i].m_n;
				return nSIMD;
			}
		}
	}
	return 20;
}

Device* DeviceUtils::allocate( DeviceType type, Config cfg )
{
	Device* deviceData;
	switch( type )
	{
#if defined(ADL_ENABLE_CL)
	case TYPE_CL:
		deviceData = new DeviceCL();
		break;
#endif
#if defined(ADL_ENABLE_DX11)
	case TYPE_DX11:
		deviceData = new DeviceDX11();
		break;
#endif
#if defined(ADL_ENABLE_METAL)
	case TYPE_METAL:
		deviceData = new DeviceMetal();
		break;
#endif
#if defined(ADL_ENABLE_VULKAN)
	case TYPE_VULKAN:
		deviceData = new DeviceVulkan();
		break;
#endif
	case TYPE_HOST:
		deviceData = new DeviceHost();
		break;
	default:
		return 0;
	};
	deviceData->m_interopAvailable = 0;
	deviceData->initialize( cfg );
	
	if( cfg.m_compileCallback.callback != NULL )
		deviceData->setCompileCallback( cfg.m_compileCallback );

	return deviceData;
}

void DeviceUtils::deallocate( Device* deviceData )
{
	if(!deviceData->m_ownedByMiRender)
	{
		ADLASSERT( deviceData->getUsedMemory() == 0, TH_ERROR_PARAMETER );
		deviceData->release();
		delete deviceData;
	}
}

void DeviceUtils::waitForCompletion( const Device* deviceData )
{
	deviceData->waitForCompletion();
}

void DeviceUtils::waitForCompletion( const SyncObject* syncObj )
{
	if( syncObj )
		syncObj->m_device->waitForCompletion( syncObj );
}

bool DeviceUtils::isComplete( const SyncObject* syncObj )
{
	bool ans = 1;
	if( syncObj )
		ans = syncObj->m_device->isComplete( syncObj );
	return ans;
}

void DeviceUtils::flush( const Device* device )
{
	device->flush();
}


void defaultlog( const char* txt )
{
	printf( "%s\n", txt );
}

};
