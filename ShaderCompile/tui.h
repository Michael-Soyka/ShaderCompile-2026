#pragma once

#include "progress.h"

#include <deque>
#include <string>
#include <unordered_map>

namespace Tui
{
	class RateTracker
	{
	public:
		void Sample( const Progress::Snapshot& snapshot, Progress::Clock::time_point now );
		[[nodiscard]] double Rate( size_t shader ) const;

	private:
		struct Point
		{
			Progress::Clock::time_point time;
			uint64_t done;
		};

		std::unordered_map<size_t, std::deque<Point>> m_history;
	};

	[[nodiscard]] std::string RenderFrame( const Progress::Snapshot& snapshot, const RateTracker& rates, int width, int height, Progress::Clock::time_point now );
}
