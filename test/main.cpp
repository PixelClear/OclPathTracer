#include <Adl/Adl.h>
#include <gtest/gtest.h>
#include <test/Array.h>
#include <math.h>

using namespace adl;

#define IASSERT(x) EXPECT_TRUE(x)

class DeviceTest : public testing::Test
{
	public:
		static int s_mode;
		static int s_deviceIdx;

	protected:
		virtual void SetUp()
		{
			DeviceType dt;
			if( s_mode == 0 )
				dt = TYPE_CL;
			else if( s_mode == 1 )
				dt = TYPE_METAL;
			else if( s_mode == 2 )
				dt = TYPE_VULKAN;
			adl::init( dt );
			DeviceUtils::Config cfg;
			cfg.m_deviceIdx = s_deviceIdx;
			m_d = DeviceUtils::allocate( dt, cfg );
			IASSERT( m_d != 0 );
		}
		virtual void TearDown()
		{
			DeviceType dt;
			if( s_mode == 0 )
				dt = TYPE_CL;
			else if( s_mode == 1 )
				dt = TYPE_METAL;
			else if( s_mode == 2 )
				dt = TYPE_VULKAN;
			DeviceUtils::deallocate( m_d );

			adl::quit( dt );
		}

	protected:
		Device* m_d;
};

int DeviceTest::s_mode = 0; /*0:ocl, 1:metal, 2:vulkan*/
int DeviceTest::s_deviceIdx = 0;

TEST_F(DeviceTest, initialize)
{
}

TEST_F(DeviceTest, deviceInfo)
{
	char t[128];
	m_d->getDeviceName( t );
	printf("%s\n", t);
	m_d->getBoardName( t );
	printf("%s\n", t);
	m_d->getDeviceVendor( t );
	printf("%s\n", t);
	m_d->getDeviceVersion( t );
	printf("%s\n", t);
	{
		const float allocSize = static_cast<float>(m_d->getMaxAllocationSize()) / (1024.f * 1024.f);
		printf("Max allocation size: %6.3f MB\n", allocSize);
	}
}

/*
TEST_F(DeviceTest, MemoryAllocation)
{
	const adlu64 size = static_cast<adlu64>(m_d->getMaxAllocationSize() * 0.9) / sizeof(int);
	Buffer<int> buffer{ m_d, size };
}

TEST_F(DeviceTest, writeRead)
{
	const int n = 128;
	Array<int> h( n );
	for(int i=0; i<n; i++)
		h[i] = i;
	Buffer<int> a( m_d, n );
	a.write( h.begin(), n );
	DeviceUtils::waitForCompletion( m_d );

	Array<int> ans( n );
	a.read( ans.begin(), n );
	DeviceUtils::waitForCompletion( m_d );
	for(int i=0; i<n; i++)
	{
		IASSERT( ans[i] == h[i] );
	}
}

TEST_F(DeviceTest, getHostPtr)
{
	const int n = 128;
	Buffer<int> a( m_d, n );
	{
		int* h = a.getHostPtr();
		DeviceUtils::waitForCompletion( m_d );
		for(int i=0; i<n; i++)
		{
			h[i] = i;
		}
		a.returnHostPtr( h );
		DeviceUtils::waitForCompletion( m_d );
	}
	{
		int* h = a.getHostPtr();
		DeviceUtils::waitForCompletion( m_d );
		for(int i=0; i<n; i++)
		{
			if( h[i] != i )
			{
				printf("error %d, %d\n", h[i], i);
				IASSERT(0);
			}
		}
		a.returnHostPtr( h );
		DeviceUtils::waitForCompletion( m_d );
	}
}

TEST_F(DeviceTest, kernelExecution)
{
	const int n = 128;
	Array<int> h( n );
	Buffer<int> a( m_d, n );
	{
		BufferInfo bInfo[] = { BufferInfo( &a ) };
		Launcher launcher( m_d, m_d->getKernel( SELECT_KERNELPATH1( m_d, "../test/", "TestKernel" ), "FillKernel", "-I ../" ) );
		launcher.setBuffers( bInfo, sizeof(bInfo)/sizeof(BufferInfo) );
		launcher.setConst( n );
		launcher.launch1D( n );
	}
	DeviceUtils::waitForCompletion( m_d );

	Array<int> ans( n );
	a.read( ans.begin(), n );
	DeviceUtils::waitForCompletion( m_d );
	for(int i=0; i<n; i++)
	{
		IASSERT( ans[i] == i );
	}
}
*/

int main(int argc, char* argv[])
{
	if( argc >= 2 )
	{
		if( strcmp( argv[1], "metal" ) == 0 )
		{
			DeviceTest::s_mode = 1;
			DeviceTest::s_deviceIdx = 0;
		}
		else if( strcmp( argv[1], "vulkan" ) == 0 )
		{
			DeviceTest::s_mode = 2;
			DeviceTest::s_deviceIdx = 0;
		}
		else
		{
			DeviceTest::s_mode = 0;
			DeviceTest::s_deviceIdx = 0;
		}
	}

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
