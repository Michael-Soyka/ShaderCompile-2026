#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ShaderList
{
	struct Entry
	{
		std::string path;
		std::string version;
		std::string target;
		std::string group;
	};

	enum class VersionStatus
	{
		Ok,
		BelowMinimum,
		Invalid
	};

	struct VersionResult
	{
		VersionStatus status;
		std::string version;
	};

	VersionResult ResolveVersion( const std::string& fileName, const std::string& groupVersion );
	std::optional<std::vector<Entry>> Load( const std::filesystem::path& root, const std::vector<std::string>& groups );
}
