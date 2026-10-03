#pragma once

#include <cstdint>
#include <string>
#include <string_view>

struct VideoFrameRate {
	uint32_t num = 60;
	uint32_t den = 1;

	double fps() const { return den == 0 ? 0.0 : static_cast<double>(num) / static_cast<double>(den); }

	bool valid() const
	{
		return num > 0 && num <= 1000000 && den > 0 && den <= 1000000 && fps() >= 1.0 && fps() <= 1000.0;
	}
	bool is_drop_frame() const;
};

class TimecodeHelper {
public:
	static uint64_t ns_to_frame_index(uint64_t ns, const VideoFrameRate &fps)
	{
		if (!fps.valid())
			return 0;
		// Split before multiplying so the full uint64_t nanosecond range is safe.
		const auto units = (ns / 1000000000ULL) * fps.num;
		const auto denominator = 1000000000ULL * fps.den;
		const auto remainder = (units % fps.den) * 1000000000ULL + (ns % 1000000000ULL) * fps.num;
		return units / fps.den + (remainder + denominator / 2) / denominator;
	}
	static uint64_t ms_to_frame_index(uint64_t ms, const VideoFrameRate &fps);
	static uint64_t frame_index_to_ms(uint64_t frame_index, const VideoFrameRate &fps);
	static std::string frame_index_to_smpte(uint64_t frame_index, const VideoFrameRate &fps,
						bool force_ndf = false);
	static std::string ms_to_smpte(uint64_t ms, const VideoFrameRate &fps, bool force_ndf = false);
	static uint64_t smpte_to_frame_index(std::string_view timecode, const VideoFrameRate &fps);
	static std::string ms_to_timestamp_str(uint64_t ms, bool include_ms = true);
	static std::string ms_to_srt_time(uint64_t ms);
	static bool is_drop_frame_rate(uint32_t num, uint32_t den);
};
