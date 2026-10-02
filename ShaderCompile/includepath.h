#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace IncludePath
{
	enum class Status
	{
		Ok,
		NotFound,
		OutsideRoot
	};

	struct Result
	{
		Status status;
		std::string key;
	};

	std::optional<std::string> Normalize( const std::filesystem::path& path );
	Result Resolve( std::string_view parentKey, std::string_view incl, const std::function<bool( const std::string& )>& exists );
}
