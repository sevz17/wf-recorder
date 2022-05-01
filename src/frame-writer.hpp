// Adapted from https://stackoverflow.com/questions/34511312/how-to-encode-a-video-from-several-images-generated-in-a-c-program-without-wri
// (Later) adapted from https://github.com/apc-llc/moviemaker-cpp

#ifndef FRAME_WRITER
#define FRAME_WRITER

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include "config.h"

#define AUDIO_RATE 44100

extern "C"
{
    #include <libswresample/swresample.h>
    #include <libavcodec/avcodec.h>
#ifdef HAVE_LIBAVDEVICE
    #include <libavdevice/avdevice.h>
#endif
    #include <libavutil/mathematics.h>
    #include <libavformat/avformat.h>
    #include <libavfilter/avfilter.h>
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
    #include <libavutil/pixdesc.h>
    #include <libavutil/hwcontext.h>
    #include <libavutil/opt.h>
}

#include "config.h"

enum InputFormat
{
     INPUT_FORMAT_BGR0,
     INPUT_FORMAT_RGB0,
     INPUT_FORMAT_BGR8
};

struct FrameWriterParams
{
    std::string file;
    int width;
    int height;
    int stride;

    InputFormat format;

    std::string video_filter = "null"; // dummy filter

    std::string codec;
    std::string muxer;
    std::string pix_fmt;
    std::string hw_device; // used only if codec contains vaapi
    std::map<std::string, std::string> codec_options;

    int64_t audio_sync_offset;

    bool enable_audio;
    bool enable_ffmpeg_debug_output;

    bool force_yuv;
    int bframes;

    std::atomic<bool>& write_aborted_flag;
    FrameWriterParams(std::atomic<bool>& flag): write_aborted_flag(flag) {}
};

class FrameWriter
{
    FrameWriterParams params;
    void load_codec_options(AVDictionary **dict);

    const AVOutputFormat* outputFmt;
    AVStream* videoStream;
    AVCodecContext* videoCodecCtx;
    AVFormatContext* fmtCtx;

    AVFilterContext* videoFilterSourceCtx = NULL;
    AVFilterContext* videoFilterSinkCtx = NULL;
    AVFilterGraph* videoFilterGraph = NULL;

    AVBufferRef *hw_device_context = NULL;
    AVBufferRef *hw_frame_context = NULL;

    AVPixelFormat lookup_pixel_format(std::string pix_fmt);
    AVPixelFormat choose_sw_format(const AVCodec *codec);
    AVPixelFormat get_input_format();
    void init_hw_accel();
    void init_codecs();
    void init_video_filters(const AVCodec *codec);
    void init_video_stream();

    void encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt);

#ifdef HAVE_PULSE
    SwrContext *swrCtx;
    AVStream *audioStream;
    AVCodecContext *audioCodecCtx;
    void init_swr();
    void init_audio_stream();
    void send_audio_pkt(AVFrame *frame);
#endif
    void finish_frame(AVCodecContext *enc_ctx, AVPacket& pkt);

    /**
     * Upload data to a frame.
     */
     AVFrame *prepare_frame_data(const uint8_t* pixels, bool y_invert);

  public:
    FrameWriter(const FrameWriterParams& params);
    bool add_frame(const uint8_t* pixels, int64_t usec, bool y_invert);

#ifdef HAVE_PULSE
    /* Buffer must have size get_audio_buffer_size() */
    void add_audio(const void* buffer);
    size_t get_audio_buffer_size();

#endif

    ~FrameWriter();
};

#include <memory>
#include <mutex>
#include <atomic>

extern std::mutex frame_writer_mutex, frame_writer_pending_mutex;
extern std::unique_ptr<FrameWriter> frame_writer;
extern std::atomic<bool> exit_main_loop;

#endif // FRAME_WRITER
