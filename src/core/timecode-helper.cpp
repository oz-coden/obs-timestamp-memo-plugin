#include "timecode-helper.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <limits>

namespace {
// Quotient/remainder scaling avoids both floating-point rounding drift and
// intermediate overflow. FPS numerator/denominator are bounded to 1,000,000;
// the millisecond scaling factor (1000 * denominator) is at most 1,000,000,000.
uint64_t rounded_scale(uint64_t value, uint64_t multiplier, uint64_t divisor)
{
	const auto whole = value / divisor;
	const auto extra = ((value % divisor) * multiplier + divisor / 2) / divisor;
	const auto limit = std::numeric_limits<uint64_t>::max();
	if (whole > (limit - extra) / multiplier)
		return limit;
	return whole * multiplier + extra;
}
} // namespace

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
	if (!fps.valid())
		return 0;

	return rounded_scale(ms, fps.num, 1000ULL * fps.den);
}

uint64_t TimecodeHelper::frame_index_to_ms(uint64_t frame_index, const VideoFrameRate &fps)
{
	if (!fps.valid())
		return 0;

	return rounded_scale(frame_index, 1000ULL * fps.den, fps.num);
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
			adjusted_frame += (drop_frames * 9 * d) + drop_frames * ((m - drop_frames) / frames_per_minute);
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

uint64_t TimecodeHelper::smpte_to_frame_index(std::string_view timecode, const VideoFrameRate &fps)
{
	if (timecode.length() < 11)
		return 0;

	size_t first = timecode.find(':');
	size_t second = first == std::string_view::npos ? first : timecode.find(':', first + 1);
	size_t third = second == std::string_view::npos ? second : timecode.find_first_of(":;.", second + 1);
	if (third == std::string_view::npos)
		return 0;
	auto parse_field = [](std::string_view field, uint32_t &value) {
		auto result = std::from_chars(field.data(), field.data() + field.size(), value);
		return result.ec == std::errc() && result.ptr == field.data() + field.size();
	};
	uint32_t hh = 0, mm = 0, ss = 0, ff = 0;
	char sep = timecode[third];
	if (!parse_field(timecode.substr(0, first), hh) ||
	    !parse_field(timecode.substr(first + 1, second - first - 1), mm) ||
	    !parse_field(timecode.substr(second + 1, third - second - 1), ss) ||
	    !parse_field(timecode.substr(third + 1), ff)) {
		return 0;
	}

	double nominal_fps = std::round(fps.fps());
	if (nominal_fps <= 0.0 || nominal_fps > 1000.0 || mm >= 60 || ss >= 60 || ff >= nominal_fps)
		return 0;

	if (sep != ':' && sep != ';' && sep != '.')
		return 0;
	bool is_df = (sep == ';' || sep == '.');
	if (is_df && !fps.is_drop_frame())
		return 0;

	if (is_df) {
		uint64_t drop_frames = (nominal_fps >= 50.0) ? 4 : 2;
		if (mm % 10 != 0 && ss == 0 && ff < drop_frames)
			return 0;
		uint64_t total_minutes = static_cast<uint64_t>(hh) * 60 + mm;
		uint64_t total_frames = ((total_minutes * 60 + ss) * static_cast<uint64_t>(nominal_fps)) + ff;
		uint64_t dropped = drop_frames * (total_minutes - (total_minutes / 10));
		if (total_frames >= dropped) {
			return total_frames - dropped;
		}
		return 0;
	} else {
		uint64_t base_fps = static_cast<uint64_t>(nominal_fps);
		return (static_cast<uint64_t>(hh) * 3600 + static_cast<uint64_t>(mm) * 60 + ss) * base_fps + ff;
	}
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
		std::snprintf(buf, sizeof(buf), "%02llu:%02llu:%02llu.%03llu", static_cast<unsigned long long>(hour),
			      static_cast<unsigned long long>(min), static_cast<unsigned long long>(sec),
			      static_cast<unsigned long long>(msec));
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
