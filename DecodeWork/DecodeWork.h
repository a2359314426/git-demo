#ifndef DECODE_WORK_H
#define DECODE_WORK_H
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iomanip>
#include <queue>
#include <memory>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

#include <opencv2/opencv.hpp>
#include "Algorithms/Base/BaseModule.h"
#include "Algorithms/DecodeH264/ThreadPool.h"
namespace Algorithms {
    struct FrameInfo {
        std::vector<uint8_t> vData;
        int size;
        int64_t timestamp;
        uint8_t streamType;
        int stream_index;
    };
    class MyFrame {
    public:
        MyFrame(int av_pix_fmt_bgr24, int width, int height, int pts, int pkt_size)
        {
            nFrame = nullptr;
            nFrame = av_frame_alloc();
            nFrame->format = av_pix_fmt_bgr24;
            nFrame->width = width;
            nFrame->height =height;
            nFrame->pts = pts;
            nFrame->pkt_size = pkt_size;
            av_frame_get_buffer(nFrame, 0);
        }
        ~MyFrame()
        {
            av_frame_free(&nFrame);
        }
        AVFrame* Get()
        {
            return nFrame;
        }
    private:
        AVFrame *nFrame;
    };
    class DecodeWork {
    public:
        DecodeWork(std::string out_jpg_path);
        ~DecodeWork();
        FrameInfo readObjectFile(std::string str);
        bool AddFrame(FrameInfo& frameinfo);
        void DoDecode();
    private:
        bool InitContext();
        void ReleaseContext();
        void DecodeFrame(FrameInfo&);
        void DecodePkt();
        void SaveFrame2Jpg(std::shared_ptr<MyFrame> pFrame);
        void AddTask2ThreadPool(int lIdx, int rIdx);
        int  getH264Size(std::string fileName);

        SwsContext            *sws_ctx = nullptr;
        AVCodecContext        *context = nullptr;
        AVFrame               *frame = nullptr;
        AVCodec               *codec = nullptr;
        AVPacket              *pkt = nullptr;

        ThreadPool pool;
        std::queue<FrameInfo> qFrames;
        std::vector<std::shared_ptr<MyFrame>> frames;
        std::thread mThread;
        std::mutex mMutex;
        std::mutex mMutexFrame;
        std::condition_variable mCondvar;
        std::condition_variable nCondvar;
        bool bExit = false;
        std::string saveDir;
        bool flag = true;
    };
}
#endif
