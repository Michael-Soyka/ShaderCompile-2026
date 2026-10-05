#include "progress.h"

#include <algorithm>

namespace Progress
{
	Model::Model()
	{
		m_state.start = Clock::now();
	}

	void Model::Add( std::string_view name, uint64_t total )
	{
		std::lock_guard guard{ m_mutex };
		ShaderState& state = m_state.shaders.emplace_back();
		state.name = name;
		state.total = total;
	}

	void Model::SetWorkers( uint32_t workers )
	{
		std::lock_guard guard{ m_mutex };
		m_state.workers = workers;
	}

	void Model::Begin( size_t shader, int slot )
	{
		std::lock_guard guard{ m_mutex };
		ShaderState& state = m_state.shaders[shader];
		state.status = Status::Compiling;
		state.slot = slot;
		state.start = Clock::now();
	}

	void Model::Advance( size_t shader, uint64_t count )
	{
		std::lock_guard guard{ m_mutex };
		ShaderState& state = m_state.shaders[shader];
		state.done = std::min( state.done + count, state.total );
	}

	void Model::Finish( size_t shader, Status status )
	{
		std::lock_guard guard{ m_mutex };
		ShaderState& state = m_state.shaders[shader];
		state.status = status;
		state.done = state.total;
		state.end = Clock::now();
	}

	Snapshot Model::Take() const
	{
		std::lock_guard guard{ m_mutex };
		Snapshot snapshot = m_state;
		return snapshot;
	}

	bool IsFinished( Status status ) noexcept
	{
		return status == Status::Compiled || status == Status::Warnings || status == Status::Failed;
	}

	std::string FormatDuration( Clock::duration duration )
	{
		const int64_t total = std::chrono::duration_cast<std::chrono::seconds>( duration ).count();
		const int64_t hours = total / 3600;
		const int64_t minutes = ( total / 60 ) % 60;
		const int64_t seconds = total % 60;

		std::string text;
		if ( hours )
			text = std::to_string( hours ) + "h " + std::to_string( minutes ) + "m";
		else if ( minutes )
			text = std::to_string( minutes ) + "m " + std::to_string( seconds ) + "s";
		else
			text = std::to_string( seconds ) + "s";

		return text;
	}
}
