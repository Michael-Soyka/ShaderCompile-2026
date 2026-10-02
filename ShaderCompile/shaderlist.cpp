#include "shaderlist.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <set>

#include "includepath.h"
#include "shaderparser.h"
#include "re2/re2.h"
#include "termcolor/style.hpp"
#include "termcolors.hpp"
#include "toml++/toml.hpp"

namespace fs = std::filesystem;
using namespace std::literals;

static constexpr std::string_view s_supportedVersions[] = { "30"sv, "40"sv, "41"sv, "50"sv, "51"sv };

ShaderList::VersionResult ShaderList::ResolveVersion( const std::string& fileName, const std::string& groupVersion )
{
	static const re2::RE2 suffix( R"reg(^.*_[vpgdh]s(\d\db|\d\d|\dx|xx))reg" );

	std::string version = groupVersion;
	if ( version.empty() )
	{
		if ( !re2::RE2::PartialMatch( fileName, suffix, &version ) )
		{
			VersionResult invalid{ VersionStatus::Invalid, {} };
			return invalid;
		}

		if ( version == "xx"sv )
			version = "30"s;
		else if ( version[1] == 'x' && version[0] >= '3' )
			version[1] = '0';
	}

	VersionStatus status = VersionStatus::Invalid;
	if ( std::ranges::find( s_supportedVersions, version ) != std::end( s_supportedVersions ) )
		status = VersionStatus::Ok;
	else if ( !version.empty() && version[0] >= '0' && version[0] < '3' )
		status = VersionStatus::BelowMinimum;

	VersionResult result{ status, std::move( version ) };
	return result;
}

static void Error( const std::string& message )
{
	std::cout << clr::red << "shaders.toml: "sv << message << clr::reset << std::endl;
}

std::optional<std::vector<ShaderList::Entry>> ShaderList::Load( const fs::path& root, const std::vector<std::string>& groups )
{
	const fs::path configPath = root / "shaders.toml"sv;
	if ( !fs::is_regular_file( configPath ) )
	{
		std::cout << clr::red << "Couldn't find shaders.toml in \""sv << root.string() << "\"!"sv << clr::reset << std::endl;
		return std::nullopt;
	}

	toml::table config;
	try
	{
		config = toml::parse_file( configPath.string() );
	}
	catch ( const toml::parse_error& e )
	{
		Error( std::string( e.description() ) + " (line "s + std::to_string( e.source().begin.line ) + ", column "s + std::to_string( e.source().begin.column ) + ")"s );
		return std::nullopt;
	}

	bool failed = false;
	if ( config.empty() )
	{
		Error( "no groups defined"s );
		failed = true;
	}

	for ( const std::string& group : groups )
	{
		if ( !config.contains( group ) )
		{
			Error( "unknown group \""s + group + "\" in -group"s );
			failed = true;
		}
	}

	const std::set<std::string> selected( groups.begin(), groups.end() );
	std::map<std::string, std::string> versions;
	std::map<std::string, std::string> outputs;
	std::set<std::string> added;
	std::vector<Entry> entries;

	for ( auto&& [key, node] : config )
	{
		const std::string group( key.str() );
		const toml::table* table = node.as_table();
		if ( !table )
		{
			Error( "\""s + group + "\" is not a group table"s );
			failed = true;
			continue;
		}

		std::string groupVersion;
		if ( const toml::node* versionNode = table->get( "version"sv ) )
		{
			const toml::value<std::string>* versionValue = versionNode->as_string();
			if ( !versionValue )
			{
				Error( "group \""s + group + "\": version must be a string"s );
				failed = true;
				continue;
			}
			groupVersion = versionValue->get();
		}

		const toml::array* files = table->get_as<toml::array>( "files"sv );
		if ( !files || files->empty() )
		{
			Error( "group \""s + group + "\": files must be a non-empty array"s );
			failed = true;
			continue;
		}

		for ( const toml::node& fileNode : *files )
		{
			const toml::value<std::string>* fileValue = fileNode.as_string();
			if ( !fileValue )
			{
				Error( "group \""s + group + "\": files must contain only strings"s );
				failed = true;
				continue;
			}

			const auto path = IncludePath::Normalize( fs::path( fileValue->get() ) );
			if ( !path )
			{
				Error( "\""s + fileValue->get() + "\" is absolute or leaves shader directory"s );
				failed = true;
				continue;
			}
			if ( !fs::is_regular_file( root / *path ) )
			{
				Error( "\""s + *path + "\" does not exist"s );
				failed = true;
				continue;
			}

			const std::string fileName = fs::path( *path ).filename().string();
			const auto version = ResolveVersion( fileName, groupVersion );
			if ( version.status == VersionStatus::BelowMinimum )
			{
				std::cout << clr::yellow << *path << ": version "sv << version.version << " is below minimum 30, skipping"sv << clr::reset << std::endl;
				continue;
			}
			if ( version.status == VersionStatus::Invalid )
			{
				Error( "\""s + *path + "\": unsupported or missing shader version \""s + version.version + "\""s );
				failed = true;
				continue;
			}

			const std::string target( Parser::GetTarget( fileName ) );
			if ( target.empty() )
			{
				Error( "\""s + *path + "\": can't determine shader type from file name"s );
				failed = true;
				continue;
			}

			if ( const auto [known, inserted] = versions.emplace( *path, version.version ); !inserted && known->second != version.version )
			{
				Error( "\""s + *path + "\" resolves to different versions \""s + known->second + "\" and \""s + version.version + "\" in different groups"s );
				failed = true;
				continue;
			}

			const std::string output = Parser::ConstructName( fileName, target, version.version );
			if ( const auto [other, inserted] = outputs.emplace( output, *path ); !inserted && other->second != *path )
			{
				Error( "\""s + other->second + "\" and \""s + *path + "\" both produce \""s + output + "\""s );
				failed = true;
				continue;
			}

			if ( ( selected.empty() || selected.contains( group ) ) && added.insert( *path ).second )
				entries.emplace_back( Entry{ *path, version.version, target, group } );
		}
	}

	if ( failed )
		return std::nullopt;
	return entries;
}
