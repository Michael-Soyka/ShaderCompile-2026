#include "tui.h"

#include <algorithm>
#include <string_view>
#include <vector>

using namespace std::literals;

namespace
{
	constexpr std::string_view kReset      = "\033[0m";
	constexpr std::string_view kGreen      = "\033[38;2;33;201;41m";
	constexpr std::string_view kBlue       = "\033[38;2;70;110;235m";
	constexpr std::string_view kCyan       = "\033[38;2;135;206;235m";
	constexpr std::string_view kGray       = "\033[38;2;128;128;128m";
	constexpr std::string_view kYellow     = "\033[38;2;230;190;40m";
	constexpr std::string_view kRed        = "\033[38;2;222;12;17m";
	constexpr std::string_view kWhite      = "\033[38;2;255;255;255m";
	constexpr std::string_view kBarFull    = "\xE2\x96\x88";
	constexpr std::string_view kBarEmpty   = "\xE2\x96\x91";
	constexpr std::string_view kHorizontal = "\xE2\x94\x80";
	constexpr int kBarWidth = 10;
	constexpr auto kRateWindow = 5s;

	class Line
	{
	public:
		explicit Line( int width )
			: m_width( width )
		{
		}

		void Text( std::string_view text, std::string_view color = {} )
		{
			const int room = m_width - m_visible;
			if ( room <= 0 || text.empty() )
				return;

			const std::string_view cut = text.substr( 0, static_cast<size_t>( room ) );
			Append( cut, color, static_cast<int>( cut.size() ) );
		}

		void Glyph( std::string_view glyph, int count, std::string_view color = {} )
		{
			const int fit = std::min( count, m_width - m_visible );
			if ( fit <= 0 )
				return;

			std::string run;
			for ( int i = 0; i < fit; ++i )
				run += glyph;

			Append( run, color, fit );
		}

		void PadTo( int column )
		{
			const int target = std::min( column, m_width );
			if ( target <= m_visible )
				return;

			m_text.append( static_cast<size_t>( target - m_visible ), ' ' );
			m_visible = target;
		}

		[[nodiscard]] const std::string& Str() const noexcept
		{
			return m_text;
		}

	private:
		void Append( std::string_view text, std::string_view color, int visible )
		{
			if ( !color.empty() )
				m_text += color;

			m_text += text;
			if ( !color.empty() )
				m_text += kReset;

			m_visible += visible;
		}

		std::string m_text;
		int m_width;
		int m_visible = 0;
	};

	struct Word
	{
		std::string_view text;
		std::string_view color;
	};

	std::string_view StatusColor( Progress::Status status )
	{
		switch ( status )
		{
			case Progress::Status::Compiled:
				return kWhite;
			case Progress::Status::Warnings:
				return kYellow;
			case Progress::Status::Failed:
				return kRed;
			case Progress::Status::Compiling:
				return kGreen;
			default:
				return kGray;
		}
	}

	std::string Plural( uint64_t count, std::string_view word )
	{
		std::string text = std::to_string( count ) + " " + std::string( word );
		if ( count != 1 )
			text += "s";

		return text;
	}

	Line SlotLine( const Progress::Snapshot& snapshot, const Tui::RateTracker& rates, int slot, int width )
	{
		Line line( width );
		const Progress::ShaderState* pActive = nullptr;
		const Progress::ShaderState* pLast = nullptr;
		size_t activeIndex = 0;
		for ( size_t i = 0; i < snapshot.shaders.size(); ++i )
		{
			const Progress::ShaderState& shader = snapshot.shaders[i];
			if ( shader.slot != slot )
				continue;

			if ( shader.status == Progress::Status::Compiling )
			{
				pActive = &shader;
				activeIndex = i;
				break;
			}

			if ( Progress::IsFinished( shader.status ) && ( !pLast || shader.end > pLast->end ) )
				pLast = &shader;
		}

		if ( pActive )
		{
			const uint64_t remaining = pActive->total - pActive->done;
			const double rate = rates.Rate( activeIndex );
			const int filled = pActive->total ? static_cast<int>( ( pActive->done * kBarWidth ) / pActive->total ) : 0;

			std::string eta = "--";
			if ( rate > 0.0 )
				eta = Progress::FormatDuration( std::chrono::duration_cast<Progress::Clock::duration>( std::chrono::duration<double>( static_cast<double>( remaining ) / rate ) ) );

			line.Text( pActive->name, kGreen );
			line.Text( " - " );
			line.Glyph( kBarFull, filled, kWhite );
			line.Glyph( kBarEmpty, kBarWidth - filled, kGray );
			line.Text( " " );
			line.Text( std::to_string( remaining ), kBlue );
			line.Text( " remaining - " );
			line.Text( std::to_string( static_cast<uint64_t>( rate + 0.5 ) ), kCyan );
			line.Text( " c/s - est. " );
			line.Text( eta, kGreen );
		}
		else if ( pLast )
		{
			if ( pLast->status == Progress::Status::Failed )
			{
				line.Text( pLast->name, kRed );
				line.Text( " failed" );
			}
			else
			{
				line.Text( pLast->name, pLast->status == Progress::Status::Warnings ? kYellow : kGreen );
				line.Text( " compiled in " );
				line.Text( Progress::FormatDuration( pLast->end - pLast->start ), kGreen );
			}
		}

		return line;
	}

	std::vector<std::vector<Word>> WrapQueue( const Progress::Snapshot& snapshot, int width, size_t& focusLine )
	{
		std::vector<std::vector<Word>> lines( 1 );
		int used = 0;
		bool bFocusSet = false;
		focusLine = 0;
		for ( const Progress::ShaderState& shader : snapshot.shaders )
		{
			const int length = static_cast<int>( shader.name.size() );
			if ( used && used + 1 + length > width )
			{
				lines.emplace_back();
				used = 0;
			}

			used += used ? length + 1 : length;
			lines.back().push_back( { shader.name, StatusColor( shader.status ) } );
			if ( !bFocusSet && !Progress::IsFinished( shader.status ) )
			{
				focusLine = lines.size() - 1;
				bFocusSet = true;
			}
		}

		return lines;
	}

	void MoveTo( std::string& frame, int row )
	{
		frame += "\033[";
		frame += std::to_string( row + 1 );
		frame += ";1H\033[2K";
	}
}

namespace Tui
{
	void RateTracker::Sample( const Progress::Snapshot& snapshot, Progress::Clock::time_point now )
	{
		for ( size_t i = 0; i < snapshot.shaders.size(); ++i )
		{
			const Progress::ShaderState& shader = snapshot.shaders[i];
			if ( shader.status != Progress::Status::Compiling )
			{
				m_history.erase( i );
				continue;
			}

			std::deque<Point>& points = m_history[i];
			points.push_back( { now, shader.done } );
			while ( points.size() > 1 && now - points.front().time > kRateWindow )
				points.pop_front();
		}
	}

	double RateTracker::Rate( size_t shader ) const
	{
		const auto find = m_history.find( shader );
		if ( find == m_history.end() || find->second.size() < 2 )
			return 0.0;

		const Point& first = find->second.front();
		const Point& last = find->second.back();
		const double seconds = std::chrono::duration<double>( last.time - first.time ).count();
		if ( seconds < 0.5 )
			return 0.0;

		const double rate = static_cast<double>( last.done - first.done ) / seconds;
		return rate;
	}

	std::string RenderFrame( const Progress::Snapshot& snapshot, const RateTracker& rates, int width, int height, Progress::Clock::time_point now )
	{
		const int slotRows = std::min( static_cast<int>( snapshot.workers ), height - 2 );
		const int queueRows = std::max( height - slotRows - 3, 0 );

		size_t focusLine = 0;
		std::vector<std::vector<Word>> queue;
		if ( queueRows )
			queue = WrapQueue( snapshot, width, focusLine );

		size_t firstLine = 0;
		if ( queue.size() > static_cast<size_t>( queueRows ) )
		{
			const size_t third = static_cast<size_t>( queueRows ) / 3;
			const size_t lastFirst = queue.size() - static_cast<size_t>( queueRows );
			firstLine = std::min( focusLine > third ? focusLine - third : 0, lastFirst );
		}

		std::string frame;
		int row = 0;
		for ( ; row < queueRows; ++row )
		{
			MoveTo( frame, row );

			Line line( width );
			const size_t lineIndex = firstLine + static_cast<size_t>( row );
			if ( lineIndex < queue.size() )
			{
				bool bFirst = true;
				for ( const Word& word : queue[lineIndex] )
				{
					if ( !bFirst )
						line.Text( " " );

					line.Text( word.text, word.color );
					bFirst = false;
				}
			}

			frame += line.Str();
		}

		Line separator( width );
		separator.Glyph( kHorizontal, width, kGray );
		if ( queueRows )
		{
			MoveTo( frame, row++ );
			frame += separator.Str();
		}

		for ( int slot = 0; slot < slotRows; ++slot, ++row )
		{
			MoveTo( frame, row );
			frame += SlotLine( snapshot, rates, slot, width ).Str();
		}

		for ( ; row < height - 2; ++row )
			MoveTo( frame, row );

		MoveTo( frame, height - 2 );
		frame += separator.Str();

		uint64_t compiled = 0;
		uint64_t warnings = 0;
		uint64_t failed = 0;
		for ( const Progress::ShaderState& shader : snapshot.shaders )
		{
			if ( shader.status == Progress::Status::Compiled )
				++compiled;
			else if ( shader.status == Progress::Status::Warnings )
				++warnings;
			else if ( shader.status == Progress::Status::Failed )
				++failed;
		}

		const uint64_t finished = compiled + warnings + failed;
		const std::string elapsed = Progress::FormatDuration( now - snapshot.start ) + " elapsed";

		MoveTo( frame, height - 1 );
		Line footer( width );
		footer.Text( std::to_string( finished ) + "/" + std::to_string( snapshot.shaders.size() ) + " shaders - " );
		footer.Text( std::to_string( compiled ) + " success", kGreen );
		footer.Text( " | " );
		footer.Text( Plural( warnings, "warning" ), kYellow );
		footer.Text( " | " );
		footer.Text( Plural( failed, "error" ), kRed );
		footer.PadTo( width - static_cast<int>( elapsed.size() ) );
		footer.Text( elapsed );
		frame += footer.Str();

		return frame;
	}
}
