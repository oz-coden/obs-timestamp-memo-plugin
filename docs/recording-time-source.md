# Recording presentation time (H1)

The former clock counted `obs_output_get_total_frames()`. OBS increments that
counter when encoded packets leave interleaving, rather than when the user sees
the rendered video. Encoding delay, B-frame reordering and skipped frames make
packet counts unsuitable as a presentation clock.

For OBS 31.1.1 encoded outputs with audio, the bridge observes video packet PTS
and the corresponding composition timestamp (CTS) through the public packet
callback. It maps `obs_get_video_frame_time()` to recording PTS using that pair:

`recording time = packet PTS + (current video clock - packet CTS)`.

The callback only validates and stores scalar anchors under a mutex. UI updates,
filesystem work and frontend API calls remain outside it. Older CTS anchors are
ignored, so B-frame delivery order cannot rewind the clock. Packet time is
rescaled as OBS's FFmpeg muxer does: PTS / timebase_den (OBS video PTS already
increments by timebase_num). Frame quantization uses integer rational FPS.

Pause freezes the current presentation time. Resume subtracts the paused
interval, rejects late pre-resume packets and then recalibrates from post-resume
PTS. The OBS pause cutoff can differ from frontend event delivery by a frame;
this needs measured verification. Until the first valid packet anchor is seen,
stamping reports that the clock is not calibrated instead of inserting zero.
This also applies when the plugin is loaded during an existing recording.

OBS 31.1.1 invokes packet callbacks from its interleaved encoded path. Raw or
video-only outputs instead use elapsed rendered-video time from the recording
started event, with an explicit warning that the origin is an estimate. This
fallback does not promise exact first-frame alignment.

The FFmpeg muxer splits on a keyframe, but `file_changed` exposes only the new
path. The bridge records the latest observed keyframe PTS as the segment origin;
an output without such a packet uses the current clock estimate. This improves
over cumulative packet counts but cannot prove an arbitrary muxer's boundary.
In particular, unusual audio buffering or custom output plugins require actual
file measurement. R6's boundary-accuracy verification remains open.

No recording output or video clock is acquired during idle plugin loading.
Outputs are retained after recording becomes active; packet and file callbacks
are removed before output release. Shutdown makes later snapshots inert.

## Sources inspected

- [OBS 31.1.1 public packet callback API](https://github.com/obsproject/obs-studio/blob/31.1.1/libobs/obs.h)
- [CTS/PTS timing definitions](https://github.com/obsproject/obs-studio/blob/31.1.1/libobs/obs-encoder.h)
- [Packet timing matching, interleaving and pause implementation](https://github.com/obsproject/obs-studio/blob/31.1.1/libobs/obs-output.c)
- [Video clock getter](https://github.com/obsproject/obs-studio/blob/31.1.1/libobs/obs.c)
- [Actual split and PTS rescaling](https://github.com/obsproject/obs-studio/blob/31.1.1/plugins/obs-ffmpeg/obs-ffmpeg-mux.c)

## Required real OBS verification

1. Record a source showing a continuously incrementing frame/time counter at
   60/1 and 60000/1001 FPS, with audio enabled. Record settings and encoder name.
2. Stamp at visible counter values. Compare those frames in the saved recording
   with marker times. Repeat with B-frames/lookahead on and off; a constant
   encoder-latency-sized offset should disappear.
3. Repeat under encoding overload. Compare positions on the presentation timeline,
   not just the number of surviving encoded frames. Missing video cannot be
   reconstructed by the plugin.
4. Pause for several seconds, stamp while paused, resume and stamp again. Check
   that paused markers share a frozen time and resumed markers exclude the gap.
5. Split near a visible counter value; check markers immediately before/after the
   split against each segment's first video PTS. Include repeated splits and
   audio buffering. Measure remaining frame error; do not assume zero.
6. Stamp immediately after recording starts and when loading during recording;
   before calibration a warning is expected, followed by successful stamping.
7. Test video-only/custom FFmpeg output separately; the estimate warning is
   expected. Check and record its first-frame alignment rather than asserting
   frame accuracy. Quit/restart OBS and verify no callbacks survive shutdown.
