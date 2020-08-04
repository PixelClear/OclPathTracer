/*
		2011 Takahiro Harada
*/

#include <map>
#include <string>
#include <fstream>

//#define SUPPORT_INDIRECT_DISPATCH 1//defined(ADL_ENABLE_METAL)//technically DX11 can support this

namespace adl
{

//==========================
//	Kernel
//==========================
struct Kernel
{
	DeviceType m_type;
	void* m_kernel;
	const char* m_funcName;
};

//==========================
//	KernelManager
//==========================
class KernelManager
{
	public:
		typedef std::map<std::string, Kernel*> KMap;

		~KernelManager();

		void reset();

		Kernel* query(const Device* dd, const char* fileName, const char* funcName, const char* option = NULL, const char** srcList = NULL, int nSrc = 0, 
			const char** depsList = NULL, int nDeps = 0, 
			bool cacheKernel = true);
		bool find( const Device* dd, const char* fileName, const char* funcName, const char* option );

	public:
		KMap m_map;	// Contains all the compiled kernels
};

struct SyncObject
{
	__inline
	SyncObject( const Device* device );
	__inline
	~SyncObject();

	const Device* m_device;
	void* m_ptr;
};

//==========================
//	Launcher
//==========================
struct BufferInfo
{
	BufferInfo(){}
	template<typename T>
	BufferInfo(Buffer<T>* buff, bool isReadOnly = false): m_buffer(buff), m_isReadOnly(isReadOnly){}
	template<typename T>
	BufferInfo(const Buffer<T>* buff, bool isReadOnly = false): m_buffer((void*)buff), m_isReadOnly(isReadOnly){}

	void* m_buffer;
	bool m_isReadOnly;
};

#define ADL_DEFAULT_LOCAL_SIZE_1D 64
#define ADL_DEFAULT_LOCAL_SIZE_2D 8

template<int N> struct ND {};
static ND<1> ND1;
static ND<2> ND2;
struct ThreadsToExec
{
    ThreadsToExec(){}
    ThreadsToExec( ND<1> nd, int numThreads, int localSize = ADL_DEFAULT_LOCAL_SIZE_1D ) : ThreadsToExec( ND2, numThreads, 1, localSize, 1 ) {}
    ThreadsToExec( ND<2> nd, int numThreadsX, int numThreadsY, int localSizeX = ADL_DEFAULT_LOCAL_SIZE_2D, int localSizeY = ADL_DEFAULT_LOCAL_SIZE_2D )
    {
        m_numThreads[0] = numThreadsX;
        m_numThreads[1] = numThreadsY;
        m_localSize[0] = localSizeX;
        m_localSize[1] = localSizeY;
    }
#if SUPPORT_INDIRECT_DISPATCH
    ThreadsToExec( ND<1> nd, Buffer<int>* indirectBuffer, int localSize = ADL_DEFAULT_LOCAL_SIZE_1D ) : ThreadsToExec( ND2, indirectBuffer, localSize, 1 ) {}
    ThreadsToExec( ND<2> nd, Buffer<int>* indirectBuffer, int localSizeX = ADL_DEFAULT_LOCAL_SIZE_2D, int localSizeY = ADL_DEFAULT_LOCAL_SIZE_2D )
    {
        m_indirectBuffer = indirectBuffer;
        m_localSize[0] = localSizeX;
        m_localSize[1] = localSizeY;
    }
#endif
    int getNumThreads() const
    {
        // TODO assert this is a 1d exec
        int nWIsToExecute;
#if SUPPORT_INDIRECT_DISPATCH
        if(m_indirectBuffer)
        {
            m_indirectBuffer->read( &nWIsToExecute, 1 ); // the first int is nWIsToExecute // TODO figure out a way to avoid passing nWIsToExecute directly to world->getFileCacheUtil()->prepare
            DeviceUtils::waitForCompletion( m_indirectBuffer->m_device );
        }
        else
#endif
        {
            nWIsToExecute = m_numThreads[0];
        }
        return nWIsToExecute;
    }
    int m_numThreads[2];
    int m_localSize[2];
#if SUPPORT_INDIRECT_DISPATCH
    Buffer<int>* m_indirectBuffer = NULL;
#endif
};
    
class Launcher
{
	public:
		enum 
		{
            // The Metal backend only supports 31 arguments. For kernels that need more, we pack constants into structs so
            // that multiple constants can be bound as a single argument (this is why MAX_ARG_SIZE needs to be increased).
            
            MAX_ARG_SIZE = 64,      // MAX_ARG_SIZE = 16,
            MAX_ARG_COUNT = 64,
		};

		struct Args
		{
			int m_isBuffer;
			size_t m_sizeInBytes;

			void* m_ptr;
			char m_data[MAX_ARG_SIZE];
		};

		struct ExecInfo
		{
			int m_nWIs[3];
			int m_wgSize[3];
			int m_nDim;

			ExecInfo(){}
			ExecInfo(int nWIsX, int nWIsY = 1, int nWIsZ = 1, int wgSizeX = 64, int wgSizeY = 1, int wgSizeZ = 1, int nDim = 1) 
			{
				m_nWIs[0] = nWIsX;
				m_nWIs[1] = nWIsY;
				m_nWIs[2] = nWIsZ;

				m_wgSize[0] = wgSizeX;
				m_wgSize[1] = wgSizeY;
				m_wgSize[2] = wgSizeZ;

				m_nDim = nDim;
			}
		};

		__inline
		Launcher(const Device* dd, const char* fileName, const char* funcName, const char* option = NULL);
		__inline
		Launcher(const Device* dd, const Kernel* kernel);
		__inline
		void setBuffers( BufferInfo* buffInfo, int n );
		template<typename T>
		__inline
		void setConst( Buffer<T>& constBuff, const T& consts );
		template<typename T>
		__inline
		void setConst( const T& consts );
		__inline
		void setConst( const void* consts, size_t byteCount );
		__inline
		void setLocalMemory( size_t size );
		__inline
		float launch1D( int numThreads, int localSize = ADL_DEFAULT_LOCAL_SIZE_1D, SyncObject* syncObj = 0 );
		__inline
		float launch2D( int numThreadsX, int numThreadsY, int localSizeX = ADL_DEFAULT_LOCAL_SIZE_2D, int localSizeY = ADL_DEFAULT_LOCAL_SIZE_2D, SyncObject* syncObj = 0 );
		__inline
		float launch( ThreadsToExec& threadsToExec, SyncObject* syncObj = 0 );
		__inline
		void serializeToFile( const char* filePath, const ExecInfo& info );
		__inline
		ExecInfo deserializeFromFile( const char* filePath, int buffCap, Buffer<char>** buffsOut, int* nBuffs );
		
	public:
		enum
		{
			CONST_BUFFER_SIZE = 512,
		};

		const Device* m_deviceData;
		const Kernel* m_kernel;
		int m_idx;
		int m_idxRw;

		Args m_args[MAX_ARG_COUNT];
};

template<DeviceType TYPE>
class KernelBuilder
{
	public:

		__inline
		KernelBuilder(): m_ptr(0){}
		
		__inline
		bool setFromFile( const Device* deviceData, const char* fileName, const char* option = NULL, bool addExtension = false,
			bool cacheKernel = true, const char** depsList = NULL, int nDeps = 0);

		__inline
		void setFromSrc( const Device* deviceData, const char* src, const char* option = NULL );

		__inline
		void setFromStringLists( const Device* deviceData, const char** src, int nSrc, const char* option = NULL, const char* fileName = NULL );

		__inline
		void createKernel( const char* funcName, Kernel& kernelOut );

		__inline
		bool isCacheUpToDate( const Device* deviceData, const char* fileName, const char* option ) { return false; }

		__inline
		~KernelBuilder();
		//	todo. implemement in kernel destructor?
		__inline
		static void deleteKernel( Kernel& kernel );

	private:
		enum
		{
			MAX_PATH_LENGTH = 260,
		};
		const Device* m_deviceData;
#ifdef UNICODE
		wchar_t m_path[MAX_PATH_LENGTH];
#else
		char m_path[MAX_PATH_LENGTH];
#endif
		void* m_ptr;
};

};
