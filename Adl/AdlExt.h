#pragma once

namespace adl
{

class CLUtils
{
public:
	template<typename T>
	static
	void acquireGLBuffer( Buffer<T>* buff )
	{
		if( !buff ) return;

		DeviceCL* cl = (DeviceCL*)buff->m_device;
		cl_int e;
		e = clEnqueueAcquireGLObjects( cl->m_commandQueue, 1, (cl_mem*)&buff->m_ptr, 0, 0, 0 );
		ADLASSERT( e == CL_SUCCESS, TH_ERROR_OPENCL);
	}

	template<typename T>
	static
	void releaseGLBuffer( Buffer<T>* buff )
	{
		if( !buff ) return;

		DeviceCL* cl = (DeviceCL*)buff->m_device;
		cl_int e;
		e = clEnqueueReleaseGLObjects( cl->m_commandQueue, 1, (cl_mem*)&buff->m_ptr, 0, 0, 0 );
		ADLASSERT( e == CL_SUCCESS, TH_ERROR_OPENCL);
	}
};

};
