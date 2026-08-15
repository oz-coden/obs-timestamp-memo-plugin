#pragma once

#include <cstdint>
#include <string>

struct VideoFrameRate {
	uint32_t num = 60;
	uint32_t den = 1;

	double fps() const
	{
		return den == 0 ? 0.0 : static_cast<double>(num) / static_cast<double>(den);
	}

	bool is_drop_frame() const;
};

class TimecodeHelper {
public:
	// ミリ秒からフレーム番号を計算（端数fps対応）
	static uint64_t ms_to_frame_index(uint64_t ms, const VideoFrameRate &fps);

	// フレーム番号からミリ秒を計算
	static uint64_t frame_index_to_ms(uint64_t frame_index, const VideoFrameRate &fps);

	// フレーム番号からSMPTEタイムコード文字列を計算（NDF: HH:MM:SS:FF, DF: HH:MM:SS;FF）
	static std::string frame_index_to_smpte(uint64_t frame_index, const VideoFrameRate &fps, bool force_ndf = false);

	// ミリ秒からSMPTEタイムコード文字列を計算
	static std::string ms_to_smpte(uint64_t ms, const VideoFrameRate &fps, bool force_ndf = false);

	// ミリ秒から HH:MM:SS.mmm 文字列を計算
	static std::string ms_to_timestamp_str(uint64_t ms, bool include_ms = true);

	// SRT字幕用タイムコード（HH:MM:SS,mmm）
	static std::string ms_to_srt_time(uint64_t ms);

	// フレームレート判定
	static bool is_drop_frame_rate(uint32_t num, uint32_t den);
};
