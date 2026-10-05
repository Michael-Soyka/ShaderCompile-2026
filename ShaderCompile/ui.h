#pragma once

#include "progress.h"
#include "tui.h"

#include <atomic>
#include <thread>
#include <vector>

namespace Ui
{
	[[nodiscard]] bool CanUseFullScreen();
	void RestoreTerminal() noexcept;

	class Display
	{
	public:
		Display( const Progress::Model& model, bool bFullScreen );
		~Display();

		void Stop();

	private:
		void Loop();
		void DrawFrame( const Progress::Snapshot& snapshot, Tui::RateTracker& rates );
		void PrintFinished( const Progress::Snapshot& snapshot );

		const Progress::Model& m_model;
		const bool m_bFullScreen;
		std::atomic<bool> m_bStop{ false };
		std::vector<bool> m_printed;
		std::thread m_thread;
	};
}
