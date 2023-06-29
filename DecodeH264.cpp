/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 */
#include "Algorithms/DecodeH264/DecodeH264.h"
#include "Algorithms/Base/ModuleCreateFuncMgr.h"
#include "Algorithms/DecodeH264/DecodeWork.h"
#include "Utils/Logger.h"
#include "Utils/FileHelper.h"
#include "ConstValues.h"

namespace Algorithms {
    static const bool B_REGIST_RET1 = REG_FUN(Algorithms::ModuleType::H264_COV_MODULE, DecodeH264);
    bool DecodeH264::Run(const std::vector<std::string> &vParam)
    {
        if (!InitParam(vParam)) {
            LOG(LOG_LEVEL_ERROR, "InitParam fail");
            return false;
        }
        return ConvH2642Jpg();
    }
    bool DecodeH264::InitParam(const std::vector<std::string> &vParam)
    {
        if (!B_REGIST_RET1) {
            LOG(LOG_LEVEL_ERROR, "Register DecodeH264 fail!");
            return false;
        }

        for (size_t iPos = 0; iPos < vParam.size() - 1; iPos++) {
            if (vParam[iPos] == "-h264_dir") {
                mParam["h264_dir"] = vParam[iPos + 1];
            } else if (vParam[iPos] == "-out_jpg_path") {
                mParam["out_jpg_path"] = vParam[iPos + 1];
            }
        }

        if (!CheckParam()) {
            LOG(LOG_LEVEL_ERROR, "h264_dir %s", mParam["h264_dir"].c_str());
            LOG(LOG_LEVEL_ERROR, "out_jpg_path %s", mParam["out_jpg_path"].c_str());
            return false;
        }
        return true;
    }
    bool DecodeH264::CheckParam()
    {
        if (mParam.size() != const_values::H264_CONV_ARGS_COUNT) {
            LOG(LOG_LEVEL_ERROR, "param num %d", mParam.size());
            return false;
        }

        if (!Utils::IsDirExist(mParam["h264_dir"])) {
            LOG(LOG_LEVEL_ERROR, "h264_dir %s not exist", mParam["h264_dir"].c_str());
            return false;
        }

        if (Utils::Suffix(mParam["out_jpg_path"]) != ".jpg") {
            LOG(LOG_LEVEL_ERROR, "out_jpg_path %s not jpg", mParam["out_jpg_path"].c_str());
            return false;
        }

        mFileNames.clear();
        Utils::ListDir(mParam["h264_dir"], mFileNames, ".h264");
        if (mFileNames.size() == 0) {
            LOG(LOG_LEVEL_ERROR, "mFileNames nums is %d", mFileNames.size());
            return false;
        }
        return true;
    }
    bool DecodeH264::ConvH2642Jpg()
    {
        DecodeWork dw(mParam["out_jpg_path"]);
        std::vector<FrameInfo> nodes;
        for (std::vector<std::string>::iterator itr = mFileNames.begin(); itr != mFileNames.end(); ++itr) {
            FrameInfo fi = dw.readObjectFile(*itr);
            nodes.push_back(fi);
        }
        for (std::vector<FrameInfo>::iterator itr = nodes.begin(); itr != nodes.end(); ++itr) {
            if (!dw.AddFrame(*itr)) {
                LOG(LOG_LEVEL_ERROR, "AddFrame error");
                return false;
            }
        }
        return true;
    }
}
