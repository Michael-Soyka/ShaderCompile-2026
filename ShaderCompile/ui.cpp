#include "ui.h"

#include <Windows.h>
#include <io.h>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string_view>

#include "termcolor/style.hpp"
#include "termcolors.hpp"

using namespace std::literals;

namespace
{
	std::atomic<bool> g_bFullScreenActive{ false };
	std::timed_mutex g_mtxScreen;
	UINT g_oldCodePage = 0;

	void WriteRaw( std::string_view text ) noexcept
	{
		DWORD written = 0;
		WriteFile( GetStdHandle( STD_OUTPUT_HANDLE ), text.data(), static_cast<DWORD>( text.size() ), &written, nullptr );
	}

	bool GetWindowSize( int& width, int& height )
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if ( !GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &csbi ) )
			return false;

		width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
		return true;
	}
}

namespace Ui
{
	bool CanUseFullScreen()
	{
		if ( !_isatty( _fileno( stdout ) ) )
			return false;

		int width = 0;
		int height = 0;
		if ( !GetWindowSize( width, height ) )
			return false;

		return width >= 60 && height >= 10;
	}

	void RestoreTerminal() noexcept
	{
		const bool bLocked = g_mtxScreen.try_lock_for( std::chrono::milliseconds( 500 ) );
		if ( g_bFullScreenActive.exchange( false ) )
		{
			WriteRaw( "\033[0m\033[?25h\033[?1049l" );
			SetConsoleOutputCP( g_oldCodePage );
		}

		if ( bLocked )
			g_mtxScreen.unlock();
	}

	Display::Display( const Progress::Model& model, bool bFullScreen )
		: m_model( model ), m_bFullScreen( bFullScreen )
	{
		if ( m_bFullScreen )
		{
			std::cout.flush();
			g_oldCodePage = GetConsoleOutputCP();
			SetConsoleOutputCP( CP_UTF8 );
			WriteRaw( "\033[?1049h\033[?25l\033[2J" );
			g_bFullScreenActive = true;
		}

		m_thread = std::thread( &Display::Loop, this );
	}

	Display::~Display()
	{
		Stop();
	}

	void Display::Stop()
	{
		if ( !m_thread.joinable() )
			return;

		m_bStop = true;
		m_thread.join();
		if ( m_bFullScreen )
			RestoreTerminal();

		PrintFinished( m_model.Take() );
	}

	void Display::Loop()
	{
		Tui::RateTracker rates;
		while ( !m_bStop.load() )
		{
			const Progress::Snapshot snapshot = m_model.Take();
			if ( m_bFullScreen )
			{
				DrawFrame( snapshot, rates );
				std::this_thread::sleep_for( 100ms );
			}
			else
			{
				PrintFinished( snapshot );
				std::this_thread::sleep_for( 250ms );
			}
		}
	}

	void Display::DrawFrame( const Progress::Snapshot& snapshot, Tui::RateTracker& rates )
	{
		int width = 0;
		int height = 0;
		if ( !GetWindowSize( width, height ) || width < 20 || height < 4 )
			return;

		const Progress::Clock::time_point now = Progress::Clock::now();
		rates.Sample( snapshot, now );
		const std::string frame = Tui::RenderFrame( snapshot, rates, width - 1, height, now );
		std::lock_guard guard{ g_mtxScreen };
		if ( g_bFullScreenActive.load() )
			WriteRaw( frame );
	}

	void Display::PrintFinished( const Progress::Snapshot& snapshot )
	{
		m_printed.resize( snapshot.shaders.size(), false );
		for ( size_t i = 0; i < snapshot.shaders.size(); ++i )
		{
			const Progress::ShaderState& shader = snapshot.shaders[i];
			if ( m_printed[i] || !Progress::IsFinished( shader.status ) )
				continue;

			m_printed[i] = true;
			const std::string duration = Progress::FormatDuration( shader.end - shader.start );
			switch ( shader.status )
			{
				case Progress::Status::Failed:
					std::cout << clr::red << shader.name << clr::reset << " failed"sv << std::endl;
					break;
				case Progress::Status::Warnings:
					std::cout << clr::yellow << shader.name << clr::reset << " compiled in "sv << duration << " with warnings"sv << std::endl;
					break;
				default:
					std::cout << clr::green << shader.name << clr::reset << " compiled in "sv << duration << std::endl;
					break;
			}
		}
	}
}
