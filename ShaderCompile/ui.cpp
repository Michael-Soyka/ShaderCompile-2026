#include "ui.h"

#include <Windows.h>
#include <io.h>
#include <cstdio>
#include <iostream>
#include <string_view>

#include "termcolor/style.hpp"
#include "termcolors.hpp"

using namespace std::literals;

namespace
{
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
	}

	Display::Display( const Progress::Model& model, bool bFullScreen )
		: m_model( model ), m_bFullScreen( bFullScreen )
	{
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
		PrintFinished( m_model.Take() );
	}

	void Display::Loop()
	{
		while ( !m_bStop.load() )
		{
			PrintFinished( m_model.Take() );
			std::this_thread::sleep_for( 250ms );
		}
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
