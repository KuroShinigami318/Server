#pragma once

#include "SerializationError.h"
#include "SocketError.h"

DeclareScopedEnumWithOperatorDefined(TransferErrorCode, DUMMY_NAMESPACE, uint8_t
	, Timeout
	, SendFailed
	, SerializationFailed);
using SendRawTransferDataError = utils::Error<TransferErrorCode, SerializationError, SocketError, utils::MessageHandleError>;