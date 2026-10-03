#pragma once
#include "timecode-helper.hpp"
#include <cstdint>
#include <limits>
#include <optional>

// Maps the current rendered video clock to recording presentation time.
// Packet CTS/PTS pairs calibrate the origin; packet delivery time is irrelevant.
// The caller serializes access (packet callbacks and the UI run on different threads).
class PresentationClock {
public:
	void reset(uint64_t video_ns, bool estimated)
	{
		anchor_video_ = video_ns;
		anchor_pts_ = frozen_ = minimum_cts_ = 0;
		ready_ = estimated;
		paused_ = false;
	}
	bool observe(uint64_t composition_ns, int64_t pts, int32_t denominator)
	{
		const auto time = pts_to_ns(pts, denominator);
		if (!time || !composition_ns || paused_ || composition_ns < minimum_cts_ ||
		    (ready_ && composition_ns < anchor_video_))
			return false;
		anchor_video_ = composition_ns;
		anchor_pts_ = *time;
		ready_ = true;
		return true;
	}
	uint64_t time(uint64_t video_ns) const
	{
		if (!ready_)
			return 0;
		if (paused_)
			return frozen_;
		const auto elapsed = video_ns > anchor_video_ ? video_ns - anchor_video_ : 0;
		return elapsed > std::numeric_limits<uint64_t>::max() - anchor_pts_
			       ? std::numeric_limits<uint64_t>::max()
			       : anchor_pts_ + elapsed;
	}
	void pause(uint64_t video_ns)
	{
		if (!paused_) {
			frozen_ = time(video_ns);
			paused_ = true;
		}
	}
	void resume(uint64_t video_ns)
	{
		if (paused_) {
			anchor_pts_ = frozen_;
			anchor_video_ = minimum_cts_ = video_ns;
			paused_ = false;
		}
	}
	bool ready() const { return ready_; }
	static std::optional<uint64_t> pts_to_ns(int64_t pts, int32_t denominator)
	{
		if (pts < 0 || denominator <= 0)
			return std::nullopt;
		const auto value = static_cast<uint64_t>(pts), den = static_cast<uint64_t>(denominator);
		const auto seconds = value / den;
		const auto fraction = (value % den) * 1000000000ULL / den;
		if (seconds > (std::numeric_limits<uint64_t>::max() - fraction) / 1000000000ULL)
			return std::nullopt;
		// OBS video PTS already advances by timebase_num per frame. The muxer
		// rescales PTS with 1/timebase_den, not timebase_num/timebase_den.
		return seconds * 1000000000ULL + fraction;
	}

private:
	uint64_t anchor_video_ = 0, anchor_pts_ = 0, frozen_ = 0, minimum_cts_ = 0;
	bool ready_ = false, paused_ = false;
};
