#pragma once
#include <cstdint>
// The single owner of recording phase and segment origin. No UI or API queries.
class RecordingTimeline {
public:
	enum class Phase { Idle, Recording, Paused };
	bool start()
	{
		if (active())
			return false;
		phase_ = Phase::Recording;
		origin_ = 0;
		return true;
	}
	bool pause()
	{
		if (phase_ != Phase::Recording)
			return false;
		phase_ = Phase::Paused;
		return true;
	}
	bool resume()
	{
		if (phase_ != Phase::Paused)
			return false;
		phase_ = Phase::Recording;
		return true;
	}
	bool split(uint64_t total)
	{
		if (!active() || total < origin_)
			return false;
		origin_ = total;
		return true;
	}
	bool stop()
	{
		if (!active())
			return false;
		phase_ = Phase::Idle;
		return true;
	}
	bool active() const { return phase_ != Phase::Idle; }
	bool paused() const { return phase_ == Phase::Paused; }
	uint64_t relative_frames(uint64_t total) const { return total >= origin_ ? total - origin_ : 0; }

private:
	Phase phase_ = Phase::Idle;
	uint64_t origin_ = 0;
};
