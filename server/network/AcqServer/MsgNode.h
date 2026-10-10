#pragma once
#include <boost/json.hpp>
#include <cstdint>
#include <memory>

namespace ndtapp::acq {
class CSession;
struct MsgNode {
    uint16_t id;
    boost::json::value body;
    std::shared_ptr<CSession> session;
};
}
