#include <fstream>
#include <boost/filesystem.hpp>
#include "Utils/Logger.h"
#include "ConstValues.h"
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
        bExit = true;
        mCondvar.notify_all();
        mThread.join();

        ReleaseContext();
        sws_freeContext(sws_ctx);
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
            LOG(LOG_LEVEL_ERROR, "Error sending a packet for decoding");
            exit(1);
        }
        while (ret >= 0) {
            ret = avcodec_receive_frame(context, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                return;
            else if (ret < 0) {
                LOG(LOG_LEVEL_ERROR, "Error during decoding");
                exit(1);
            }
            sws_ctx = sws_getContext(context->width, context->height, context->pix_fmt, context->width, context->height,
                AV_PIX_FMT_BGR24, 0, nullptr, nullptr, nullptr);
            std::shared_ptr<MyFrame> framePtr = std::make_shared<MyFrame>(AV_PIX_FMT_BGR24, context->width,
                context->height, frame->pts, frame->pkt_size);
            sws_scale(sws_ctx, frame->data, frame->linesize, 0, context->height, framePtr.get()->Get()->data,
                framePtr.get()->Get()->linesize);
            frames.push_back(framePtr);
        }
    }
    void DecodeWork::SaveFrame2Jpg(std::shared_ptr<MyFrame> pFrame)
    {
        cv::Mat img(pFrame->Get()->height, pFrame->Get()->width, CV_8UC3, pFrame->Get()->data[0],
            pFrame->Get()->linesize[0]);
        int64_t timestamp = pFrame->Get()->pts;
        int size = pFrame->Get()->pkt_size;
        std::stringstream ss;
#ifdef _WIN64
        ss << saveDir.c_str() << "\\" << std::to_string(timestamp)
            << "_" << std::setw(const_values::EFFECTIVE_NUMBER) << std::setfill('0') << size << ".jpg";
#else
        ss << saveDir.c_str() << "/" << std::to_string(timestamp)
            << "_" << std::setw(const_values::EFFECTIVE_NUMBER) << std::setfill('0') << size << ".jpg";
#endif
        cv::imwrite(ss.str(), img);
    }

    void DecodeWork::AddTask2ThreadPool(int lIdx, int rIdx)
    {
        for (int i = lIdx; i < rIdx; i++) {
            pool.enqueue(&DecodeWork::SaveFrame2Jpg, this, frames[i]);
        }
    }
    void DecodeWork::DoDecode()
    {
        std::unique_lock<std::mutex> lck(mMutex);

        int rIdx = 0;
        while (true) {
            mCondvar.wait(lck);
            lck.unlock();
            while (true) {
                lck.lock();
                if (qFrames.empty()) {
                    rIdx = frames.size();
                    AddTask2ThreadPool(0, rIdx);
                    frames.clear();
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
                rIdx = frames.size();
                AddTask2ThreadPool(0, rIdx);
                break;
            }
        }
    }
    bool DecodeWork::AddFrame(FrameInfo& frame)
    {
        std::unique_lock<std::mutex> lck(mMutexFrame);
        if (frame.vData.empty() || frame.vData.size() < (const_values::DATA_INDEX_CHECK + 1)) {
            return false;
        }
        if (frame.vData.at(const_values::DATA_INDEX_CHECK) == 'g'&&flag == true) {
            flag = !flag;
        } else if (frame.vData.at(const_values::DATA_INDEX_CHECK) == 'g'&&flag == false) {
            mCondvar.notify_all();
            nCondvar.wait(lck);
        }
        qFrames.push(frame);
        return true;
    }
    bool DecodeWork::InitContext()
    {
#ifdef LIBAVCODEC_VERSION_MAJOR
#if (LIBAVCODEC_VERSION_MAJOR < 58)
        avcodec_register_all();
#endif
#endif
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) {
            LOG(LOG_LEVEL_ERROR, "cannot find decoder");
            return false;
        }

        context = avcodec_alloc_context3(codec);
        if (!context) {
            LOG(LOG_LEVEL_ERROR, "cannot allocate context");
            return false;
        }

        if (codec->capabilities & AV_CODEC_CAP_TRUNCATED) {
            context->flags |= AV_CODEC_FLAG_TRUNCATED;
        }

        int err = avcodec_open2(context, codec, nullptr);
        if (err < 0) {
            LOG(LOG_LEVEL_ERROR, "cannot open context");
            return false;
        }

        frame = av_frame_alloc();
        if (!frame) {
            LOG(LOG_LEVEL_ERROR, "cannot allocate frame");
            return false;
        }
        pkt = av_packet_alloc();
        if (!pkt) {
            LOG(LOG_LEVEL_ERROR, "cannot allocate packet");
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
    int  DecodeWork::getH264Size(std::string fileName)
    {
        std::ifstream file(fileName, std::ios::in | std::ios::binary);
        if (!file.is_open())
            LOG(LOG_LEVEL_ERROR, "file open failed!");
        int beg = file.tellg();
        file.seekg(0, std::ios::end);
        int end = file.tellg();
        int len = end - beg;
        file.seekg(0, std::ios::beg);
        return len;
    }
    FrameInfo DecodeWork::readObjectFile(std::string fileName)
    {
        int len = getH264Size(fileName);
        std::vector<uint8_t> vData(len);
        FILE* fp = fopen(fileName.c_str(), "rb");
        fread(&vData[0], 1, len, fp);
        fclose(fp);

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
