#ifndef DECODE_WORK_H
#define DECODE_WORK_H
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iomanip>
#include <queue>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
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
        void SaveFrame2Jpg(std::shared_ptr<AVFrame> pFrame, int index);
        void AddTask2ThreadPool(int lIdx, int rIdx);

        SwsContext            *sws_ctx = nullptr;
        AVCodecContext        *context = nullptr;
        AVFrame               *frame = nullptr;
        AVCodec               *codec = nullptr;
        AVPacket              *pkt = nullptr;

        ThreadPool pool;
        std::queue<FrameInfo> qFrames;
        std::vector<std::shared_ptr<AVFrame>> frames;
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
