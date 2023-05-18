#include "ManagementServer.h"

#include "AppGUIIncludes.h"
#include "WebServerUtils.h"

bool ManagementServer::ConfigReloadHandler::handlePost(CivetServer* server, mg_connection* conn)
{
	appRef.triggerConfigUpdate();
	return true;
}

ManagementServer::ManagementServer(QuickOpenApplication& appRef, unsigned port) : CivetServer({
		"listening_ports", "127.0.0.1:" + std::to_string(port),
		"num_threads", "1"
	}), configReloadHandler(appRef)
{
	std::ofstream mgmtFileOut;
	mgmtFileOut.exceptions(std::ios::failbit);
#ifdef WIN32
	mgmtFileOut.open((InstallationInfo::detectInstallation().configFolder / wxFileName(".", "mgmtServer.json")).GetFullPath().ToStdWstring());
#else
    mgmtFileOut.open((InstallationInfo::detectInstallation().configFolder / wxFileName(".", "mgmtServer.json")).GetFullPath().ToStdString());
#endif
	mgmtFileOut << nlohmann::json(MgmtServerFileData{ port, authHandler.addToken("127.0.0.1") });
	mgmtFileOut.close();

	addAuthHandler("/", authHandler);
	addHandler("/api/config/reload", configReloadHandler);
}

void ManagementClient::sendReload()
{
	std::string serverDataString = fileReadAll(InstallationInfo::detectInstallation().configFolder / wxFileName(".", "mgmtServer.json"));
	auto serverData = MgmtServerFileData(nlohmann::json::parse(serverDataString));

	char errorBuf[250];
	mg_connection* conn = mg_connect_client("127.0.0.1", serverData.port, false, errorBuf, sizeof(errorBuf));
	mg_printf(conn, ("POST /api/config/reload?csrfToken=" + std::to_string(serverData.csrfToken) + " HTTP/1.1\r\n").c_str());
	mg_printf(conn, "Connection: close\r\n\r\n");

	mg_get_response(conn, errorBuf, sizeof(errorBuf), 1000);

	if (mg_get_response_info(conn)->status_code != 200)
	{
		std::cerr << "WARNING: API returned unexpected status code." << std::endl;
	}

	mg_close_connection(conn);
}
