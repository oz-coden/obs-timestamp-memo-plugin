#include "timecode-helper.hpp"

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>

bool VideoFrameRate::is_drop_frame() const
{
	return TimecodeHelper::is_drop_frame_rate(num, den);
}

bool TimecodeHelper::is_drop_frame_rate(uint32_t num, uint32_t den)
{
	if (den == 0)
		return false;

	if ((num == 30000 && den == 1001) || (num == 60000 && den == 1001)) {
		return true;
	}

	double fps = static_cast<double>(num) / static_cast<double>(den);
	if (std::abs(fps - 29.97002997) < 0.01 || std::abs(fps - 59.94005994) < 0.01) {
		return true;
	}

	return false;
}

uint64_t TimecodeHelper::ms_to_frame_index(uint64_t ms, const VideoFrameRate &fps)
{
	if (fps.den == 0 || fps.num == 0)
		return 0;

	double seconds = static_cast<double>(ms) / 1000.0;
	double frames = seconds * (static_cast<double>(fps.num) / static_cast<double>(fps.den));
	return static_cast<uint64_t>(std::llround(frames));
}

uint64_t TimecodeHelper::frame_index_to_ms(uint64_t frame_index, const VideoFrameRate &fps)
{
	if (fps.den == 0 || fps.num == 0)
		return 0;

	double seconds =
		(static_cast<double>(frame_index) * static_cast<double>(fps.den)) / static_cast<double>(fps.num);
	return static_cast<uint64_t>(std::llround(seconds * 1000.0));
}

std::string TimecodeHelper::frame_index_to_smpte(uint64_t frame_index, const VideoFrameRate &fps, bool force_ndf)
{
	bool use_df = !force_ndf && fps.is_drop_frame();
	char buf[32];

	if (use_df) {
		double nominal_fps = std::round(fps.fps());
		uint64_t drop_frames = 2;
		if (nominal_fps >= 50.0) {
			drop_frames = 4;
		}

		uint64_t frames_per_minute = static_cast<uint64_t>(nominal_fps * 60) - drop_frames;
		uint64_t frames_per_10minutes = frames_per_minute * 10 + drop_frames;

		uint64_t d = frame_index / frames_per_10minutes;
		uint64_t m = frame_index % frames_per_10minutes;

		uint64_t adjusted_frame = frame_index;
		if (m > drop_frames) {
			adjusted_frame +=
				(drop_frames * 9 * d) + drop_frames * ((m - drop_frames) / frames_per_minute);
		} else {
			adjusted_frame += (drop_frames * 9 * d);
		}

		uint64_t base_fps = static_cast<uint64_t>(nominal_fps);
		uint64_t ff = adjusted_frame % base_fps;
		uint64_t ss = (adjusted_frame / base_fps) % 60;
		uint64_t mm = (adjusted_frame / (base_fps * 60)) % 60;
		uint64_t hh = adjusted_frame / (base_fps * 3600);

		std::snprintf(buf, sizeof(buf), "%02llu:%02llu:%02llu;%02llu", static_cast<unsigned long long>(hh),
			      static_cast<unsigned long long>(mm), static_cast<unsigned long long>(ss),
			      static_cast<unsigned long long>(ff));
	} else {
		uint64_t nominal_fps = static_cast<uint64_t>(std::round(fps.fps()));
		if (nominal_fps == 0)
			nominal_fps = 30;

		uint64_t ff = frame_index % nominal_fps;
		uint64_t ss = (frame_index / nominal_fps) % 60;
		uint64_t mm = (frame_index / (nominal_fps * 60)) % 60;
		uint64_t hh = frame_index / (nominal_fps * 3600);

		std::snprintf(buf, sizeof(buf), "%02llu:%02llu:%02llu:%02llu", static_cast<unsigned long long>(hh),
			      static_cast<unsigned long long>(mm), static_cast<unsigned long long>(ss),
			      static_cast<unsigned long long>(ff));
	}

	return std::string(buf);
}

std::string TimecodeHelper::ms_to_smpte(uint64_t ms, const VideoFrameRate &fps, bool force_ndf)
{
	uint64_t frame_index = ms_to_frame_index(ms, fps);
	return frame_index_to_smpte(frame_index, fps, force_ndf);
}

std::string TimecodeHelper::ms_to_timestamp_str(uint64_t ms, bool include_ms)
{
	uint64_t msec = ms % 1000;
	uint64_t total_sec = ms / 1000;
	uint64_t sec = total_sec % 60;
	uint64_t min = (total_sec / 60) % 60;
	uint64_t hour = total_sec / 3600;

	char buf[32];
	if (include_ms) {
		std::snprintf(buf, sizeof(buf), "%02llu:%02llu:%02llu.%03llu",
			      static_cast<unsigned long long>(hour), static_cast<unsigned long long>(min),
			      static_cast<unsigned long long>(sec), static_cast<unsigned long long>(msec));
	} else {
		std::snprintf(buf, sizeof(buf), "%02llu:%02llu:%02llu", static_cast<unsigned long long>(hour),
			      static_cast<unsigned long long>(min), static_cast<unsigned long long>(sec));
	}
	return std::string(buf);
}

std::string TimecodeHelper::ms_to_srt_time(uint64_t ms)
{
	uint64_t msec = ms % 1000;
	uint64_t total_sec = ms / 1000;
	uint64_t sec = total_sec % 60;
	uint64_t min = (total_sec / 60) % 60;
	uint64_t hour = total_sec / 3600;

	char buf[32];
	std::snprintf(buf, sizeof(buf), "%02llu:%02llu:%02llu,%03llu", static_cast<unsigned long long>(hour),
		      static_cast<unsigned long long>(min), static_cast<unsigned long long>(sec),
		      static_cast<unsigned long long>(msec));
	return std::string(buf);
}
