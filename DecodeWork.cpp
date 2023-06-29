#include <fstream>
#include <boost/filesystem.hpp>
#include "Utils/Logger.h"
#include "Algorithms/DecodeH264/DecodeWork.h"
namespace Algorithms {
    DecodeWork::DecodeWork(std::string out_jpg_path):pool(std::thread::hardware_concurrency())
    {
        saveDir = out_jpg_path;
        if (InitContext()) {
            mThread = std::thread(&DecodeWork::DoDecode, this);
        }
    }
    DecodeWork::~DecodeWork()
    {
        std::cout << "beg ~DecodeH264" << std::endl;
        bExit = true;
        mCondvar.notify_all();
        mThread.join();

        ReleaseContext();
        sws_freeContext(sws_ctx);
        std::cout << "end ~DecodeH264" << std::endl;
    }
    void DecodeWork::DecodeFrame(FrameInfo&frameInfo)
    {
        pkt->pts = frameInfo.timestamp;
        pkt->data = (uint8_t*)(&frameInfo.vData[0]);
        pkt->size = frameInfo.size;
        DecodePkt();
    }
    void DecodeWork::DecodePkt()
    {
        int ret = avcodec_send_packet(context, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error sending a packet for decoding\n");
            exit(1);
        }
        while (ret >= 0) {
            ret = avcodec_receive_frame(context, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                return;
            else if (ret < 0) {
                fprintf(stderr, "Error during decoding\n");
                exit(1);
            }
            sws_ctx = sws_getContext(context->width, context->height, context->pix_fmt,
                context->width, context->height, AV_PIX_FMT_BGR24, 0, nullptr, nullptr, nullptr);
            AVFrame* new_frame = av_frame_alloc();
            new_frame->format = AV_PIX_FMT_BGR24;
            new_frame->width = context->width;
            new_frame->height = context->height;
            new_frame->pts = frame->pts;
            new_frame->pkt_size = frame->pkt_size;
            av_frame_get_buffer(new_frame, 0);
            sws_scale(sws_ctx, frame->data, frame->linesize, 0, context->height, new_frame->data, new_frame->linesize);
            std::shared_ptr<AVFrame> framePtr(new_frame, &av_frame_unref);
            frames.push_back(framePtr);
        }
    }
    void DecodeWork::SaveFrame2Jpg(std::shared_ptr<AVFrame> pFrame, int index)
    {
        cv::Mat img(pFrame->height, pFrame->width, CV_8UC3, pFrame->data[0], pFrame->linesize[0]);
        int64_t timestamp = pFrame->pts;
        int size = pFrame->pkt_size;
        std::stringstream ss;
#ifdef _WIN64
        ss << saveDir.c_str() << "\\" << std::to_string(timestamp) \
            << "_" << std::setw(EFFECTIVE_NUMBER) << std::setfill('0') << size << ".jpg";
#else
        ss << saveDir.c_str() << "/" << std::to_string(timestamp) \
            << "_" << std::setw(EFFECTIVE_NUMBER) << std::setfill('0') << size << ".jpg";
#endif
        cv::imwrite(ss.str(), img);
    }

    void DecodeWork::AddTask2ThreadPool(int lIdx, int rIdx)
    {
        for (int i = lIdx; i < rIdx; i++) {
            pool.enqueue(&DecodeWork::SaveFrame2Jpg, this, frames[i], i);
        }
    }
    void  DecodeWork::DoDecode()
    {
        std::unique_lock<std::mutex> lck(mMutex);

        int lIdx = 0;
        int rIdx = 0;
        while (true) {
            mCondvar.wait(lck);
            lck.unlock();
            while (true) {
                lck.lock();
                if (qFrames.empty()) {
                    lIdx = rIdx;
                    rIdx = context->frame_number;
                    AddTask2ThreadPool(lIdx, rIdx);
                    nCondvar.notify_all();
                    break;
                }
                auto &frameInfo = qFrames.front();
                DecodeFrame(qFrames.front());
                qFrames.pop();
                lck.unlock();
            }
            if (bExit) {
                pkt = nullptr;
                DecodePkt();
                lIdx = rIdx;
                rIdx = context->frame_number;
                AddTask2ThreadPool(lIdx, rIdx);
                break;
            }
        }
    }
    bool DecodeWork::AddFrame(FrameInfo& frame)
    {
        std::unique_lock<std::mutex> lck(mMutexFrame);
        if (frame.vData.empty() || frame.vData.size() < (DATA_INDEX_CHECK + 1)) {
            return false;
        }
        if (frame.vData.at(DATA_INDEX_CHECK) == 'g'&&flag == true) {
            flag = !flag;
        } else if (frame.vData.at(DATA_INDEX_CHECK) == 'g'&&flag == false) {
            mCondvar.notify_all();
            nCondvar.wait(lck);
        }
        qFrames.push(frame);
        return true;
    }
    bool DecodeWork::InitContext()
    {
#if (LIBAVCODEC_VERSION_MAJOR < 58)
        avcodec_register_all();
#endif
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) {
            std::cout << "cannot find decoder" << std::endl;
            return false;
        }

        context = avcodec_alloc_context3(codec);
        if (!context) {
            std::cout << "cannot allocate context" << std::endl;
            return false;
        }

        if (codec->capabilities & AV_CODEC_CAP_TRUNCATED) {
            context->flags |= AV_CODEC_FLAG_TRUNCATED;
        }

        int err = avcodec_open2(context, codec, nullptr);
        if (err < 0) {
            std::cout << "cannot open context" << std::endl;
            return false;
        }

        frame = av_frame_alloc();
        if (!frame) {
            std::cout << "cannot allocate frame" << std::endl;
            return false;
        }
        pkt = av_packet_alloc();
        if (!pkt) {
            std::cout << "cannot allocate packet" << std::endl;
            return false;
        }
        return true;
    }
    void DecodeWork::ReleaseContext()
    {
        if (context == nullptr) {
            avcodec_close(context);
            context = nullptr;
        }
        if (context) {
            av_free(context);
            context = nullptr;
        }
        if (frame) {
            av_frame_free(&frame);
            frame = nullptr;
        }
        if (pkt) {
            av_packet_free(&pkt);
            pkt = nullptr;
        }
    }
    FrameInfo DecodeWork::readObjectFile(std::string fileName)
    {
        std::ifstream file(fileName, std::ios::in | std::ios::binary);
        if (!file.is_open())
            LOG(LOG_LEVEL_ERROR, "file open failed!");
        int beg = file.tellg();
        file.seekg(0, std::ios::end);
        int end = file.tellg();
        int len = end - beg;
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> vData(len);
        file.read(reinterpret_cast<char*>(vData.data()), len);

        boost::filesystem::path filePath(fileName);
        std::string tmp = filePath.stem().string();
        int size = len;
        int64_t timestamp = stol(tmp.substr(0, 6));
        std::vector<uint8_t> curData(vData.data(), vData.data() + size);
        FrameInfo TmpInfo;
        TmpInfo.vData.swap(curData);
        TmpInfo.size = size;
        TmpInfo.timestamp = timestamp;
        TmpInfo.streamType = 0;
        TmpInfo.stream_index = 0;
        return TmpInfo;
    }
}
