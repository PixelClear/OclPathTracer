/*
		2011 Takahiro Harada
*/

#include <Adl/Adl.h>
#include <algorithm>

#define max2 std::max
#define min2 std::min

namespace adl
{
//===========================================
//	KernelBuilderCLImpl
//===========================================

bool KernelBuilderCLImpl::isFileUpToDate(const char* binaryFileName,const char* srcFileName)

{
	FileStat b(binaryFileName);

	if( !b.found() )
		return false;

	FileStat s(srcFileName);

	if( !s.found() )
	{
		//	todo. compare with exe time
		return true;
	}

	if( s.getTime() < b.getTime() )
		return true;

	return false;
}

inline
adlu64 checksum( const char* data, adlu64 size )
{
	adlu64 ans = 0;
	adlu64 end = (adlu64)data + size;
	while( (adlu64)data + sizeof(int)/2 < end )
	{
		ans += ((int*)(data))[0] & 0xffff;
		data += sizeof(int);
	}
	return ans;
}

inline
void getCheckSumFileName( const char* binaryName, char* dst )
{
	sprintf( dst,"%s.check",binaryName );
}

cl_program KernelBuilderCLImpl::loadFromCache(const Device* device, const char* binaryFileName, const char* option)
{
	adlu64 checksumValue = 0;
	{
		char csFileName[512];
		getCheckSumFileName( binaryFileName, csFileName );
		FILE* csfile = fopen(csFileName, "rb");
		if( csfile )
		{
			fread( &checksumValue, sizeof(adlu64), 1, csfile );
			fclose( csfile );
		}
	}

	if( checksumValue == 0 )
		return 0;

	cl_program program = 0;
	cl_int status = 0;
	FILE* file = fopen(binaryFileName, "rb");
	if( file )
	{
		fseek( file, 0L, SEEK_END );
		size_t binarySize = ftell( file );
		rewind( file );
		char* binary = new char[binarySize];
		size_t dummy = fread( binary, sizeof(char), binarySize, file );
		fclose( file );

		adlu64 s = checksum( binary, binarySize );
		if( s != checksumValue )
		{
			printf("adl::checksum doesn't match %x : %x\n", s, checksumValue);
			return 0;
		}
		const DeviceCL* dd = (const DeviceCL*) device;
		program = clCreateProgramWithBinary( dd->m_context, 1, &dd->m_deviceIdx, &binarySize, (const unsigned char**)&binary, 0, &status );
		if( status != CL_SUCCESS )
			handleBuildError(dd->m_deviceIdx, program );
		status = clBuildProgram( program, 1, &dd->m_deviceIdx, option, 0, 0 );
		if( status != CL_SUCCESS )
			handleBuildError(dd->m_deviceIdx, program );

		delete [] binary;
	}
	return program;
}

void KernelBuilderCLImpl::hashString(const char *ss, const size_t size, char buf[9]) 
{
	const unsigned int hash = hashBin(ss, size);

	sprintf(buf, "%08x", hash);
}

unsigned int KernelBuilderCLImpl::hashBin(const char *s, const size_t size) 
{
	unsigned int hash = 0;

	for (unsigned int i = 0; i < size; ++i) {
		hash += *s++;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

void KernelBuilderCLImpl::getBinaryFileName(const Device* deviceData, const char* fileName, const char* option, char *binaryFileName) 
{
	char deviceName[256];
	deviceData->getDeviceName(deviceName);
	char driverVersion[256];
	const DeviceCL* dd = (const DeviceCL*) deviceData;
	clGetDeviceInfo(dd->m_deviceIdx, CL_DRIVER_VERSION, 256, &driverVersion, NULL);
	const char* strippedFileName = strip(fileName,"\\");
	strippedFileName = strip(strippedFileName,"/");
	char optionHash[9] = "0x0";
	if( fileName && option )
	{
		//256 is not enough
		// 'option' can by very long: more than 200
		const int tmpSize = 1024;

		char tmp[tmpSize];
		
		size_t fileName_len = strlen(fileName);
		size_t option_len = strlen(option);
		if ( fileName_len + option_len >= tmpSize )
		{
			binaryFileName = nullptr;
			ADLASSERT( false, TH_FAILURE);
			return;
		}
		sprintf(tmp, "%s%s", fileName, option);
		hashString(tmp, strlen(tmp), optionHash);
	}
	sprintf(binaryFileName, "%s/%s-%s.v.%d.%s.%s_%d.bin", s_cacheDirectory, strippedFileName, optionHash, deviceData->getBinaryFileVersion(), deviceName, driverVersion, (sizeof(void*)==4)?32:64 );
}

void KernelBuilderCLImpl::handleBuildError(cl_device_id deviceIdx, cl_program program)
{
	char *build_log;
	size_t ret_val_size;
	cl_int status = clGetProgramBuildInfo(program, deviceIdx, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
	build_log = new char[ret_val_size+1];
	status = clGetProgramBuildInfo(program, deviceIdx, CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL);
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

	build_log[ret_val_size] = '\0';
	ADL_LOG("CL Kernel Load Failure: %d\n", status);
	ADL_LOG("%s\n", build_log);
	printf("%s\n", build_log);

	ADLASSERT(0, TH_ERROR_INTERNAL);
	delete build_log;
}

cl_program KernelBuilderCLImpl::loadFromSrc( const Device* deviceData, const char* src, const char* option, bool buildProgram )
{
	ADLASSERT( deviceData->m_type == TYPE_CL, TH_ERROR_OPENCL);
	const DeviceCL* dd = (const DeviceCL*) deviceData;

	cl_program program;
	cl_int status = 0;
	size_t srcSize[] = {strlen( src )};
	program = clCreateProgramWithSource( dd->m_context, 1, &src, srcSize, &status );
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
	if( buildProgram )
		status = clBuildProgram( program, 1, &dd->m_deviceIdx, option, NULL, NULL );
#if defined(TH_CL2)
	else
		status = clCompileProgram( program, 1, &dd->m_deviceIdx, option, 0,0,0,0,0 );
#endif
	if( status != CL_SUCCESS )
		KernelBuilderCLImpl::handleBuildError( dd->m_deviceIdx, program );

	return program;
}

void KernelBuilderCLImpl::cacheBinaryToFile(const Device* device, cl_program& program, const char* binaryFileName)
{
	cl_int status = 0;
	size_t binarySize;
	status = clGetProgramInfo( program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &binarySize, 0 );
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

	char* binary = new char[binarySize];

	status = clGetProgramInfo( program, CL_PROGRAM_BINARIES, sizeof(char*), &binary, 0 );
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

	FILE* file = fopen(binaryFileName, "wb");
	if (file)
	{
		debugPrintf("Cached file created %s\n", binaryFileName);

		fwrite( binary, sizeof(char), binarySize, file );
		fclose( file );
	}

	{
		adlu64 s = checksum( binary, binarySize );
		char filename[512];
		getCheckSumFileName( binaryFileName, filename );
		FILE* file = fopen(filename, "wb");
		if (file)
		{
			fwrite( &s, sizeof(adlu64), 1, file );
			fclose( file );
		}
	}

	delete [] binary;
}

cl_program KernelBuilderCLImpl::setFromStringListsImpl( const Device* deviceData, const char** src, int nSrc, const char* option )
{
	ADLASSERT( deviceData->m_type == TYPE_CL, TH_ERROR_OPENCL);
	const DeviceCL* dd = (const DeviceCL*) deviceData;

	cl_program program = 0;
	cl_int status = 0;
	//	size_t srcSize[] = {strlen( src )};
	size_t srcSize[128];
	ADLASSERT( nSrc < 128, TH_ERROR_PARAMETER );
	for(int i=0; i<nSrc; i++)
	{
		srcSize[i] = strlen( src[i] );
	}

	// Callback for compile start
	if ( deviceData->m_compileCallback.callback != nullptr )
		deviceData->m_compileCallback.callback(true, deviceData->m_compileCallback.userData);

	program = clCreateProgramWithSource( dd->m_context, nSrc, src, srcSize, &status );
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
	status = clBuildProgram( program, 1, &dd->m_deviceIdx, "-O0", NULL, NULL );

	// Callback for compile end
	if (deviceData->m_compileCallback.callback != nullptr)
		deviceData->m_compileCallback.callback(false, deviceData->m_compileCallback.userData);

	if( status != CL_SUCCESS )
		KernelBuilderCLImpl::handleBuildError( dd->m_deviceIdx, program );
	return program;
}

void KernelBuilderCLImpl::createDirectory( const char* cacheDirName )
{
#if defined(WIN32)
#ifdef UNICODE
	{
		WCHAR fName[128];
		MultiByteToWideChar(CP_ACP,0,cacheDirName,-1, fName, 128);
		CreateDirectory( fName, 0 );
	}
#else
	CreateDirectory(cacheDirName,0);
#endif
#else
	mkdir( cacheDirName, 0775 );
#endif
}

class File
{
public:
	__inline
		bool open(const char* fileNameWithExtension)
	{
		size_t      size;
		char*       str;

		// Open file stream
		std::fstream f(fileNameWithExtension, (std::fstream::in | std::fstream::binary));

		// Check if we have opened file stream
		if (f.is_open()) {
			size_t  sizeFile;
			// Find the stream size
			f.seekg(0, std::fstream::end);
			size = sizeFile = (size_t)f.tellg();
			f.seekg(0, std::fstream::beg);

			str = new char[size + 1];
			if (!str) {
				f.close();
				return  false;
			}

			// Read file
			f.read(str, sizeFile);
			f.close();
			str[size] = '\0';

			m_source  = str;

			delete[] str;

			return true;
		}

		return false;
	}
	const std::string& getSource() const {return m_source;}

private:
	std::string m_source;
};

bool KernelBuilderCLImpl::setFromFile( const Device* deviceData, const char* fileName, const char* option, bool addExtension,
	bool cacheKernel, const char** depsList, int nDeps, cl_program& program)
{
	if( option == 0 )
		option = s_cLDefaultCompileOption;

	char fileNameWithExtension[256];

	if( addExtension )
		sprintf( fileNameWithExtension, "%s.cl", fileName );
	else
		sprintf( fileNameWithExtension, "%s", fileName );

	//		cl_program& program = (cl_program&)m_ptr;

	bool cacheBinary = cacheKernel;

	bool upToDate = false;

	if ( deviceData->m_compileCallback.callback != nullptr )
		deviceData->m_compileCallback.callback(true, deviceData->m_compileCallback.userData);

	char binaryFileName[512];
	{
		KernelBuilderCLImpl::getBinaryFileName(deviceData, fileName, option, binaryFileName);
		upToDate = isFileUpToDate(binaryFileName,fileNameWithExtension);
		for(int i=0; i<nDeps && upToDate; i++)
		{
			upToDate = isFileUpToDate( binaryFileName, depsList[i] );
		}
		createDirectory( s_cacheDirectory );
	}
	if( cacheBinary && upToDate)
	{
		program = KernelBuilderCLImpl::loadFromCache( deviceData, binaryFileName, option );
	}

	if( !program )
	{
		File kernelFile;
		bool result = kernelFile.open( fileNameWithExtension );
		if( !result )
		{
			debugPrintf("Kernel not found %s\n", fileNameWithExtension); fflush( stdout );
			return false;
		}
		ADLASSERT( result, TH_ERROR_NULLPTR );
		const char* source = kernelFile.getSource().c_str();
		program = KernelBuilderCLImpl::setFromStringListsImpl( deviceData, &source, 1, option );

		//		if( cacheBinary )
		{	//	write to binary
			KernelBuilderCLImpl::cacheBinaryToFile( deviceData, program, binaryFileName );
		}
	}

	if (deviceData->m_compileCallback.callback != nullptr)
		deviceData->m_compileCallback.callback(false, deviceData->m_compileCallback.userData);

	return true;
}

//===========================================
//	launcher
//===========================================
void LauncherCL::setBuffers( Launcher* launcher, BufferInfo* buffInfo, int n )
{
//    ADLASSERT(launcher->m_idx + n <= Launcher::MAX_ARG_COUNT, TH_ERROR_INTERNAL);

	KernelCL* clKernel = (KernelCL*)launcher->m_kernel;
	for(int i=0; i<n; i++)
	{
		Buffer<int>* buff = (Buffer<int>*)buffInfo[i].m_buffer;
//		ADLASSERT( buff->getSize(), TH_ERROR_PARAMETER );
		cl_int status = clSetKernelArg( clKernel->getKernel(), launcher->m_idx++, sizeof(cl_mem), &buff->m_ptr );
		ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
		
		if(1)
		{
			Launcher::Args& arg = launcher->m_args[launcher->m_idx-1];
			arg.m_isBuffer = 1;
			arg.m_sizeInBytes = 0;
			arg.m_ptr = (void*)buff;//->m_ptr;
		}
		else
		{
			size_t sizeInByte;
			if( buff->m_ptr == 0 )
			{
				sizeInByte = 0;
				buff = 0;
			}
			else
			{
				cl_int e = clGetMemObjectInfo( (cl_mem)buff->m_ptr, CL_MEM_SIZE, sizeof(size_t), &sizeInByte, 0 );
				ADLASSERT( e == CL_SUCCESS, TH_ERROR_OPENCL);
			}

			Launcher::Args& arg = launcher->m_args[launcher->m_idx-1];
			arg.m_isBuffer = 1;
			arg.m_sizeInBytes = (int)sizeInByte;
			arg.m_ptr = (void*)buff;//->m_ptr;
		}
	}
}

void LauncherCL::launch2D( Launcher* launcher, int numThreadsX, int numThreadsY, int localSizeX, int localSizeY, SyncObject* syncObj, float& t )
{
	t = 0.f;
	cl_event* e = 0;
	if( syncObj )
	{
		ADLASSERT( syncObj->m_device->getType() == TYPE_CL, TH_ERROR_OPENCL);
		e = (cl_event*)syncObj->m_ptr;

		if( *e )
		{
			cl_int status = clReleaseEvent( *e );
			ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
		}
	}
	KernelCL* clKernel = (KernelCL*)launcher->m_kernel;
	const DeviceCL* ddcl = (const DeviceCL*)launcher->m_deviceData;

	if( localSizeX > ddcl->m_maxLocalWGSize[0] ) localSizeX = ddcl->m_maxLocalWGSize[0];
	if( localSizeY > ddcl->m_maxLocalWGSize[1] ) localSizeY = ddcl->m_maxLocalWGSize[1];

	size_t gRange[3] = {1,1,1};
	size_t lRange[3] = {1,1,1};
	lRange[0] = localSizeX;
	lRange[1] = localSizeY;
	gRange[0] = max2((size_t)1, (numThreadsX/lRange[0])+(!(numThreadsX%lRange[0])?0:1));
	gRange[0] *= lRange[0];
	gRange[1] = max2((size_t)1, (numThreadsY/lRange[1])+(!(numThreadsY%lRange[1])?0:1));
	gRange[1] *= lRange[1];

	StopwatchHost sw;
	sw.start();

	cl_int status = clEnqueueNDRangeKernel( ddcl->m_commandQueue, 
		clKernel->getKernel(), 2, NULL, gRange, lRange, 0,0,e );
	ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

#if defined(_DEBUG)
	DeviceUtils::waitForCompletion( launcher->m_deviceData );
#endif
	if( launcher->m_deviceData->m_enableProfiling != Device::PROFILE_NON )
	{
		DeviceUtils::waitForCompletion( launcher->m_deviceData );
		sw.stop();

		if( launcher->m_deviceData->m_enableProfiling & Device::PROFILE_RETURN_TIME )
			t = sw.getMs();
/*
		if( launcher->m_deviceData->m_enableProfiling & Device::PROFILE_WRITE_FILE )
		{
			char filename[256];
			sprintf(filename, "ProfileCL.%s.%s.csv", ddcl->m_deviceName, ddcl->m_driverVersion);
			std::ofstream ofs( filename, std::ios::out | std::ios::app );

			ofs << "\"" << launcher->m_kernel->m_funcName << "\",\"" << sw.getMs()  << "\",\""  << gRange[0] << "\",\"" << gRange[1] << "\",\"" << gRange[1]  << "\"" ;
			ofs << std::endl;
			ofs.close();
		}
*/
	}
}

#if SUPPORT_INDIRECT_DISPATCH
void LauncherCL::launchIndirect2D( Launcher* launcher, Buffer<int>& indirectBuffer, int localSizeX, int localSizeY, SyncObject* syncObj, float& t )
{
	abort(); // this can work in OpenCL2.0 I think...
}
#endif

void LauncherCL::serializeToFile( Launcher* launcher, const char* filePath, const Launcher::ExecInfo& info )
{
	std::ofstream ofs;
	ofs.open( filePath, std::ios::out|std::ios::binary );

	ofs.write( (char*)&launcher->m_idx, sizeof(int) );

	for(int i=0; i<launcher->m_idx; i++)
	{
		Launcher::Args& arg = launcher->m_args[i];

		ofs.write( (char*)&arg.m_isBuffer, sizeof(int) );

		if( arg.m_isBuffer )
		{
			Buffer<int>* buff = (Buffer<int>*)arg.m_ptr;
			if( buff->m_ptr == 0 )
			{
				arg.m_sizeInBytes = 0;
				buff = 0;
			}
			else
			{
				cl_int e = clGetMemObjectInfo( (cl_mem)buff->m_ptr, CL_MEM_SIZE, sizeof(size_t), &arg.m_sizeInBytes, 0 );
				ADLASSERT( e == CL_SUCCESS, TH_ERROR_OPENCL);
			}

			ofs.write( (char*)&arg.m_sizeInBytes, sizeof(int) );
			if( arg.m_ptr && arg.m_sizeInBytes != 0 )
			{
				Buffer<char>* gpu = (Buffer<char>*)arg.m_ptr;
				char* h = gpu->getHostPtr( (int)(arg.m_sizeInBytes/sizeof(char)));
				DeviceUtils::waitForCompletion( launcher->m_deviceData );

				ofs.write( h, arg.m_sizeInBytes );

				gpu->returnHostPtr( h );
				DeviceUtils::waitForCompletion( launcher->m_deviceData );
			}
			else
			{
				ADLASSERT( arg.m_sizeInBytes == 0, TH_ERROR_PARAMETER );
			}
		}
		else
		{
			ofs.write( (char*)&arg.m_sizeInBytes, sizeof(int) );
			ofs.write( (char*)arg.m_data, arg.m_sizeInBytes );
		}
	}

	ofs.write( (char*)&info, sizeof(Launcher::ExecInfo) );

	ofs.close();
}

void LauncherCL::deserializeFromFile( Launcher* launcher, const char* filePath, int buffCap, Buffer<char>** buffsOut, int* nBuffs, Launcher::ExecInfo& infoOut )
{
	KernelCL* clKernel = (KernelCL*)launcher->m_kernel;

	std::ifstream ifs;
	ifs.open( filePath, std::ios::in|std::ios::binary);

	int nArgs;
	ifs.read( (char*)&nArgs, sizeof(int) );

	(*nBuffs) = 0;
	for(int i=0; i<nArgs; i++)
	{
		int isBuffer;
		int sizeInBytes;

		ifs.read( (char*)&isBuffer, sizeof(int) );
		ifs.read( (char*)&sizeInBytes, sizeof(int) );

		char* data = new char[sizeInBytes];

		ifs.read( data, sizeInBytes );

		if( isBuffer )
		{
			Buffer<char>* gpu;

			gpu = new Buffer<char>( launcher->m_deviceData, sizeInBytes );
			if( sizeInBytes == 0 )
			{

			}
			else
			{
				gpu->write( data, sizeInBytes );
				DeviceUtils::waitForCompletion( launcher->m_deviceData );
			}

			cl_int status = clSetKernelArg( clKernel->getKernel(), launcher->m_idx++, sizeof(cl_mem), (gpu->m_ptr)?&gpu->m_ptr:0 );
			ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);

			if( (*nBuffs) < buffCap )
				buffsOut[(*nBuffs)++] = gpu;
		}
		else
		{
			cl_int status = clSetKernelArg( clKernel->getKernel(), launcher->m_idx++, sizeInBytes, data );
			ADLASSERT( status == CL_SUCCESS, TH_ERROR_OPENCL);
		}

		delete [] data;
	}
	ifs.read( (char*)&infoOut, sizeof(Launcher::ExecInfo) );

	ifs.close();
}


};
