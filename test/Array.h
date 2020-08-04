/*
		2011 Takahiro Harada
*/

#ifndef ARRAY_H
#define ARRAY_H

#include <string.h>
#if defined(__APPLE__)
	#include <sys/malloc.h>
#else
	#include <malloc.h>
#endif

#ifdef __LINUX__
	#define max2 std::max
#endif

//namespace Tahoe
//{

typedef unsigned int u32;
typedef unsigned long long u64;

template <typename T, u32 DEFAULT_SIZE = 128>
class Array
{
	public:
		__inline
		Array();
		__inline
		Array(u64 size);
		__inline
		virtual ~Array();
		__inline
		T& operator[] (u64 idx);
		__inline
		const T& operator[] (u64 idx) const;
		__inline
		void pushBack(const T& elem);
		__inline
		T pop();
		__inline
		void popBack();
		__inline
		void clear();
		__inline
		void setSize(u64 size);
		__inline
		bool isEmpty() const;
		__inline
		u64 getSize() const;
		__inline
		T* begin();
		__inline
		const T* begin() const;
		__inline
		T* end();
		__inline
		const T* end() const;
		__inline
		u64 indexOf(const T& data) const;
		__inline
		void removeAt(u64 idx);
		__inline
		T& expandOne();


		__inline
		void* alignedMalloc(size_t size, u32 alignment)
		{
			return new char[size];
		}
		__inline
		void alignedFree( void* ptr )
		{
			delete [] (char*)ptr;
		}


	private:
		Array(const Array& a){}

	protected:
		enum
		{
			INCREASE_SIZE = 128,
		};

		T* m_data;
		u64 m_size;
		u64 m_capacity;
};

template <typename T, u32 DEFAULT_SIZE>
Array<T, DEFAULT_SIZE>::Array()
{
	m_size = 0;
	m_capacity = DEFAULT_SIZE;
	m_data = (T*)alignedMalloc(sizeof(T)*m_capacity, 16);

	if (m_data == NULL)
	{
		m_size = m_capacity = 0;
		return;
	}
	for(u64 i=0; i<m_capacity; i++) new(&m_data[i])T;
}

template <typename T, u32 DEFAULT_SIZE>
Array<T, DEFAULT_SIZE>::Array(u64 size)
{
	m_size = size;
	m_capacity = size;
	m_data = (T*)alignedMalloc(sizeof(T)*m_capacity, 16);

	if (m_data == NULL)
	{
		m_size = m_capacity = 0;
		return;
	}

	for(u64 i=0; i<m_capacity; i++) new(&m_data[i])T;
}

template <typename T, u32 DEFAULT_SIZE>
Array<T,DEFAULT_SIZE>::~Array()
{
	if( m_data )
	{
		delete [] m_data;
		m_data = NULL;
	}
}

template <typename T, u32 DEFAULT_SIZE>
T& Array<T,DEFAULT_SIZE>::operator[](u64 idx)
{
	ADLASSERT( 0 <= idx, TH_ERROR_PARAMETER );
	ADLASSERT(idx<m_size, TH_ERROR_PARAMETER);
	return m_data[idx];
}

template <typename T, u32 DEFAULT_SIZE>
const T& Array<T,DEFAULT_SIZE>::operator[](u64 idx) const
{
	ADLASSERT( 0 <= idx, TH_ERROR_PARAMETER);
	ADLASSERT(idx<m_size, TH_ERROR_PARAMETER);
	return m_data[idx];
}

template <typename T, u32 DEFAULT_SIZE>
void Array<T,DEFAULT_SIZE>::pushBack(const T& elem)
{
	if( m_size == m_capacity )
	{
		u64 s = m_size;
		setSize( 2*max2( (u64)1, m_capacity ) );
		m_size = s;
	}
	m_data[ m_size++ ] = elem;
}

template <typename T, u32 DEFAULT_SIZE>
T Array<T,DEFAULT_SIZE>::pop()
{
	ADLASSERT( m_size>0, TH_ERROR_PARAMETER);
	return m_data[--m_size];
}

template <typename T, u32 DEFAULT_SIZE>
void Array<T,DEFAULT_SIZE>::popBack()
{
	ADLASSERT( m_size>0, TH_ERROR_PARAMETER);
	m_size--;
}

template <typename T, u32 DEFAULT_SIZE>
void Array<T,DEFAULT_SIZE>::clear()
{
	m_size = 0;
}

template <typename T, u32 DEFAULT_SIZE>
void Array<T,DEFAULT_SIZE>::setSize(u64 size)
{
	if( size > m_capacity || size == 0 )
	{
		u64 oldCap = m_capacity;
		u64 capacity = (size==0)? 1:max2(size, m_capacity * 2);
		T* s = (T*)alignedMalloc(sizeof(T)*capacity, 16);
		
		if (s == NULL) 
		{
			if( m_data )
				alignedFree( m_data );
			m_capacity = 0;
			m_size = 0;
			m_data = NULL;
			return;
		}
		m_capacity = capacity;
//		for(int i=0; i<m_capacity; i++) new(&s[i])T;
		if( m_data )
		{
			memcpy( s, m_data, sizeof(T)*std::min(oldCap,capacity) );
			if (m_data) alignedFree( m_data );
		}
		m_data = s;
	}
	m_size = size;
}

template <typename T, u32 DEFAULT_SIZE>
bool Array<T,DEFAULT_SIZE>::isEmpty() const
{
	return m_size==0;
}

template <typename T, u32 DEFAULT_SIZE>
u64 Array<T,DEFAULT_SIZE>::getSize() const
{
	return m_size;
}

template <typename T, u32 DEFAULT_SIZE>
const T* Array<T,DEFAULT_SIZE>::begin() const
{
	return m_data;
}

template <typename T, u32 DEFAULT_SIZE>
T* Array<T,DEFAULT_SIZE>::begin()
{
	return m_data;
}

template <typename T, u32 DEFAULT_SIZE>
T* Array<T,DEFAULT_SIZE>::end()
{
	return m_data+m_size;
}

template <typename T, u32 DEFAULT_SIZE>
const T* Array<T,DEFAULT_SIZE>::end() const
{
	return m_data+m_size;
}

template <typename T, u32 DEFAULT_SIZE>
u64 Array<T,DEFAULT_SIZE>::indexOf(const T& data) const
{
	for(u64 i=0; i<m_size; i++)
	{
		if( data == m_data[i] ) return i;
	}
	return -1;
}

template <typename T, u32 DEFAULT_SIZE>
void Array<T,DEFAULT_SIZE>::removeAt(u64 idx)
{
	ADLASSERT(idx<m_size, TH_ERROR_PARAMETER);
	m_data[idx] = m_data[--m_size];
//	ADLASSERT( 0, TH_ERROR_INTERNAL );
}

template <typename T, u32 DEFAULT_SIZE>
T& Array<T,DEFAULT_SIZE>::expandOne()
{
	setSize( m_size+1 );
	new( &m_data[ m_size-1 ] )T;
	return m_data[ m_size-1 ];
}

//};

#endif

