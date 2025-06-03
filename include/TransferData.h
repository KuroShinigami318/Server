#pragma once
#include "networking/RawTransferData.h"
#include "types.h"

struct CustomTransferData
{
public:
	DeclareScopedEnumImpl(MsgType, size_t,
		HandShake,
		InvalidateSession,
		Logging)

	MsgType msgType = MsgType::_COUNT;
};
DefineScopeEnumOperatorImpl(MsgType, CustomTransferData)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CustomTransferData, msgType)

using TransferData = RawTransferData<CustomTransferData>;