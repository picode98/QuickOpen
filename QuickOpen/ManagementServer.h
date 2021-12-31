#pragma once
#include "AppConfig.h"
#include "CivetWebIncludes.h"

class IQuickOpenApplication;

struct MgmtServerFileData
{
	unsigned port;
	uint64_t authToken;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(MgmtServerFileData, port, authToken)
};

class ManagementServer : public CivetServer
{
public:
	class ConfigReloadHandler : public CivetHandler
	{
		IQuickOpenApplication& appRef;

	public:
		ConfigReloadHandler(IQuickOpenApplication& appRef): appRef(appRef)
		{}

		bool handlePost(CivetServer* server, mg_connection* conn) override;
	};

	class MgmtAuthHandler : public CivetAuthHandler
	{
		uint64_t authToken;
		bool authorize(CivetServer* server, mg_connection* conn) override;
	public:
		MgmtAuthHandler(): authToken(generateCryptoRandomInteger<uint64_t>())
		{}
		constexpr uint64_t getAuthToken() const { return this->authToken; }
	};

private:
	ConfigReloadHandler configReloadHandler;
	MgmtAuthHandler authHandler;

public:
	ManagementServer(IQuickOpenApplication& appRef, unsigned port);
};

namespace ManagementClient
{
	void sendReload();
}
