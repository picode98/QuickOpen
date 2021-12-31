#include "ManagementServer.h"

#include "AppGUI.h"
#include "WebServerUtils.h"

bool ManagementServer::ConfigReloadHandler::handlePost(CivetServer* server, mg_connection* conn)
{
	appRef.triggerConfigUpdate();
	return true;
}

bool ManagementServer::MgmtAuthHandler::authorize(CivetServer* server, mg_connection* conn)
{
	auto queryStringMap = parseQueryString(conn);
	if(requireParameter(conn, queryStringMap, "authToken"))
	{
		char* endPtr;
		if(strtoull(queryStringMap["authToken"].c_str(), &endPtr, 10) == this->authToken)
		{
			return true;
		}
		else
		{
			auto jsonErrorInfo = nlohmann::json(FormErrorList{
				{
					{"authToken", "The authentication token supplied is not valid."}
				}
			});
			sendJSONResponse(conn, 403, jsonErrorInfo);
			return false;
		}
	}
	else
	{
		return false; // requireParameter sent Bad Request
	}
}

ManagementServer::ManagementServer(IQuickOpenApplication& appRef, unsigned port) : CivetServer({
		"listening_ports", "127.0.0.1:" + std::to_string(port) + ",[::1]:" + std::to_string(port),
		"num_threads", "1"
	}), configReloadHandler(appRef)
{
	std::ofstream mgmtFileOut;
	mgmtFileOut.exceptions(std::ios::failbit);
	mgmtFileOut.open((InstallationInfo::detectInstallation().configFolder / wxFileName(".", "mgmtServer.json")).GetFullPath().ToStdWstring());
	mgmtFileOut << nlohmann::json(MgmtServerFileData{ port, authHandler.getAuthToken() });
	mgmtFileOut.close();

	addAuthHandler("/", &authHandler);
	addHandler("/api/config/reload", &configReloadHandler);
}

void ManagementClient::sendReload()
{
	std::ifstream mgmtFileIn;
	std::string mgmtFileStr;
	mgmtFileIn.exceptions(std::ios::failbit);
	mgmtFileIn.open((InstallationInfo::detectInstallation().configFolder / wxFileName(".", "mgmtServer.json")).GetFullPath().ToStdWstring());
	std::stringstream serverDataString;
	serverDataString << mgmtFileIn.rdbuf();
	auto serverData = MgmtServerFileData(nlohmann::json::parse(serverDataString.str()));
	mgmtFileIn.close();

	char errorBuf[250];
	mg_connection* conn = mg_connect_client("127.0.0.1", serverData.port, false, errorBuf, sizeof(errorBuf));
	mg_printf(conn, ("POST /api/config/reload?authToken=" + std::to_string(serverData.authToken) + " HTTP/1.1\r\n").c_str());
	mg_printf(conn, "Connection: close\r\n\r\n");

	mg_get_response(conn, errorBuf, sizeof(errorBuf), 1000);

	if (mg_get_response_info(conn)->status_code != 200)
	{
		std::cerr << "WARNING: API returned unexpected status code." << std::endl;
	}

	mg_close_connection(conn);
}
