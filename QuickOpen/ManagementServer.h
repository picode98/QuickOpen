#pragma once
#include "AppConfig.h"
#include "WebServerUtils.h"
#include "CivetWebIncludes.h"

class QuickOpenApplication;

struct MgmtServerFileData
{
	unsigned port;
	uint64_t csrfToken;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(MgmtServerFileData, port, csrfToken)
};

class ManagementServer : public CivetServer
{
public:
	class ConfigReloadHandler : public CivetHandler
	{
		QuickOpenApplication& appRef;

	public:
		ConfigReloadHandler(QuickOpenApplication& appRef): appRef(appRef)
		{}

		bool handlePost(CivetServer* server, mg_connection* conn) override;
	};

private:
	ConfigReloadHandler configReloadHandler;
	CSRFAuthHandler authHandler;

public:
	ManagementServer(QuickOpenApplication& appRef, unsigned port);
};

namespace ManagementClient
{
	void sendReload();
}
