#include "recording-session.hpp"
#include "presentation-clock.hpp"
#include "recording-timeline.hpp"
#include "marker-edit-history.hpp"
#include "text-formats.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)
int main()
{
	try {
		for (auto fps : {VideoFrameRate{30000, 1001}, VideoFrameRate{60000, 1001}, VideoFrameRate{24000, 1001},
				 VideoFrameRate{60, 1}}) {
			CHECK(fps.valid());
			for (auto frame : {0ULL, 1798ULL, 1800ULL, 17982ULL, 107892ULL, 215784ULL})
				for (bool ndf : {false, true})
					CHECK(TimecodeHelper::smpte_to_frame_index(
						      TimecodeHelper::frame_index_to_smpte(frame, fps, ndf), fps) ==
					      frame);
		}
		CHECK(!(VideoFrameRate{0, 1}).valid());
		CHECK(!(VideoFrameRate{60, 0}).valid());
		const auto maximum = std::numeric_limits<uint64_t>::max();
		CHECK(TimecodeHelper::ms_to_frame_index(maximum, {1000, 1}) == maximum);
		CHECK(TimecodeHelper::frame_index_to_ms(maximum, {1, 1}) == maximum);
		CHECK(TimecodeHelper::ms_to_frame_index(1728000000ULL, {60000, 1001}) == 103576424ULL);
		CHECK(TimecodeHelper::frame_index_to_ms(103576424ULL, {60000, 1001}) == 1728000007ULL);
		CHECK(TimecodeHelper::smpte_to_frame_index("00:01:00;00", {30000, 1001}) == 0);
		CHECK(TimecodeHelper::smpte_to_frame_index("00:00:01:xx", {60, 1}) == 0);
		PresentationClock clock;
		clock.reset(1000000000ULL, false);
		CHECK(!clock.ready() && clock.time(2000000000ULL) == 0);
		CHECK(clock.observe(1000000000ULL, 0, 60000));
		CHECK(clock.time(2500000000ULL) == 1500000000ULL);
		// An encoded packet arrives half a second late; its CTS, not its arrival,
		// preserves the current presentation position.
		CHECK(clock.observe(2000000000ULL, 60000, 60000));
		CHECK(clock.time(2500000000ULL) == 1500000000ULL);
		CHECK(!clock.observe(1500000000ULL, 30000, 60000));
		clock.pause(2500000000ULL);
		CHECK(clock.time(9000000000ULL) == 1500000000ULL);
		CHECK(!clock.observe(2500000000ULL, 90000, 60000));
		clock.resume(9000000000ULL);
		CHECK(clock.time(10000000000ULL) == 2500000000ULL);
		CHECK(!clock.observe(2400000000ULL, 84000, 60000));
		CHECK(clock.observe(9500000000ULL, 120000, 60000));
		CHECK(clock.time(10000000000ULL) == 2500000000ULL);
		CHECK(TimecodeHelper::ns_to_frame_index(1001000000ULL, {60000, 1001}) == 60);
		CHECK(TimecodeHelper::ns_to_frame_index(1001000000ULL, {30000, 1001}) == 30);
		CHECK(TimecodeHelper::ns_to_frame_index(2002000000ULL, {60000, 1001}) == 120);
		CHECK(TimecodeHelper::ns_to_frame_index(UINT64_MAX, {1000, 1}) == 18446744073710ULL);
		CHECK(!PresentationClock::pts_to_ns(-1, 60));
		CHECK(!PresentationClock::pts_to_ns(INT64_MAX, 1));
		CHECK(!PresentationClock::pts_to_ns(10, 0));
		clock.reset(1000000000ULL, true);
		CHECK(clock.ready() && clock.time(2000000000ULL) == 1000000000ULL);

		RecordingTimeline timeline;
		CHECK(!timeline.pause() && !timeline.split(10));
		CHECK(timeline.start() && !timeline.start());
		CHECK(timeline.pause() && !timeline.pause());
		CHECK(timeline.relative_frames(600) == 600);
		CHECK(timeline.split(600) && timeline.paused());
		CHECK(timeline.relative_frames(660) == 60 && timeline.relative_frames(599) == 0);
		CHECK(!timeline.split(599));
		CHECK(timeline.resume() && !timeline.resume());
		CHECK(timeline.stop() && !timeline.stop());
		CHECK(timeline.start() && timeline.relative_frames(600) == 600);
		RecordingSession session;
		session.replace({"video.mkv", "id", "2026-10-02", {60000, 1001}, 1280, 720});
		auto marker = session.add_marker_at_frame(60, 1, "Chapter", "#abcdef", "memo", false, "created");
		CHECK(marker.timestamp_ms == 1001 && marker.frame_index == 60 && marker.created_at_utc == "created");
		auto snapshot = session;
		CHECK(session.update_marker(marker.id, "Edited", "#123456", "changed", 3));
		CHECK(session.get_markers().front().type_index == 3);
		CHECK(snapshot.get_markers().front().comment == "memo");
		CHECK(!session.update_marker(999, "", "", "") && !session.delete_marker(999));
		CHECK(session.delete_marker(marker.id));
		auto second = session.add_marker_at_frame(120, 0, "late", "", "<&>\ntext", true);
		session.clear_markers();
		auto third = session.add_marker_at_frame(0, 0, "start", "", "first", false);
		CHECK(third.id > second.id);
		auto exhausted = snapshot;
		auto last = marker;
		last.id = std::numeric_limits<uint32_t>::max();
		exhausted.replace({"video.mkv", "id", "", {60000, 1001}, 1920, 1080}, {last});
		CHECK(exhausted.add_marker_at_frame(60, 0, "", "", "", false).id == 0);
		CHECK(exhausted.get_markers().size() == 1);
		session.add_marker_at_frame(120, 0, "later", "", "x", false);
		session.add_marker_at_frame(130, 0, "same", "", "y", false);
		CHECK(TextFormats::generate_chapters(session).find(" / same") != std::string::npos);
		CHECK(TextFormats::generate_markdown(snapshot).find("memo") != std::string::npos);
		RecordingSession public_memo;
		public_memo.replace({"C:\\Users\\private-name\\clip`|.mkv", "id", "2026-10-03", {60, 1}, 1920, 1080});
		public_memo.add_marker(1000, 0, "**label**|`", "#abcdef", "line1\nline2 <tag> `code`", false);
		const auto markdown = TextFormats::generate_markdown(public_memo);
		public_memo.add_marker(2000, 0, "CR", "", "a\rb\r\nc", false);
		CHECK(TextFormats::generate_markdown(public_memo).find("a<br>b<br>c") != std::string::npos);
		CHECK(markdown.find("private-name") == std::string::npos);
		CHECK(markdown.find("clip\\`\\|\\.mkv") != std::string::npos);
		CHECK(markdown.find("\\*\\*label\\*\\*\\|\\`") != std::string::npos);
		CHECK(markdown.find("line1<br>line2 &lt;tag&gt; \\`code\\`") != std::string::npos);
		RecordingSession subtitles;
		subtitles.add_marker(2000, 0, "later", "", "", false);
		subtitles.add_marker(1000, 0, "first", "", "a\nb", false);
		subtitles.add_marker(1000, 0, "second", "", "", false);
		const auto cues = TextFormats::subtitle_cues(subtitles);
		CHECK(cues.size() == 3 && cues[0].start_ms == 1000 && cues[0].end_ms == 2000);
		CHECK(cues[0].text == "[first] a\nb" && cues[1].text == "[second]");
		CHECK(cues[1].end_ms == 2000 && cues[2].end_ms == 4500);
		CHECK(subtitles.get_markers().front().timestamp_ms == 2000);
		CHECK(TextFormats::subtitle_cues(RecordingSession{}).empty());
		MarkerEditHistory edits;
		for (size_t i = 0; i < MarkerEditHistory::max_commands + 5; ++i)
			CHECK(edits.record({{0, std::nullopt, marker}}));
		size_t available = 0;
		while (edits.undo_command()) {
			edits.did_undo();
			++available;
		}
		CHECK(available == MarkerEditHistory::max_commands);
		CHECK(edits.redo_command());
		CHECK(edits.record({{0, std::nullopt, marker}}) && !edits.redo_command());
		auto oversized = marker;
		oversized.comment.assign(MarkerEditHistory::max_bytes, 'x');
		CHECK(!edits.record({{0, std::nullopt, oversized}}) && !edits.undo_command());
		std::cout
			<< "PASS pure FPS/timecodes, lifecycle/segments, document operations/snapshots and text export\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
