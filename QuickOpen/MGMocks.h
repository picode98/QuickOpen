#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <optional>

#define CIVETWEB_VERSION "1.0.0.0-MOCK"

class CivetHandler;

struct mg_request_info
{
	const char* query_string = nullptr;
	const char* request_uri = nullptr;
	char remote_addr[48];
};

struct mg_connection
{
	bool isOpen = true;
	std::optional<int> responseStatus;
	std::optional<std::string> responseMimeType;
	std::string outputBuffer;
	std::string inputBuffer;

	std::vector<std::pair<std::string, std::optional<std::string>>> sentFiles;
	mg_request_info requestInfo;
};

class CivetServer
{
public:
	// No-ops for now; add logging when needed for testing
	CivetServer(const std::vector<std::string> &options) {}

	void addHandler(const std::string &uri, CivetHandler &handler) {}
	void close() {}
	virtual ~CivetServer() {}
};

class CivetHandler
{
public:
	/**
	 * Destructor
	 */
	virtual ~CivetHandler()
	{
	}

	/**
	 * Callback method for GET request.
	 *
	 * @param server - the calling server
	 * @param conn - the connection information
	 * @returns true if implemented, false otherwise
	 */
	virtual bool handleGet(CivetServer *server, struct mg_connection *conn) { return false; }

	/**
	 * Callback method for POST request.
	 *
	 * @param server - the calling server
	 * @param conn - the connection information
	 * @returns true if implemented, false otherwise
	 */
	virtual bool handlePost(CivetServer *server, struct mg_connection *conn) { return false; }

	/**
	 * Callback method for HEAD request.
	 *
	 * @param server - the calling server
	 * @param conn - the connection information
	 * @returns true if implemented, false otherwise
	 */
	virtual bool handleHead(CivetServer *server, struct mg_connection *conn) { return false; }

	/**
	 * Callback method for PUT request.
	 *
	 * @param server - the calling server
	 * @param conn - the connection information
	 * @returns true if implemented, false otherwise
	 */
	virtual bool handlePut(CivetServer *server, struct mg_connection *conn) { return false; }

	/**
	 * Callback method for DELETE request.
	 *
	 * @param server - the calling server
	 * @param conn - the connection information
	 * @returns true if implemented, false otherwise
	 */
	virtual bool handleDelete(CivetServer *server, struct mg_connection *conn) { return false; }

	/**
	 * Callback method for OPTIONS request.
	 *
	 * @param server - the calling server
	 * @param conn - the connection information
	 * @returns true if implemented, false otherwise
	 */
	virtual bool handleOptions(CivetServer *server, struct mg_connection *conn) { return false; }

	/**
	 * Callback method for PATCH request.
	 *
	 * @param server - the calling server
	 * @param conn - the connection information
	 * @returns true if implemented, false otherwise
	 */
	virtual bool handlePatch(CivetServer *server, struct mg_connection *conn) { return false; }
};

inline int mg_response_header_start(struct mg_connection *conn,
	int status)
{
	conn->responseStatus = status;
	return 0;
}

inline int mg_response_header_add(struct mg_connection *conn,
	const char *header,
	const char *value,
	int value_len)
{
	auto headerName = std::string(header);

	if(headerName == "Content-Type")
	{
		conn->responseMimeType = std::string(value);
	}

	return 0;
}

inline int mg_response_header_send(struct mg_connection *conn) { return 0; }

inline void mg_close_connection(struct mg_connection *conn)
{
	conn->isOpen = false;
}

inline int mg_read(struct mg_connection* conn, void *buf, size_t len)
{
	size_t bytesToRead = len > conn->inputBuffer.size() ? conn->inputBuffer.size() : len;
	std::memcpy(buf, conn->inputBuffer.c_str(), bytesToRead);
	return bytesToRead;
}

inline int mg_write(struct mg_connection* conn, const void *buf, size_t len)
{
	conn->outputBuffer += std::string(reinterpret_cast<const char*>(buf), len);
	return len;
}

inline int mg_send_http_ok(struct mg_connection *conn,
	const char *mime_type,
	long long content_length)
{
	conn->responseStatus = 200;
	conn->responseMimeType = std::string(mime_type);
	return 0;
}

inline int mg_send_http_error(struct mg_connection *conn,
	int status_code, const char *fmt)
{
	mg_write(conn, fmt, strlen(fmt));
	conn->responseStatus = status_code;
	return 0;
}

inline void mg_send_mime_file(struct mg_connection *conn,
	const char *path,
	const char *mime_type)
{
	if(mime_type == nullptr)
	{
		conn->sentFiles.emplace_back(path, std::nullopt);
	}
	else
	{
		conn->sentFiles.emplace_back(path, mime_type);
	}
}

inline const struct mg_request_info *mg_get_request_info(const struct mg_connection* conn) { return &conn->requestInfo; }

inline unsigned mg_exit_library(void) { return 0; }