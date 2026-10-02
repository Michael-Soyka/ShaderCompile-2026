//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======//
//
// Purpose: D3DX command implementation.
//
// $NoKeywords: $
//
//=============================================================================//

#define WIN32_LEAN_AND_MEAN
#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME
#define NOMINMAX

#include "d3dxfxc.h"

#include "basetypes.h"
#include "cfgprocessor.h"
#include "cmdsink.h"
#include "d3dcompiler.h"
#include "gsl/narrow"
#include "includepath.h"
#include <malloc.h>
#include <string>
#include <vector>

#pragma comment( lib, "D3DCompiler" )

CSharedFile::CSharedFile( std::vector<char>&& data ) noexcept : std::vector<char>( std::forward<std::vector<char>>( data ) )
{
}

void FileCache::Add( const std::string& fileName, std::vector<char>&& data )
{
	const auto [it, inserted] = m_map.try_emplace( fileName, std::move( data ) );
	if ( inserted && it->second.Data() )
		m_names.emplace( it->second.Data(), &it->first );
}

const CSharedFile* FileCache::Get( const std::string& filename ) const
{
	// Search the cache first
	const auto find = m_map.find( filename );
	if ( find != m_map.cend() )
		return &find->second;
	return nullptr;
}

const std::string* FileCache::NameOf( const void* data ) const
{
	const auto find = m_names.find( data );
	if ( find != m_names.cend() )
		return find->second;
	return nullptr;
}

void FileCache::Clear()
{
	m_names.clear();
	m_map.clear();
}

FileCache fileCache;

class DxInclude final : public ID3DInclude
{
public:
	explicit DxInclude( const std::string& mainKey ) noexcept
		: m_mainKey( mainKey )
	{
	}

	STDMETHOD( Open )( THIS_ D3D_INCLUDE_TYPE, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes ) override
	{
		const std::string* parent = fileCache.NameOf( pParentData );
		const auto exists = []( const std::string& key )
		{
			return fileCache.Get( key ) != nullptr;
		};
		const auto resolved = IncludePath::Resolve( parent ? *parent : m_mainKey, pFileName, exists );
		if ( resolved.status != IncludePath::Status::Ok )
			return E_FAIL;

		const CSharedFile* file = fileCache.Get( resolved.key );
		*ppData = file->Data();
		*pBytes = gsl::narrow<UINT>( file->Size() );

		return S_OK;
	}

	STDMETHOD( Close )( THIS_ LPCVOID ) override
	{
		return S_OK;
	}

	virtual ~DxInclude() = default;

private:
	const std::string& m_mainKey;
};

class CResponse final : public CmdSink::IResponse
{
public:
	explicit CResponse( ID3DBlob* pShader, ID3DBlob* pListing, HRESULT hr ) noexcept
		: m_pShader( pShader )
		, m_pListing( pListing )
		, m_hr( hr )
	{
	}

	~CResponse() override
	{
		if ( m_pShader )
			m_pShader->Release();

		if ( m_pListing )
			m_pListing->Release();
	}

	bool Succeeded() const noexcept override { return m_pShader && m_hr == S_OK; }
	size_t GetResultBufferLen() const override { return Succeeded() ? m_pShader->GetBufferSize() : 0; }
	const void* GetResultBuffer() const override { return Succeeded() ? m_pShader->GetBufferPointer() : nullptr; }
	const char* GetListing() const override { return static_cast<const char*>( m_pListing ? m_pListing->GetBufferPointer() : nullptr ); }

protected:
	ID3DBlob* m_pShader;
	ID3DBlob* m_pListing;
	HRESULT m_hr;
};


void Compiler::ExecuteCommand( const CfgProcessor::ComboBuildCommand& pCommand, CmdSink::IResponse* &pResponse, unsigned int flags )
{
	// Macros to be defined for D3DX
	std::vector<D3D_SHADER_MACRO> macros;
	macros.resize( pCommand.defines.size() + 1 );
	std::transform( pCommand.defines.cbegin(), pCommand.defines.cend(), macros.begin(), []( const auto& d ) { return D3D_SHADER_MACRO{ d.first.data(), d.second.data() }; } );

	ID3DBlob* pShader        = nullptr; // NOTE: Must release the COM interface later
	ID3DBlob* pErrorMessages = nullptr; // NOTE: Must release COM interface later

	const std::string mainKey( pCommand.fileName );
	const CSharedFile* source = fileCache.Get( mainKey );
	HRESULT hr = E_FAIL;
	if ( source )
	{
		DxInclude include( mainKey );
		hr = D3DCompile( source->Data(), source->Size(), mainKey.c_str(), macros.data(), &include, pCommand.entryPoint.data(), pCommand.shaderModel.data(), flags, 0, &pShader, &pErrorMessages );
	}

	pResponse = new( std::nothrow ) CResponse( pShader, pErrorMessages, hr );
}