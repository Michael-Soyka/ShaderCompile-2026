//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: DXC backend, HLSL compute shaders to SPIR-V
//
//=============================================================================//

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "dxc.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <vector>

#include "cfgprocessor.h"
#include "d3dcompiler.h"
#include "d3dxfxc.h"
#include "dxc/dxcapi.h"
#include "gsl/narrow"
#include "includepath.h"

namespace fs = std::filesystem;

namespace
{
	class DxcResponse final : public CmdSink::IResponse
	{
	public:
		DxcResponse( std::vector<char>&& code, std::string&& listing, bool bSucceeded )
			: m_Code( std::move( code ) )
			, m_Listing( std::move( listing ) )
			, m_bSucceeded( bSucceeded )
		{
		}

		bool Succeeded() const override
		{
			return m_bSucceeded;
		}

		size_t GetResultBufferLen() const override
		{
			return m_bSucceeded ? m_Code.size() : 0;
		}

		const void* GetResultBuffer() const override
		{
			return m_bSucceeded ? m_Code.data() : nullptr;
		}

		const char* GetListing() const override
		{
			return m_Listing.empty() ? nullptr : m_Listing.c_str();
		}

	private:
		std::vector<char> m_Code;
		std::string m_Listing;
		bool m_bSucceeded;
	};

	// Includes come from the file cache, DXC asks for every search candidate
	class DxcInclude final : public IDxcIncludeHandler
	{
	public:
		explicit DxcInclude( IDxcUtils* pUtils )
			: m_pUtils( pUtils )
		{
		}

		// DXC asks one #include as a series of candidates: the includer's folder first, then every folder up
		// the includer stack, then -I. Like IncludePath::Resolve, only the first candidate and the root
		// candidate (a path suffix of the first one) are accepted, never another folder of the stack
		HRESULT STDMETHODCALLTYPE LoadSource( LPCWSTR pFilename, IDxcBlob** ppIncludeSource ) override
		{
			*ppIncludeSource = nullptr;

			const auto key = IncludePath::Normalize( fs::path( pFilename ) );
			const std::string candidate = key ? *key : std::string();
			if ( m_bNewInclude )
			{
				m_FirstCandidate = candidate;
				m_bNewInclude = false;
			}
			else if ( !IsPathSuffix( m_FirstCandidate, candidate ) )
			{
				return E_FAIL;
			}

			if ( !key )
			{
				return E_FAIL;
			}

			const CSharedFile* pFile = fileCache.Get( *key );
			if ( !pFile )
			{
				return E_FAIL;
			}

			IDxcBlobEncoding* pBlob = nullptr;
			const HRESULT hr = m_pUtils->CreateBlobFromPinned( pFile->Data(), gsl::narrow<UINT32>( pFile->Size() ), DXC_CP_UTF8, &pBlob );
			*ppIncludeSource = pBlob;
			if ( SUCCEEDED( hr ) )
			{
				m_bNewInclude = true;
				CheckFileName( *key );
			}

			return hr;
		}

		// Empty when the compile used no two different headers with one file name
		const std::string& GetConflict() const
		{
			return m_Conflict;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, void** ppvObject ) override
		{
			if ( ( riid == __uuidof( IDxcIncludeHandler ) ) || ( riid == __uuidof( IUnknown ) ) )
			{
				*ppvObject = this;
				return S_OK;
			}

			*ppvObject = nullptr;
			return E_NOINTERFACE;
		}

		ULONG STDMETHODCALLTYPE AddRef() override
		{
			return 1;
		}

		ULONG STDMETHODCALLTYPE Release() override
		{
			return 1;
		}

	private:
		// "a/b/c.h" ends with "c.h" and "b/c.h", not with "/c.h" or "x/c.h"
		static bool IsPathSuffix( const std::string& full, const std::string& tail )
		{
			if ( tail.empty() || ( tail.size() > full.size() ) )
			{
				return false;
			}

			const size_t start = full.size() - tail.size();
			const bool bEndsWith = ( full.compare( start, tail.size(), tail ) == 0 );
			const bool bBoundary = ( start == 0 ) || ( full[start - 1] == '/' );
			return bEndsWith && bBoundary;
		}

		// DXC keeps the first header it loaded under a file name and reuses it for another folder
		void CheckFileName( const std::string& key )
		{
			std::string name = fs::path( key ).filename().string();
			std::transform( name.begin(), name.end(), name.begin(), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );

			const auto [it, inserted] = m_FileNames.try_emplace( name, key );
			if ( !inserted && ( it->second != key ) && m_Conflict.empty() )
			{
				m_Conflict = "error: \"" + it->second + "\" and \"" + key + "\" have the same file name, DXC would use one of them for both, rename one\n";
			}
		}

		IDxcUtils* m_pUtils;
		bool m_bNewInclude = true;
		std::string m_FirstCandidate;
		std::map<std::string, std::string> m_FileNames;
		std::string m_Conflict;
	};

	// DXC instances are not shared between threads
	struct ThreadState
	{
		IDxcUtils* pUtils = nullptr;
		IDxcCompiler3* pCompiler = nullptr;

		~ThreadState()
		{
			if ( pCompiler )
			{
				pCompiler->Release();
			}

			if ( pUtils )
			{
				pUtils->Release();
			}
		}
	};

	thread_local ThreadState t_State;

	DxcCreateInstanceProc LoadDxc()
	{
		static std::once_flag s_Once;
		static DxcCreateInstanceProc s_pCreate = nullptr;

		std::call_once( s_Once, []()
		{
			wchar_t exePath[MAX_PATH];
			const DWORD len = GetModuleFileNameW( nullptr, exePath, MAX_PATH );
			if ( !len || ( len == MAX_PATH ) )
			{
				return;
			}

			const fs::path dll = fs::path( exePath ).parent_path() / L"dxcompiler.dll";
			const HMODULE module = LoadLibraryW( dll.c_str() );
			if ( module )
			{
				s_pCreate = reinterpret_cast<DxcCreateInstanceProc>( GetProcAddress( module, "DxcCreateInstance" ) );
			}
		} );

		return s_pCreate;
	}

	std::wstring Widen( std::string_view text )
	{
		std::wstring wide( text.begin(), text.end() );
		return wide;
	}

	// Keeps one line per diagnostic, DXC also prints the source line and a caret line under each
	std::string DiagnosticLines( std::string_view text )
	{
		std::string result;
		size_t pos = 0;
		while ( pos < text.size() )
		{
			size_t end = text.find( '\n', pos );
			if ( end == std::string_view::npos )
			{
				end = text.size();
			}

			const std::string_view line = text.substr( pos, end - pos );
			const bool bDiagnostic = ( line.find( " warning:" ) != std::string_view::npos ) || ( line.find( " error:" ) != std::string_view::npos ) || ( line.find( " fatal error:" ) != std::string_view::npos );
			if ( bDiagnostic )
			{
				result.append( line );
				result.push_back( '\n' );
			}

			pos = end + 1;
		}

		// Messages without a location ("Unknown argument") are kept as they are
		if ( result.empty() )
		{
			result.assign( text );
		}

		return result;
	}

	void Fail( CmdSink::IResponse*& pResponse, std::string message )
	{
		pResponse = new( std::nothrow ) DxcResponse( {}, std::move( message ), false );
	}
}

void Dxc::ExecuteCommand( const CfgProcessor::ComboBuildCommand& command, CmdSink::IResponse*& pResponse, unsigned int flags )
{
	const DxcCreateInstanceProc pCreate = LoadDxc();
	if ( !pCreate )
	{
		Fail( pResponse, "dxcompiler.dll not found next to ShaderCompile26.exe" );
		return;
	}

	if ( !t_State.pCompiler )
	{
		pCreate( CLSID_DxcUtils, IID_PPV_ARGS( &t_State.pUtils ) );
		pCreate( CLSID_DxcCompiler, IID_PPV_ARGS( &t_State.pCompiler ) );
	}

	if ( !t_State.pUtils || !t_State.pCompiler )
	{
		Fail( pResponse, "DxcCreateInstance failed" );
		return;
	}

	const std::string mainKey( command.fileName );
	const CSharedFile* pSource = fileCache.Get( mainKey );
	if ( !pSource )
	{
		Fail( pResponse, "\"" + mainKey + "\" is not in the file cache" );
		return;
	}

	std::vector<std::wstring> args;
	args.emplace_back( Widen( mainKey ) );
	args.emplace_back( L"-E" );
	args.emplace_back( Widen( command.entryPoint ) );
	args.emplace_back( L"-T" );
	args.emplace_back( Widen( command.shaderModel ) );
	args.emplace_back( L"-spirv" );
	args.emplace_back( L"-fspv-target-env=vulkan1.1" );
	args.emplace_back( L"-HV" );
	args.emplace_back( L"2021" );
	args.emplace_back( L"-I" );
	args.emplace_back( L"." );

	// fxc's "file(line,col): warning ..." shape, source and caret lines are dropped by DiagnosticLines
	args.emplace_back( L"-fdiagnostics-format=msvc" );
	args.emplace_back( ( flags & D3DCOMPILE_SKIP_OPTIMIZATION ) ? L"-Od" : L"-O3" );

	if ( flags & D3DCOMPILE_SKIP_VALIDATION )
	{
		args.emplace_back( L"-Vd" );
	}

	for ( const auto& [name, value] : command.defines )
	{
		args.emplace_back( L"-D" + Widen( name ) + L"=" + Widen( value ) );
	}

	std::vector<LPCWSTR> argPtrs;
	argPtrs.reserve( args.size() );
	for ( const std::wstring& arg : args )
	{
		argPtrs.emplace_back( arg.c_str() );
	}

	const DxcBuffer buffer{ pSource->Data(), pSource->Size(), DXC_CP_UTF8 };
	DxcInclude include( t_State.pUtils );

	IDxcResult* pResult = nullptr;
	const HRESULT hr = t_State.pCompiler->Compile( &buffer, argPtrs.data(), gsl::narrow<UINT32>( argPtrs.size() ), &include, IID_PPV_ARGS( &pResult ) );

	HRESULT status = E_FAIL;
	std::string listing;
	std::vector<char> code;

	if ( pResult )
	{
		pResult->GetStatus( &status );

		IDxcBlobUtf8* pErrors = nullptr;
		if ( SUCCEEDED( pResult->GetOutput( DXC_OUT_ERRORS, IID_PPV_ARGS( &pErrors ), nullptr ) ) && pErrors )
		{
			listing = DiagnosticLines( std::string_view( pErrors->GetStringPointer(), pErrors->GetStringLength() ) );
			pErrors->Release();
		}

		IDxcBlob* pObject = nullptr;
		if ( SUCCEEDED( status ) && SUCCEEDED( pResult->GetOutput( DXC_OUT_OBJECT, IID_PPV_ARGS( &pObject ), nullptr ) ) && pObject )
		{
			const char* pBytes = static_cast<const char*>( pObject->GetBufferPointer() );
			code.assign( pBytes, pBytes + pObject->GetBufferSize() );
			pObject->Release();
		}

		pResult->Release();
	}

	// Same file name in two folders: the SPIR-V may hold the wrong header, never accept it
	const bool bConflict = !include.GetConflict().empty();
	if ( bConflict )
	{
		listing = include.GetConflict() + listing;
	}

	const bool bSucceeded = SUCCEEDED( hr ) && SUCCEEDED( status ) && !code.empty() && !bConflict;
	pResponse = new( std::nothrow ) DxcResponse( std::move( code ), std::move( listing ), bSucceeded );
}
