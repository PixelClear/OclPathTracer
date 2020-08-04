/*
		2011 Takahiro Harada
*/

namespace adl
{


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

	#define SELECT_DEVICEDATA1( deviceData, func ) \
		switch( deviceData->m_type ) \
		{ \
		case TYPE_CL: ((DeviceCL*)deviceData)->func; break; \
		case TYPE_DX11: ((DeviceDX11*)deviceData)->func; break; \
		case TYPE_HOST: ((DeviceHost*)deviceData)->func; break; \
		default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
		}
	#else
	#define SELECT_DEVICEDATA( type, func ) \
		switch( type ) \
		{ \
		case TYPE_DX11: ((DeviceDX11*)m_device)->func; break; \
		case TYPE_HOST: ((DeviceHost*)m_device)->func; break; \
		default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
		}

	#define SELECT_DEVICEDATA1( deviceData, func ) \
		switch( deviceData->m_type ) \
		{ \
		case TYPE_DX11: ((DeviceDX11*)deviceData)->func; break; \
		case TYPE_HOST: ((DeviceHost*)deviceData)->func; break; \
		default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
		}
	#endif
#else
	#if defined(ADL_ENABLE_CL)
		#if defined(ADL_ENABLE_METAL)
			#define SELECT_DEVICEDATA( type, func ) \
					switch( type ) \
					{ \
					case TYPE_CL: ((DeviceCL*)m_device)->func; break; \
					case TYPE_HOST: ((DeviceHost*)m_device)->func; break; \
					case TYPE_METAL: ((DeviceMetal*)m_device)->func; break; \
					default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
					}

			#define SELECT_DEVICEDATA1( deviceData, func ) \
					switch( deviceData->m_type ) \
					{ \
					case TYPE_CL: ((DeviceCL*)deviceData)->func; break; \
					case TYPE_HOST: ((DeviceHost*)deviceData)->func; break; \
					case TYPE_METAL: ((DeviceMetal*)deviceData)->func; break; \
					default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
					}
		#elif defined(ADL_ENABLE_VULKAN)
			#define SELECT_DEVICEDATA( type, func ) \
				switch( type ) \
				{ \
				case TYPE_CL: ((DeviceCL*)m_device)->func; break; \
				case TYPE_HOST: ((DeviceHost*)m_device)->func; break; \
				case TYPE_VULKAN: ((DeviceVulkan*)m_device)->func; break; \
				default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
				}

			#define SELECT_DEVICEDATA1( deviceData, func ) \
				switch( deviceData->m_type ) \
				{ \
				case TYPE_CL: ((DeviceCL*)deviceData)->func; break; \
				case TYPE_HOST: ((DeviceHost*)deviceData)->func; break; \
				case TYPE_VULKAN: ((DeviceVulkan*)deviceData)->func; break; \
				default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
				}
		#else
			#define SELECT_DEVICEDATA( type, func ) \
				switch( type ) \
				{ \
				case TYPE_CL: ((DeviceCL*)m_device)->func; break; \
				case TYPE_HOST: ((DeviceHost*)m_device)->func; break; \
				default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
				}

			#define SELECT_DEVICEDATA1( deviceData, func ) \
				switch( deviceData->m_type ) \
				{ \
				case TYPE_CL: ((DeviceCL*)deviceData)->func; break; \
				case TYPE_HOST: ((DeviceHost*)deviceData)->func; break; \
				default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
				}
		#endif
	#else
	#define SELECT_DEVICEDATA( type, func ) \
		switch( type ) \
		{ \
		case TYPE_HOST: ((DeviceHost*)m_device)->func; break; \
		default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
		}

	#define SELECT_DEVICEDATA1( deviceData, func ) \
		switch( deviceData->m_type ) \
		{ \
		case TYPE_HOST: ((DeviceHost*)deviceData)->func; break; \
		default: ADLASSERT(0, TH_ERROR_INTERNAL); break; \
		}
	#endif
#endif
	
	
#if defined(ADL_ENABLE_METAL)
	#define SELECT_KERNELPATH1(device, base, name) (device->m_type == adl::TYPE_METAL ? base "MtlKernels/" name : base "ClKernels/" name)
	#define SELECT_KERNELARGS(device, name) (device->m_type == adl::TYPE_METAL ? metal::name : cl::name), (device->m_type == adl::TYPE_METAL ? sizeof(metal::name) / sizeof(void*) : sizeof(cl::name) / sizeof(void*))
#elif defined(ADL_ENABLE_VULKAN)
	#define SELECT_KERNELPATH1(device, base, name) (device->m_type == adl::TYPE_VULKAN ? base "ClKernels/" name : base "ClKernels/" name)
	#define SELECT_KERNELARGS(device, name) (device->m_type == adl::TYPE_VULKAN ? cl::name : cl::name), (device->m_type == adl::TYPE_VULKAN ? sizeof(cl::name) / sizeof(void*) : sizeof(cl::name) / sizeof(void*))
#else
	#define SELECT_KERNELPATH1(device, base, name) (base "ClKernels/" name)
	#define SELECT_KERNELARGS(device, name) (cl::name), (sizeof(cl::name) / sizeof(void*))
#endif	
#define SELECT_KERNELDEPS(device, name) SELECT_KERNELARGS(device, name)

template<typename T>
Buffer<T>::Buffer()
{
	m_device = 0;
	m_size = 0;
	m_ptr = 0;

	m_dx11.m_uav = 0;
	m_dx11.m_srv = 0;

	m_cl.m_hostPtr = 0;

	m_allocated = false;
}

template<typename T>
Buffer<T>::Buffer(const Device* deviceData, adlu64 nElems, BufferType type )
{
	m_device = 0;
	m_allocated = false;
	allocate( deviceData, nElems, type );
}

template<typename T>
Buffer<T>::~Buffer()
{
	if( m_allocated )
	{
		if( m_device )
			SELECT_DEVICEDATA( m_device->m_type, deallocate( this ) );
	}

	m_device = 0;
	m_ptr = 0;
	m_size = 0;
}

template<typename T>
void Buffer<T>::setRawPtr(const Device* device, T* ptr, adlu64 size, BufferType type)
{
	//	ADLASSERT( m_device == 0, TH_ERROR_NULLPTR );
	ADLASSERT(type == BUFFER, TH_ERROR_PARAMETER);	//	todo. implement
	ADLASSERT(device->m_type != TYPE_DX11, TH_ERROR_PARAMETER);	//	todo. implement set srv, uav

	if (m_device)
	{
		ADLASSERT(m_device == device, TH_ERROR_PARAMETER);
	}

	m_device = device;
	m_ptr = ptr;
	m_size = size;
}


template<typename T>
void Buffer<T>::allocate(const Device* deviceData, adlu64 nElems, BufferType type )
{
	ADLASSERT( m_device == 0, TH_ERROR_NULLPTR);
	m_device = deviceData;
	m_size = 0;
	m_ptr = 0;

	m_dx11.m_uav = 0;
	m_dx11.m_srv = 0;
	m_cl.m_hostPtr = 0;
	if( nElems )
	{
		SELECT_DEVICEDATA( m_device->m_type, allocate( this, nElems, type ) );
		m_allocated = true;
	}
}

template<typename T>
void Buffer<T>::write(const T* hostPtr, adlu64 nElems, adlu64 offsetNElems, SyncObject* syncObj)
{
	if( nElems==0 ) return;
	ADLASSERT( nElems+offsetNElems <= m_size, TH_ERROR_PARAMETER);
	SELECT_DEVICEDATA( m_device->m_type, copy(this, (T*)hostPtr, nElems, offsetNElems, syncObj) );
}

template<typename T>
void Buffer<T>::read(T* hostPtr, adlu64 nElems, adlu64 offsetNElems, SyncObject* syncObj) const
{
	if( nElems==0 ) return;
	SELECT_DEVICEDATA( m_device->m_type, copy(hostPtr,this, nElems, offsetNElems, syncObj) );
}

template<typename T>
void Buffer<T>::write(const Buffer<T>& src, adlu64 nElems, adlu64 dstOffsetNElems, SyncObject* syncObj)
{
	if( nElems==0 ) return;
	ADLASSERT( nElems <= m_size, TH_ERROR_PARAMETER);
	SELECT_DEVICEDATA( m_device->m_type, copy(this, &src, nElems, dstOffsetNElems, syncObj) );
}

template<typename T>
void Buffer<T>::read(Buffer<T>& dst, adlu64 nElems, adlu64 offsetNElems, SyncObject* syncObj) const
{
	ADLASSERT( nElems <= m_size, TH_ERROR_PARAMETER);
	ADLASSERT( offsetNElems == 0, TH_ERROR_PARAMETER);
	if( nElems==0 ) return;
	SELECT_DEVICEDATA( m_device->m_type, copy(&dst, this, nElems, offsetNElems, syncObj) );
}

template<typename T>
void Buffer<T>::clear()
{
	SELECT_DEVICEDATA( m_device->m_type, clear(this) );
}

template<typename T>
void Buffer<T>::fill(void* pattern, int patternSize)
{
	SELECT_DEVICEDATA( m_device->m_type, fill(this, pattern, patternSize) );
}

template<typename T>
T* Buffer<T>::getHostPtr(adlu64 size, bool blocking) const
{
	T* ptr;
	SELECT_DEVICEDATA( m_device->m_type, getHostPtr(this, size, ptr, blocking) );
	return ptr;
}

template<typename T>
void Buffer<T>::returnHostPtr(T* ptr) const
{
	SELECT_DEVICEDATA( m_device->m_type, returnHostPtr(this, ptr) );
}

template<typename T>
void Buffer<T>::setSize( adlu64 size )
{
	ADLASSERT( m_device != 0, TH_ERROR_NULLPTR);
	if( !m_allocated )
	{
//		ADLASSERT( m_ptr == 0, TH_ERROR_NULLPTR);
		if( size==0 ) return;
		SELECT_DEVICEDATA( m_device->m_type, allocate( this, size, BUFFER ) );
		m_allocated = true;
	}
	else
	{
		//	todo. add capacity to member
		if( getSize() < size )
		{
			ADLASSERT( m_allocated, TH_ERROR_PARAMETER);

			{
				const Device* d = m_device;
				SELECT_DEVICEDATA( m_device->m_type, deallocate( this ) );
				DeviceUtils::waitForCompletion( d );
				allocate( d, size );
			}
		}
	}
}




/*
template<typename T>
Buffer<T>& Buffer<T>::operator = ( const Buffer<T>& buffer )
{
//	ADLASSERT( buffer.m_size <= m_size, TH_ERROR_PARAMETER );

	SELECT_DEVICEDATA( m_device->m_type, copy(this, &buffer, min2( m_size, buffer.m_size) ) );

	return *this;
}
*/

template<DeviceType TYPE, bool COPY, typename T>
__inline
//static
typename adl::Buffer<T>* BufferUtils::map(const Device* device, const Buffer<T>* in, int copySize)
{
	Buffer<T>* native;
	ADLASSERT( device->m_type == TYPE, TH_ERROR_PARAMETER);

	if( in->getType() == TYPE )
		native = (Buffer<T>*)in;
	else
	{
		ADLASSERT( copySize <= in->getSize(), TH_ERROR_PARAMETER);
		copySize = (copySize==-1)? in->getSize() : copySize;

		if( TYPE == TYPE_HOST )
		{
			native = new Buffer<T>;
			native->m_device = device;
			native->m_ptr = in->getHostPtr( copySize );
			native->m_size = copySize;
		}
		else
		{
			native = new Buffer<T>( device, copySize );
			if( COPY )
			{
				if( in->getType() == TYPE_HOST )
					native->write( in->m_ptr, copySize );
	//			else if( native->getType() == TYPE_HOST )
	//			{
	//				in->read( native->m_ptr, copySize );
	//				DeviceUtils::waitForCompletion( in->m_device );
	//			}
				else
				{
					T* tmp = new T[copySize];
					in->read( tmp, copySize );
					DeviceUtils::waitForCompletion( in->m_device );
					native->write( tmp, copySize );
					DeviceUtils::waitForCompletion( native->m_device );
					delete [] tmp;
				}
			}
		}
	}
	return native;
}

template<bool COPY, typename T>
__inline
//static
void BufferUtils::unmap( Buffer<T>* native, const Buffer<T>* orig, int copySize )
{
	if( native != orig )
	{
		if( native->getType() == TYPE_HOST )
		{
//				Buffer<T>* dst = (Buffer<T>*)orig;
//				dst->write( native->m_ptr, copySize );
//				DeviceUtils::waitForCompletion( dst->m_device );
			orig->returnHostPtr( native->m_ptr );
		}
		else
		{
			if( COPY ) 
			{
				copySize = (copySize==-1)? min2(orig->getSize(), native->getSize()) : copySize;
				ADLASSERT( copySize <= orig->getSize(), TH_ERROR_PARAMETER);
				if( orig->getType() == TYPE_HOST )
				{
					native->read( orig->m_ptr, copySize );
					DeviceUtils::waitForCompletion( native->m_device );
				}
				else
				{
					T* tmp = new T[copySize];
					native->read( tmp, copySize );
					DeviceUtils::waitForCompletion( native->m_device );
					Buffer<T>* dst = (Buffer<T>*)orig;
					dst->write( tmp, copySize );
					DeviceUtils::waitForCompletion( dst->m_device );
					delete [] tmp;
				}
			}
			DeviceUtils::waitForCompletion( native->m_device );
		}
		delete native;
	}
}

template<DeviceType TYPE, bool COPY, typename T>
__inline
//static
typename adl::Buffer<T>* BufferUtils::mapInplace(const Device* device, Buffer<T>* allocatedBuffer, const Buffer<T>* in, int copySize)
{
	Buffer<T>* native;
	ADLASSERT( device->m_type == TYPE, TH_ERROR_PARAMETER);

	if( in->getType() == TYPE )
		native = (Buffer<T>*)in;
	else
	{
		ADLASSERT( copySize <= in->getSize(), TH_ERROR_PARAMETER);
		copySize = (copySize==-1)? min2( in->getSize(), allocatedBuffer->getSize() ) : copySize;

		native = allocatedBuffer;
		if( COPY )
		{
			if( in->getType() == TYPE_HOST )
				native->write( in->m_ptr, copySize );
			else if( native->getType() == TYPE_HOST )
			{
				in->read( native->m_ptr, copySize );
				DeviceUtils::waitForCompletion( in->m_device );
			}
			else
			{
				T* tmp = new T[copySize];
				in->read( tmp, copySize );
				DeviceUtils::waitForCompletion( in->m_device );
				native->write( tmp, copySize );
				DeviceUtils::waitForCompletion( native->m_device );
				delete [] tmp;
			}
		}
	}
	return native;
}

template<bool COPY, typename T>
__inline
//static
void BufferUtils::unmapInplace( Buffer<T>* native, const Buffer<T>* orig, int copySize)
{
	if( native != orig )
	{
		if( COPY ) 
		{
			copySize = (copySize==-1)? min2(orig->getSize(), native->getSize()) : copySize;
			ADLASSERT( copySize <= orig->getSize(), TH_ERROR_PARAMETER);
			if( orig->getType() == TYPE_HOST )
			{
				native->read( orig->m_ptr, copySize );
				DeviceUtils::waitForCompletion( native->m_device );
			}
			else if( native->getType() == TYPE_HOST )
			{
				Buffer<T>* dst = (Buffer<T>*)orig;
				dst->write( native->m_ptr, copySize );
				DeviceUtils::waitForCompletion( dst->m_device );
			}
			else
			{
				T* tmp = new T[copySize];
				native->read( tmp, copySize );
				DeviceUtils::waitForCompletion( native->m_device );
				Buffer<T>* dst = (Buffer<T>*)orig;
				dst->write( tmp, copySize );
				DeviceUtils::waitForCompletion( dst->m_device );
				delete [] tmp;
			}
		}
//			delete native;
	}
}

template<typename T>
T& HostBuffer<T>::operator[](int idx)
{
	return Buffer<T>::m_ptr[idx];
}

template<typename T>
const T& HostBuffer<T>::operator[](int idx) const
{
	return Buffer<T>::m_ptr[idx];
}

template<typename T>
HostBuffer<T>& HostBuffer<T>::operator = ( const Buffer<T>& device )
{
	ADLASSERT( device.m_size <= Buffer<T>::m_size, TH_ERROR_PARAMETER);

	SELECT_DEVICEDATA1( device.m_device, copy( Buffer<T>::m_ptr, &device, device.m_size ) );

	return *this;
}

#undef SELECT_DEVICEDATA
#undef SELECT_DEVICEDATA1

};
