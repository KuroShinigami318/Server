#pragma once
#include "nlohmann/json.hpp"
#include "types.h"
#include <vector>

struct RawTransferData
{
public:
	DeclareScopedEnum(MsgType, size_t
		, HandShake
		, InvalidateSession);
public:
	MsgType msgType = MsgType::_COUNT;
	size_t totalBytes = 0;
	std::vector<char> msg;
};
DefineScopeEnumOperatorImpl(MsgType, RawTransferData);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RawTransferData, msgType, totalBytes, msg);

#define SERIALIZE_FUNC nlohmann::json::to_ubjson
#define DESERIALIZE_FUNC nlohmann::json::from_ubjson