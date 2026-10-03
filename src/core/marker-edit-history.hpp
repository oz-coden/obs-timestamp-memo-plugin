#pragma once
#include "memo-marker.hpp"
#include <optional>
#include <vector>
#include <cstddef>

// Commands retain only affected markers, never whole document snapshots.
class MarkerEditHistory {
public:
	struct Change {
		size_t row;
		std::optional<MemoMarker> before, after;
	};
	using Command = std::vector<Change>;
	void clear()
	{
		commands_.clear();
		cursor_ = 0;
		bytes_ = 0;
	}
	bool record(Command command)
	{
		while (commands_.size() > cursor_) {
			bytes_ -= size(commands_.back());
			commands_.pop_back();
		}
		const auto cost = size(command);
		if (cost > max_bytes) {
			clear();
			return false;
		}
		if (command.empty())
			return true;
		while (!commands_.empty() && (commands_.size() >= max_commands || bytes_ + cost > max_bytes)) {
			bytes_ -= size(commands_.front());
			commands_.erase(commands_.begin());
			--cursor_;
		}
		bytes_ += cost;
		commands_.push_back(std::move(command));
		cursor_ = commands_.size();
		return true;
	}
	const Command *undo_command() const { return cursor_ ? &commands_[cursor_ - 1] : nullptr; }
	const Command *redo_command() const { return cursor_ < commands_.size() ? &commands_[cursor_] : nullptr; }
	void did_undo()
	{
		if (cursor_)
			--cursor_;
	}
	void did_redo()
	{
		if (cursor_ < commands_.size())
			++cursor_;
	}
	static constexpr size_t max_commands = 128;
	static constexpr size_t max_bytes = 8 * 1024 * 1024;

private:
	static size_t size(const Command &command)
	{
		size_t result = sizeof(Command) + command.size() * sizeof(Change);
		for (const auto &change : command)
			for (const auto *marker : {&change.before, &change.after})
				if (*marker) {
					const auto &m = **marker;
					result += m.label.size() + m.color.size() + m.comment.size() +
						  m.created_at_utc.size() + m.timecode_ndf.size() +
						  m.timecode_df.size();
				}
		return result;
	}
	std::vector<Command> commands_;
	size_t cursor_ = 0, bytes_ = 0;
};
