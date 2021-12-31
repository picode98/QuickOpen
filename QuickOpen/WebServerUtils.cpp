#include "WebServerUtils.h"

#include <sstream>

std::string URLDecode(const std::string& encodedStr, bool decodePlus)
{
	std::ostringstream outputStream;

	for (size_t i = 0; i < encodedStr.size();)
	{
		switch (encodedStr[i])
		{
		case '%':
			outputStream << char(std::stoi(encodedStr.substr(i + 1, 2), nullptr, 16));
			i += 3;
			break;
		case '+':
			if (decodePlus)
			{
				outputStream << ' ';
				++i;
				break;
			}
		default:
			outputStream << encodedStr[i];
			++i;
			break;
		}
	}

	return outputStream.str();
}

std::map<std::string, std::string> parseFormEncodedBody(mg_connection* conn)
{
	static const long long CHUNK_SIZE = 1LL << 16;
	char bodyBuffer[CHUNK_SIZE];
	std::string currentKey, currentValue;
	bool readingValue = false;
	// const mg_request_info* rqInfo = mg_get_request_info(conn);

	std::map<std::string, std::string> resultMap;

	// long long maxContentLength = rqInfo->content_length == -1 ? std::numeric_limits<long long>::max() : rqInfo->content_length;
	int bytesRead;
	while ((bytesRead = mg_read(conn, bodyBuffer, CHUNK_SIZE)) > 0)
	{
		for (int i = 0; i < bytesRead; ++i)
		{
			switch (bodyBuffer[i])
			{
			case '&':
				readingValue = false;
				resultMap.insert({ URLDecode(currentKey, true), URLDecode(currentValue, true) });
				currentKey = currentValue = "";
				break;
			case '=':
				readingValue = true;
				break;
			default:
				if (readingValue)
				{
					currentValue += bodyBuffer[i];
				}
				else
				{
					currentKey += bodyBuffer[i];
				}

				break;
			}
		}
	}

	resultMap.insert({ URLDecode(currentKey, true), URLDecode(currentValue, true) });

	return resultMap;
}

std::map<std::string, std::string> parseQueryString(mg_connection* conn)
{
	const char* queryStringPtr = mg_get_request_info(conn)->query_string;
	if (queryStringPtr == nullptr) return {};

	std::string queryString = queryStringPtr;

	std::string currentKey, currentValue;
	bool readingValue = false;

	std::map<std::string, std::string> resultMap;

	for (size_t i = 0; i < queryString.size(); ++i)
	{
		switch (queryString[i])
		{
		case '&':
			readingValue = false;
			resultMap.insert({ URLDecode(currentKey, false), URLDecode(currentValue, false) });
			currentKey = currentValue = "";
			break;
		case '=':
			readingValue = true;
			break;
		default:
			if (readingValue)
			{
				currentValue += queryString[i];
			}
			else
			{
				currentKey += queryString[i];
			}

			break;
		}
	}

	resultMap.insert({ URLDecode(currentKey, false), URLDecode(currentValue, false) });

	return resultMap;
}

void sendJSONResponse(mg_connection* conn, int status, const nlohmann::json& json)
{
	mg_response_header_start(conn, status);
	mg_response_header_add(conn, "Content-Type", "application/json", -1);
	mg_response_header_send(conn);
	std::string jsonString = json.dump();
	mg_write(conn, jsonString.c_str(), jsonString.size());
	mg_close_connection(conn);
}

bool requireParameter(mg_connection* conn, const std::map<std::string, std::string>& paramMap, const std::string& parameter)
{
	if(paramMap.count(parameter) == 0)
	{
		auto jsonErrorInfo = nlohmann::json(FormErrorList{
			{
				{parameter, "Argument for required parameter \"" + parameter + "\" is missing."}
			}
			});
		sendJSONResponse(conn, 400, jsonErrorInfo);
		return false;
	}
	else
	{
		return true;
	}
}