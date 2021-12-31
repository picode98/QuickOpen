#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "CivetWebIncludes.h"

struct FormErrorList
{
	struct FormError
	{
		std::string fieldName,
			errorString;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(FormError, fieldName, errorString)
	};

	std::vector<FormError> errors;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(FormErrorList, errors)
};

std::map<std::string, std::string> parseFormEncodedBody(mg_connection* conn);
std::map<std::string, std::string> parseQueryString(mg_connection* conn);
void sendJSONResponse(mg_connection* conn, int status, const nlohmann::json& json);
bool requireParameter(mg_connection* conn, const std::map<std::string, std::string>& paramMap, const std::string& parameter);