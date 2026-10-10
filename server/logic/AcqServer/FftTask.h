#pragma once
#include "NdtCommon.h"
#include "server/network/AcqServer/ProcGrpcClient.h"
#include <memory>

namespace ndtapp::acq {
class CSession;
class FftTask final {
public:
    static void Run(std::shared_ptr<CSession> session, BoundedQueue<ndt::Waveform> &queue,
                    ProcGrpcClient &client);
};
}
