#include "catch.hpp"

#include "WebServer.h"

TEST_CASE("sendJSONResponse function")
{
	nlohmann::json testJSON = { { "key1", "value1" }, 5 };
	mg_connection testConn;
	sendJSONResponse(&testConn, 200, testJSON);
	REQUIRE(testConn.responseStatus.has_value());
	REQUIRE(testConn.responseStatus.value() == 200);
	REQUIRE(testConn.responseMimeType.has_value());
	REQUIRE(testConn.responseMimeType.value() == "application/json");
	REQUIRE(nlohmann::json::parse(testConn.outputBuffer) == testJSON);
	REQUIRE(!testConn.isOpen);
}

TEST_CASE("StaticHandler class - happy path")
{
	wxFileName expectedStaticBase = InstallationInfo::detectInstallation().dataFolder / wxFileName("static", "");

	CivetServer testServer({});
	StaticHandler handler("/static/");
	mg_connection testConn;

	SECTION("happy path")
	{
		testConn.requestInfo = { "", "/static/page.html", "::1" };
		REQUIRE(handler.handleGet(&testServer, &testConn));
		REQUIRE(testConn.sentFiles.size() == 1);
		REQUIRE(wxFileName(testConn.sentFiles[0].first) == expectedStaticBase / wxFileName(".", "page.html"));
	}
	SECTION("unhappy path - path outside of static root")
	{
		testConn.requestInfo = { "", "/some_dir/page.html", "::1" };
		REQUIRE(handler.handleGet(&testServer, &testConn));
		REQUIRE(testConn.sentFiles.empty());
		REQUIRE(testConn.responseStatus.has_value());
		REQUIRE(testConn.responseStatus.value() == 400);
	}
	SECTION("unhappy path - path with .. symbols")
	{
		testConn.requestInfo = { "", "/static/../page.html", "::1" };
		REQUIRE(handler.handleGet(&testServer, &testConn));
		REQUIRE(testConn.sentFiles.empty());
		REQUIRE(testConn.responseStatus.has_value());
		REQUIRE(testConn.responseStatus.value() == 400);
	}
}

class MockGUIApp : public IQuickOpenApplication
{
public:
	bool confirmPrompts = false, requestBan = false;

	MockGUIApp(bool confirmPrompts, bool requestBan): confirmPrompts(confirmPrompts), requestBan(requestBan)
	{}

	bool promptedForWebpage = false;
	struct
	{
		std::string URL;
		wxString requesterName;
	} webpagePromptInfo;

	TrayStatusWindow* getTrayWindow() const override
	{
		// TODO: Not yet mockable
		return nullptr;
	}

	QuickOpenTaskbarIcon* getTrayIcon() const override
	{
		// TODO: Not yet mockable
		return nullptr;
	}

	void notifyUser(MessageSeverity severity, const wxString& title, const wxString& text) override {}
	void setupServer(unsigned newPort) override {}
	bool promptForWebpageOpen(std::string URL, const wxString& requesterName, bool& banRequested) override
	{
		this->promptedForWebpage = true;
		this->webpagePromptInfo = { URL, requesterName };
		banRequested = requestBan;
		return confirmPrompts;
	}

	std::optional<wxFileName> fileDestFolder;
	bool promptedForFileSave = false;

	std::pair<ConsentDialog::ResultCode, bool> promptForFileSave(const wxFileName& defaultDestDir, const wxString& requesterName, FileConsentRequestInfo& rqFileInfo) override
	{
		this->promptedForFileSave = true;
		wxFileName destFolder = fileDestFolder.value_or(defaultDestDir);

		if(confirmPrompts)
		{
			for (auto& thisFile : rqFileInfo.fileList)
			{
				thisFile.consentedFileName = destFolder / wxFileName("", thisFile.filename);
			}

			return { ConsentDialog::ResultCode::ACCEPT, requestBan };
		}
		else
		{
			return { ConsentDialog::ResultCode::DECLINE, requestBan };
		}
	}
};

TEST_CASE("OpenWebpageAPIEndpoint tests")
{
	CivetServer testServer({});
	auto configLock = std::make_shared<WriterReadersLock<AppConfig>>(std::make_unique<AppConfig>());
	auto bannedSetLock = WriterReadersLock(std::make_unique<std::set<wxString>>());
	std::mutex dlgMutex;
	mg_connection testConn;
	testConn.requestInfo = { "", "/api/openWebpage", "::1" };

	SECTION("happy path")
	{
		auto wxTestApp = MockGUIApp(true, false);
		OpenWebpageAPIEndpoint endpoint(configLock, wxTestApp, dlgMutex, bannedSetLock);

		testConn.inputBuffer = "url=http://example.com";
		REQUIRE(endpoint.handlePost(&testServer, &testConn));
		REQUIRE(testConn.inputBuffer.empty());
		REQUIRE(testConn.responseStatus == 200);
		REQUIRE(!testConn.isOpen);
		REQUIRE(testConn.outputBuffer.empty());

		REQUIRE(wxTestApp.promptedForWebpage);
		REQUIRE(wxTestApp.webpagePromptInfo.requesterName == "the IP address ::1");
		REQUIRE(wxTestApp.webpagePromptInfo.URL == "http://example.com");

		REQUIRE(decltype(bannedSetLock)::ReadableReference(bannedSetLock)->empty());
	}
	SECTION("unhappy path - request denied")
	{
		auto wxTestApp = MockGUIApp(false, false);
		OpenWebpageAPIEndpoint endpoint(configLock, wxTestApp, dlgMutex, bannedSetLock);

		testConn.inputBuffer = "url=http://example.com";
		REQUIRE(endpoint.handlePost(&testServer, &testConn));
		REQUIRE(testConn.inputBuffer.empty());
		REQUIRE(testConn.responseStatus == 403);
		REQUIRE(!testConn.isOpen);

		REQUIRE(wxTestApp.promptedForWebpage);
		REQUIRE(wxTestApp.webpagePromptInfo.requesterName == "the IP address ::1");
		REQUIRE(wxTestApp.webpagePromptInfo.URL == "http://example.com");

		REQUIRE(decltype(bannedSetLock)::ReadableReference(bannedSetLock)->empty());
	}
	SECTION("unhappy path - request denied and user banned")
	{
		auto wxTestApp = MockGUIApp(false, true);
		OpenWebpageAPIEndpoint endpoint(configLock, wxTestApp, dlgMutex, bannedSetLock);

		testConn.inputBuffer = "url=http://example.com";
		REQUIRE(endpoint.handlePost(&testServer, &testConn));
		REQUIRE(testConn.inputBuffer.empty());
		REQUIRE(testConn.responseStatus == 403);
		REQUIRE(!testConn.isOpen);

		REQUIRE(wxTestApp.promptedForWebpage);
		REQUIRE(wxTestApp.webpagePromptInfo.requesterName == "the IP address ::1");
		REQUIRE(wxTestApp.webpagePromptInfo.URL == "http://example.com");

		REQUIRE(*bannedSetLock.obj == std::set<wxString> { wxT("::1") });
		wxTestApp.confirmPrompts = true;
		wxTestApp.requestBan = false;
		wxTestApp.promptedForWebpage = false;

		mg_connection testConn2;
		testConn2.requestInfo = { "", "/api/openWebpage", "::1" };
		testConn2.inputBuffer = "url=http://example.com";
		REQUIRE(endpoint.handlePost(&testServer, &testConn2));
		REQUIRE(testConn2.responseStatus == 403);
		REQUIRE(!testConn2.isOpen);

		REQUIRE(!wxTestApp.promptedForWebpage);

		REQUIRE(*bannedSetLock.obj == std::set<wxString> { wxT("::1") });
	}
	SECTION("unhappy path - invalid URL")
	{
		auto wxTestApp = MockGUIApp(true, false);
		OpenWebpageAPIEndpoint endpoint(configLock, wxTestApp, dlgMutex, bannedSetLock);

		testConn.inputBuffer = "url=ftp://example.com";
		REQUIRE(endpoint.handlePost(&testServer, &testConn));
		REQUIRE(testConn.inputBuffer.empty());
		REQUIRE(testConn.responseStatus == 400);
		REQUIRE(!testConn.isOpen);
		REQUIRE(nlohmann::json::parse(testConn.outputBuffer)["errors"][0]["fieldName"] == "url");

		REQUIRE(!wxTestApp.promptedForWebpage);
		REQUIRE(decltype(bannedSetLock)::ReadableReference(bannedSetLock)->empty());
	}
}

TEST_CASE("FileConsentTokenService tests")
{
	CivetServer testServer({});
	auto configLock = std::make_shared<WriterReadersLock<AppConfig>>(std::make_unique<AppConfig>());
	auto bannedSetLock = WriterReadersLock(std::make_unique<std::set<wxString>>());
	std::mutex dlgMutex;
	mg_connection testConn;
	testConn.requestInfo = { "", "/api/saveFile", "::1" };

	SECTION("happy path")
	{
		auto wxTestApp = MockGUIApp(true, false);
		FileConsentTokenService endpoint(configLock, dlgMutex, wxTestApp, bannedSetLock);

		testConn.inputBuffer = R"eos({"fileList": [{"filename": "test.txt", "fileSize": 2000}, {"filename": "anotherFile.zip", "fileSize": 2500700853}]})eos";
		auto jsonInput = nlohmann::json::parse(testConn.inputBuffer);

		REQUIRE(endpoint.handlePost(&testServer, &testConn));
		REQUIRE(testConn.inputBuffer.empty());
		REQUIRE(testConn.responseStatus == 200);
		REQUIRE(!testConn.isOpen);

		ConsentToken tokenVal = nlohmann::json::parse(testConn.outputBuffer)["consentToken"];
		REQUIRE(decltype(bannedSetLock)::ReadableReference(bannedSetLock)->empty());

		auto* tokenMap = endpoint.tokenWRRef.obj.get();
		REQUIRE(tokenMap->size() == 1);
		REQUIRE(tokenMap->at(tokenVal).size() == 2);
		REQUIRE(tokenMap->at(tokenVal).at(0).filename == "test.txt");
		REQUIRE(tokenMap->at(tokenVal).at(0).fileSize == 2000);
		REQUIRE(tokenMap->at(tokenVal).at(1).filename == "anotherFile.zip");
		REQUIRE(tokenMap->at(tokenVal).at(1).fileSize == 2500700853);

		REQUIRE(wxTestApp.promptedForFileSave);
	}
}