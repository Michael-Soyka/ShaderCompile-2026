#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Progress
{
	using Clock = std::chrono::steady_clock;

	enum class Status
	{
		Queued,
		Compiling,
		Compiled,
		Warnings,
		Failed
	};

	struct ShaderState
	{
		std::string name;
		uint64_t total = 0;
		uint64_t done = 0;
		Status status = Status::Queued;
		int slot = -1;
		Clock::time_point start;
		Clock::time_point end;
	};

	struct Snapshot
	{
		std::vector<ShaderState> shaders;
		uint32_t workers = 0;
		Clock::time_point start;
	};

	class Model
	{
	public:
		Model();

		void Add( std::string_view name, uint64_t total );
		void SetWorkers( uint32_t workers );
		void Begin( size_t shader, int slot );
		void Advance( size_t shader, uint64_t count );
		void Finish( size_t shader, Status status );

		[[nodiscard]] Snapshot Take() const;

	private:
		mutable std::mutex m_mutex;
		Snapshot m_state;
	};

	[[nodiscard]] bool IsFinished( Status status ) noexcept;
	[[nodiscard]] std::string FormatDuration( Clock::duration duration );
}
