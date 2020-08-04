/*
		2011 Takahiro Harada
*/

namespace adl
{

struct DeviceHost : public Device
{
	DeviceHost() : Device( TYPE_HOST ), m_threads(0){}

	__inline
	void initialize(const Config& cfg);
	__inline
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
	void copy(T* dst, const Buffer<T>* src, adlu64 nElems, adlu64 offsetNElems = 0, SyncObject* syncObj = 0);

	template<typename T>
	__inline
	void copy(Buffer<T>* dst, const T* src, adlu64 nElems, adlu64 offsetNElems = 0, SyncObject* syncObj = 0);

	template<typename T>
	__inline
	void clear(const Buffer<T>* const src) const;

	template<typename T>
	__inline
	void fill(const Buffer<T>* const src, void* pattern, int patternSize) const;

	template<typename T>
	__inline
	void getHostPtr(const Buffer<T>* const src, adlu64 size, T*& ptr, bool blocking = false) const{ptr = src->m_ptr;}

	template<typename T>
	__inline
	void returnHostPtr(const Buffer<T>* const src, T* ptr) const{}

	__inline
	void waitForCompletion() const;

	__inline
	void waitForCompletion( const SyncObject* syncObj ) const {}

    __inline
    bool isComplete( const SyncObject* syncObj ) const{ return 1; }
    
	__inline
	void flush() const;

	__inline
	void allocateSyncObj( void*& ptr ) const {}

	__inline
	void deallocateSyncObj( void* ptr ) const {}

	uint64_t getExecutionTimeNanoseconds(const SyncObject * syncObj) const
	{
		return (uint64_t)(-1);
	}

	void* m_threads;
};

void DeviceHost::initialize(const Config& cfg)
{

}

void DeviceHost::release()
{

}

template<typename T>
void DeviceHost::allocate(Buffer<T>* buf, adlu64 nElems, BufferBase::BufferType type)
{
	buf->m_device = this;

	if( type == BufferBase::BUFFER_CONST ) return;

	buf->m_ptr = new T[nElems];
	ADLASSERT( buf->m_ptr != 0, TH_ERROR_NULLPTR );
	buf->m_size = nElems;
}

template<typename T>
void DeviceHost::deallocate(Buffer<T>* buf)
{
	if( buf->m_ptr ) delete [] buf->m_ptr;
}

template<typename T>
void DeviceHost::copy(Buffer<T>* dst, const Buffer<T>* src, adlu64 nElems, adlu64 dstOffsetNElems, SyncObject* syncObj)
{
	ADLASSERT( src->getType() == TYPE_HOST, TH_ERROR_NODETYPE );
	copy( dst, src->m_ptr, nElems, dstOffsetNElems );
}

template<typename T>
void DeviceHost::copy(T* dst, const Buffer<T>* src, adlu64 nElems, adlu64 srcOffsetNElems, SyncObject* syncObj)
{
	ADLASSERT( src->getType() == TYPE_HOST, TH_ERROR_NODETYPE);
	memcpy( dst, src->m_ptr+srcOffsetNElems, nElems*sizeof(T) );
}

template<typename T>
void DeviceHost::copy(Buffer<T>* dst, const T* src, adlu64 nElems, adlu64 dstOffsetNElems, SyncObject* syncObj)
{
	ADLASSERT( dst->getType() == TYPE_HOST, TH_ERROR_NODETYPE);
	memcpy( dst->m_ptr+dstOffsetNElems, src, nElems*sizeof(T) );
}

template<typename T>
void DeviceHost::clear(const Buffer<T>* const src) const
{
	char* ptr = (char*)src->m_ptr;
	for(int i=0; i<src->getSize(); i++)
	    ptr[i] = 0;
}

template<typename T>
void DeviceHost::fill(const Buffer<T>* const src, void* pattern, int patternSize) const
{
	ADLASSERT( src->getSize()%patternSize == 0, TH_ERROR_PARAMETER );

	char* ptr = (char*)src->m_ptr;
	for(int i=0; i<src->getSize()/patternSize; i++)
	{
		memcpy( ptr, pattern, patternSize );

		ptr += patternSize;
	}
}

void DeviceHost::waitForCompletion() const
{

}

void DeviceHost::flush() const
{

}

};
