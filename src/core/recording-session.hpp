#pragma once

#include "memo-marker.hpp"
#include "timecode-helper.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <QJsonObject>

class RecordingSession {
public:
	RecordingSession();
	~RecordingSession();

	// 新規録画セッション開始
	bool start_session(const std::string &video_path, const VideoFrameRate &fps, uint32_t width, uint32_t height);

	// 録画セッション停止＆確定
	bool stop_session(std::string *out_json_path = nullptr);

	// クラッシュキャッシュの復元チェック
	static bool recover_from_cache(const std::string &cache_path, RecordingSession &out_session);

	// JSONファイルからセッション読み込み（過去録画の編集用）
	bool load_from_json(const std::string &json_path);

	// 現在のセッションをJSONファイルに保存
	bool save_to_json(const std::string &target_json_path = "");

	// マーカー追加（スレッドセーフ）
	MemoMarker add_marker(uint64_t ms, int type_index, const std::string &label,
			      const std::string &color, const std::string &comment, bool is_paused);

	// マーカーの更新
	bool update_marker(uint32_t marker_id, const std::string &label, const std::string &color, const std::string &comment);

	// マーカーの削除
	bool delete_marker(uint32_t marker_id);

	// 全マーカークリア
	void clear_markers();

	// セッター/ゲッター
	bool is_active() const { return active_; }
	std::string video_path() const { return video_path_; }
	std::string session_id() const { return session_id_; }
	std::string started_at() const { return started_at_; }
	VideoFrameRate frame_rate() const { return fps_; }
	uint32_t width() const { return width_; }
	uint32_t height() const { return height_; }
	std::vector<MemoMarker> get_markers() const;

	QJsonObject to_json() const;

private:
	std::string get_cache_file_path() const;
	void flush_marker_to_cache(const MemoMarker &marker);
	void cleanup_cache_file();

	mutable std::mutex mutex_;
	bool active_ = false;
	std::string video_path_;
	std::string session_id_;
	std::string started_at_;
	VideoFrameRate fps_{60, 1};
	uint32_t width_ = 1920;
	uint32_t height_ = 1080;
	std::vector<MemoMarker> markers_;
	uint32_t next_marker_id_ = 1;

	std::ofstream cache_stream_;
	std::string cache_file_path_;
};
