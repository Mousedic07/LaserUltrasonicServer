#include "MsgNode.h"
// The node is deliberately a value type: frame parsing completes before
// ownership passes from CSession's IO callback to LogicSystem's bounded queue.
