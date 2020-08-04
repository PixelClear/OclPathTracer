/*
	2011 Takahiro Harada
*/

#include <Adl/Adl.h>


namespace adl
{
//==========================
//	KernelManager
//==========================
bool KernelManager::find(const Device* dd, const char* fileName, const char* funcName, const char* option )
{//todo. remove copied code
	const int charSize = 1024*2;
	KernelManager* s_kManager = this;

	char fullFineName[charSize];
	switch( dd->m_type )
	{
#if defined(ADL_ENABLE_CL)
	case TYPE_CL:
		sprintf(fullFineName,"%s.cl", fileName);
		break;
#endif
#if defined(ADL_ENABLE_DX11)
	case TYPE_DX11:
		sprintf(fullFineName,"%s.hlsl", fileName);
		break;
#endif
#if defined(ADL_ENABLE_METAL)
    case TYPE_METAL:
        sprintf(fullFineName,"%s.metal", fileName);
        break;
#endif
	default:
		ADLASSERT(0, TH_ERROR_PARAMETER);
		break;
	};

	char mapName[charSize];
	{
		if( option )
			sprintf(mapName, "%d%s%s%s", (int)(long long)dd->getContext(), fullFineName, funcName, option);
		else
			sprintf(mapName, "%d%s%s", (int)(long long)dd->getContext(), fullFineName, funcName);
	}

	std::string str(mapName);

	KMap::iterator iter = s_kManager->m_map.find( str );

	if( iter != s_kManager->m_map.end() )
	{
		return true;
	}

	char optionWithFlags[charSize];
	sprintf(optionWithFlags, "%s", option ? option : " -I ..\\..\\"); // set default options if no options are provided

	// Set special compile flags for nvidia to speedup kernel compilation
	if (dd->getType() == TYPE_CL && ((DeviceCL*)dd)->m_vendor == VD_NV)
		sprintf(optionWithFlags, "%s%s", optionWithFlags, " -cl-nv-opt-level=2");

	switch( dd->m_type )
	{
#if defined(ADL_ENABLE_CL)
	case TYPE_CL:
		{
			KernelBuilder<TYPE_CL> builder;
			if( builder.isCacheUpToDate( dd, fileName, optionWithFlags ) )
			{
				return true;
			}
		}
		break;
#endif
#if defined(ADL_ENABLE_METAL)
        case TYPE_METAL:
        {
            KernelBuilder<TYPE_METAL> builder;
			if( builder.isCacheUpToDate( dd, fileName, optionWithFlags ) )
				return true;
		}
		break;
#endif
	default:
		ADLASSERT(0, TH_ERROR_INTERNAL);
		break;
	};	
	return false;
}

Kernel* KernelManager::query(const Device* dd, const char* fileName, const char* funcName, const char* option, const char** srcList, int nSrc, const char** depsList, int nDeps, bool cacheKernel)
{//todo. remove copied code
	const int charSize = 1024*2;
	KernelManager* s_kManager = this;

	char fullFineName[charSize];
	switch( dd->m_type )
	{
#if defined(ADL_ENABLE_CL)
	case TYPE_CL:
		sprintf(fullFineName,"%s.cl", fileName);
		break;
#endif
#if defined(ADL_ENABLE_DX11)
	case TYPE_DX11:
		sprintf(fullFineName,"%s.hlsl", fileName);
		break;
#endif
#if defined(ADL_ENABLE_METAL)
    case TYPE_METAL:
        sprintf(fullFineName,"%s.metal", fileName);
        break;
#endif
	default:
		ADLASSERT(0, TH_ERROR_PARAMETER);
		break;
	};

	char mapName[charSize];
	{
		if( option )
			sprintf(mapName, "%d%s%s%s", (int)(long long)dd->getContext(), fullFineName, funcName, option);
		else
			sprintf(mapName, "%d%s%s", (int)(long long)dd->getContext(), fullFineName, funcName);
	}

	std::string str(mapName);

	if( !cacheKernel )
	{
		if( s_kManager->m_map.count( str ) )
		{
			Kernel* k = s_kManager->m_map[str];
			s_kManager->m_map.erase( str );
			delete k;
		}
	}

	KMap::iterator iter = s_kManager->m_map.find( str );

	bool uptodate = true;
	if( iter != s_kManager->m_map.end() && !srcList )
	{//	set from file
		//	check if files are up to date, if out of dated, trigger rebuild
		//	however, this adds overhead so not sure we should add this or not although it's useful for debugging of course
		//if( anyFileOutDated )
		//	uptodate = false;
	}

	Kernel* kernelOut;
	if( iter == s_kManager->m_map.end() || !uptodate )
	{
		kernelOut = new Kernel();

		char optionWithFlags[charSize];
		sprintf(optionWithFlags, "%s", option ? option : " -I ..\\..\\"); // set default options if no options are provided

		// Set special compile flags for nvidia to speedup kernel compilation
		if (dd->getType() == TYPE_CL && ((DeviceCL*)dd)->m_vendor == VD_NV)
			sprintf(optionWithFlags, "%s%s", optionWithFlags, " -cl-nv-opt-level=2");

		switch( dd->m_type )
		{
#if defined(ADL_ENABLE_CL)
		case TYPE_CL:
			{
				KernelBuilder<TYPE_CL> builder;
				if( srcList )
					builder.setFromStringLists( dd, srcList, nSrc, optionWithFlags, fileName, cacheKernel );
//					builder.setFromSrc( dd, src, option );
				else
				{
					bool e = builder.setFromFile( dd, fileName, optionWithFlags, true, cacheKernel, depsList, nDeps );
					if( !e )
					{
						delete kernelOut;
						return 0;
					}
				}
				builder.createKernel( funcName, *kernelOut );
				kernelOut->m_funcName = funcName;
			}
			break;
#endif
#if defined(ADL_ENABLE_DX11)
		case TYPE_DX11:
			{
				KernelBuilder<TYPE_DX11> builder;
				if( src )
					builder.setFromSrc( dd, src, option );
				else
					builder.setFromFile( dd, fileName, option, true, cacheKernel );
				builder.createKernel( funcName, *kernelOut );
			}
			break;
#endif
#if defined(ADL_ENABLE_METAL)
            case TYPE_METAL:
            {
                KernelBuilder<TYPE_METAL> builder;
                if( srcList )
                    builder.setFromStringLists( dd, srcList, nSrc, option, fileName, cacheKernel );
                else
                    builder.setFromFile( dd, fileName, option, true, cacheKernel );
                builder.createKernel( funcName, *kernelOut );
            }
                break;
#endif
		default:
			ADLASSERT(0, TH_ERROR_INTERNAL);
			break;
		};
		s_kManager->m_map.insert( KMap::value_type(str,kernelOut) );
	}
	else
	{
		kernelOut = iter->second;
	}

	return kernelOut;
}

void KernelManager::reset()
{
	for(KMap::iterator iter = m_map.begin(); iter != m_map.end(); iter++)
	{
		Kernel* k = iter->second;
		switch( k->m_type )
		{
#if defined(ADL_ENABLE_CL)
		case TYPE_CL:
			KernelBuilder<TYPE_CL>::deleteKernel( *k );
			break;
#endif
#if defined(ADL_ENABLE_DX11)
		case TYPE_DX11:
			KernelBuilder<TYPE_DX11>::deleteKernel( *k );
			break;
#endif
#if defined(ADL_ENABLE_METAL)
        case TYPE_METAL:
            KernelBuilder<TYPE_METAL>::deleteKernel( *k );
            break;
#endif
		default:
			ADLASSERT(0, TH_ERROR_INTERNAL);
			break;
		};
		delete k;
	}
	m_map.clear();
}

KernelManager::~KernelManager()
{
	for(KMap::iterator iter = m_map.begin(); iter != m_map.end(); iter++)
	{
		Kernel* k = iter->second;
		switch( k->m_type )
		{
#if defined(ADL_ENABLE_CL)
		case TYPE_CL:
			KernelBuilder<TYPE_CL>::deleteKernel( *k );
			break;
#endif
#if defined(ADL_ENABLE_DX11)
		case TYPE_DX11:
			KernelBuilder<TYPE_DX11>::deleteKernel( *k );
			break;
#endif
#if defined(ADL_ENABLE_METAL)
        case TYPE_METAL:
            KernelBuilder<TYPE_METAL>::deleteKernel( *k );
            break;
#endif
		default:
			ADLASSERT(0, TH_ERROR_INTERNAL);
			break;
		};
		delete k;
	}
}

};
