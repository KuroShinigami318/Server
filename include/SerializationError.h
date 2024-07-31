#pragma once

DeclareScopedEnumWithOperatorDefined(SerializationErrorCode, DUMMY_NAMESPACE, uint8_t
	, SerializeFailed
	, DeserializeFailed);
using SerializationError = utils::Error<SerializationErrorCode>;