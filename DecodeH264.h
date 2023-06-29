#ifndef DECODE_H264_H
#define DECODE_H264_H
#include <map>
#include "Algorithms/Base/BaseModule.h"

namespace Algorithms {
class DecodeH264 : public BaseModule {
public:
    DecodeH264() = default;
    bool Run(const std::vector<std::string> &vParam) override;

private:
    bool InitParam(const std::vector<std::string> &vParam);
    bool CheckParam();
    bool ConvH2642Jpg();

    std::map<std::string, std::string> mParam;
    std::vector<std::string> mFileNames;
};
}
#endif
