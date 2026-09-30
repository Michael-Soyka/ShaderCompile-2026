#include "includepath.h"

namespace fs = std::filesystem;

std::optional<std::string> IncludePath::Normalize( const fs::path& path )
{
	if ( path.has_root_path() )
		return std::nullopt;

	const fs::path normal = path.lexically_normal();
	if ( normal.empty() || normal == "." || *normal.begin() == ".." )
		return std::nullopt;

	std::string key = normal.generic_string();
	return key;
}

IncludePath::Result IncludePath::Resolve( std::string_view parentKey, std::string_view incl, const std::function<bool( const std::string& )>& exists )
{
	const fs::path inclPath( incl );
	const fs::path candidates[] = { fs::path( parentKey ).parent_path() / inclPath, inclPath };

	bool outside = false;
	for ( const auto& candidate : candidates )
	{
		const auto key = Normalize( candidate );
		if ( !key )
		{
			outside = true;
			continue;
		}

		if ( exists( *key ) )
		{
			Result found{ Status::Ok, *key };
			return found;
		}
	}

	Result failed{ outside ? Status::OutsideRoot : Status::NotFound, {} };
	return failed;
}
