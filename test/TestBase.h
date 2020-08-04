#pragma once

#include <Adl/Adl.h>
#include <gtest/gtest.h>
#include <test/Array.h>
#include <math.h>

using namespace adl;

#define IASSERT(x) EXPECT_TRUE(x)


class DeviceTest : public testing::Test
{
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

		void getFilePath( const char* prefix, const char* ext,
			char* dst )
		{
			char t[256];
			m_d->getDeviceVersion( t );
			sprintf( dst, "%s_%s.%s", prefix, t, ext );
		}

	protected:
		Device* m_d;
	public:
		static int s_mode; /*0:ocl, 1:metal, 2:vulkan*/
		static int s_deviceIdx;
};