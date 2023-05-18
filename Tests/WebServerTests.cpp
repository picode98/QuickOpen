#include "catch.hpp"

#include "WebServer.h"
#include "WebServerUtils.h"
#include "AppGUIIncludes.h"

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
		testConn.requestInfo = mg_request_info { "", "/static/image.png", "::1" };
		REQUIRE(handler.handleGet(&testServer, &testConn));
		REQUIRE(testConn.sentFiles.size() == 1);
		REQUIRE(wxFileName(testConn.sentFiles[0].first) == expectedStaticBase / wxFileName(".", "image.png"));
	}
	SECTION("unhappy path - path outside of static root")
	{
		testConn.requestInfo = mg_request_info { "", "/some_dir/image.png", "::1" };
		REQUIRE(handler.handleGet(&testServer, &testConn));
		REQUIRE(testConn.sentFiles.empty());
		REQUIRE(testConn.responseStatus.has_value());
		REQUIRE(testConn.responseStatus.value() == 400);
	}
	SECTION("unhappy path - path with .. symbols")
	{
		testConn.requestInfo = mg_request_info { "", "/static/../image.png", "::1" };
		REQUIRE(handler.handleGet(&testServer, &testConn));
		REQUIRE(testConn.sentFiles.empty());
		REQUIRE(testConn.responseStatus.has_value());
		REQUIRE(testConn.responseStatus.value() == 400);
	}
}

TEST_CASE("OpenWebpageAPIEndpoint tests")
{
	CivetServer testServer({});
	auto bannedSetLock = WriterReadersLock(std::make_unique<std::set<wxString>>());
	std::mutex dlgMutex;
	mg_connection testConn;
	testConn.requestInfo = mg_request_info { "", "/api/openWebpage", "::1" };

	SECTION("happy path")
	{
		auto wxTestApp = QuickOpenApplication(true, false);
		OpenWebpageAPIEndpoint endpoint(wxTestApp, dlgMutex, bannedSetLock);

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
		auto wxTestApp = QuickOpenApplication(false, false);
		OpenWebpageAPIEndpoint endpoint(wxTestApp, dlgMutex, bannedSetLock);

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
		auto wxTestApp = QuickOpenApplication(false, true);
		OpenWebpageAPIEndpoint endpoint(wxTestApp, dlgMutex, bannedSetLock);

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
		testConn2.requestInfo = mg_request_info { "", "/api/openWebpage", "::1" };
		testConn2.inputBuffer = "url=http://example.com";
		REQUIRE(endpoint.handlePost(&testServer, &testConn2));
		REQUIRE(testConn2.responseStatus == 403);
		REQUIRE(!testConn2.isOpen);

		REQUIRE(!wxTestApp.promptedForWebpage);

		REQUIRE(*bannedSetLock.obj == std::set<wxString> { wxT("::1") });
	}
	SECTION("unhappy path - invalid URL")
	{
		auto wxTestApp = QuickOpenApplication(true, false);
		OpenWebpageAPIEndpoint endpoint(wxTestApp, dlgMutex, bannedSetLock);

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
	const std::string testFileInfo = R"eos({"fileList": [{"filename": "test.txt", "fileSize": 2000}, {"filename": "anotherFile.zip", "fileSize": 2500700853}]})eos";

	CivetServer testServer({});
	auto bannedSetLock = WriterReadersLock(std::make_unique<std::set<wxString>>());
	std::mutex dlgMutex;
	mg_connection testConn;
	testConn.requestInfo = mg_request_info { "", "/api/saveFile", "::1" };

	SECTION("happy path")
	{
		auto wxTestApp = QuickOpenApplication(true, false);
		FileConsentTokenService endpoint(dlgMutex, wxTestApp, bannedSetLock);

		testConn.inputBuffer = testFileInfo;
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
	SECTION("unhappy path - request denied")
	{
		auto wxTestApp = QuickOpenApplication(false, false);
		FileConsentTokenService endpoint(dlgMutex, wxTestApp, bannedSetLock);

		testConn.inputBuffer = testFileInfo;
		auto jsonInput = nlohmann::json::parse(testConn.inputBuffer);

		REQUIRE(endpoint.handlePost(&testServer, &testConn));
		REQUIRE(testConn.inputBuffer.empty());
		REQUIRE(testConn.responseStatus == 403);
		REQUIRE(!testConn.isOpen);

		REQUIRE(decltype(bannedSetLock)::ReadableReference(bannedSetLock)->empty());

		REQUIRE(endpoint.tokenWRRef.obj->empty());

		REQUIRE(wxTestApp.promptedForFileSave);
	}
	SECTION("unhappy path - request denied and user banned")
	{
		auto wxTestApp = QuickOpenApplication(false, true);
		FileConsentTokenService endpoint(dlgMutex, wxTestApp, bannedSetLock);

		testConn.inputBuffer = testFileInfo;
		auto jsonInput = nlohmann::json::parse(testConn.inputBuffer);

		REQUIRE(endpoint.handlePost(&testServer, &testConn));
		REQUIRE(testConn.inputBuffer.empty());
		REQUIRE(testConn.responseStatus == 403);
		REQUIRE(!testConn.isOpen);

		REQUIRE(*bannedSetLock.obj.get() == std::set<wxString> { wxT("::1") });

		REQUIRE(endpoint.tokenWRRef.obj->empty());

		REQUIRE(wxTestApp.promptedForFileSave);

		wxTestApp.requestBan = false;
		wxTestApp.confirmPrompts = true;
		wxTestApp.promptedForFileSave = false;

		mg_connection testConn2;
		testConn2.requestInfo = mg_request_info { "", "/api/saveFile", "::1" };
		testConn2.inputBuffer = testFileInfo;

		REQUIRE(endpoint.handlePost(&testServer, &testConn2));
		REQUIRE(testConn.inputBuffer.empty());
		REQUIRE(testConn.responseStatus == 403);
		REQUIRE(!testConn.isOpen);

		REQUIRE(!wxTestApp.promptedForFileSave);

		REQUIRE(*bannedSetLock.obj.get() == std::set<wxString> { wxT("::1") });
	}
}

TEST_CASE("OpenSaveFileAPIEndpoint tests")
{
    CivetServer testServer({});
    auto bannedSetLock = WriterReadersLock(std::make_unique<std::set<wxString>>());
    std::mutex dlgMutex;
    std::string testContent = "The brown fox jumped over the lazy dog.";

    auto wxTestApp = QuickOpenApplication(true, false);
    FileConsentTokenService consentEndpoint(dlgMutex, wxTestApp, bannedSetLock);
    ConsentToken testToken = 3;

    OpenSaveFileAPIEndpoint saveEndpoint(consentEndpoint, wxTestApp);

    SECTION("happy path")
    {
        FileConsentRequestInfo::RequestedFileInfo testFileInfo;
        testFileInfo.filename = wxT("testFile.txt");
        testFileInfo.fileSize = testContent.size();
        testFileInfo.consentedFileName = wxT("testFileConsented.txt");

        {
            WriterReadersLock<FileConsentTokenService::TokenMap>::WritableReference
                    tokens(consentEndpoint.tokenWRRef);
            tokens->insert({ testToken, { testFileInfo } });
        }

        mg_connection testConn;
        testConn.requestInfo = mg_request_info { "consentToken=3&fileIndex=0", "/api/saveFile", "::1" };
        testConn.inputBuffer = testContent;

        REQUIRE(saveEndpoint.handlePost(&testServer, &testConn));
        REQUIRE(testConn.inputBuffer.empty());
        REQUIRE(testConn.responseStatus == 200);
        REQUIRE(!testConn.isOpen);
        REQUIRE(testFileInfo.consentedFileName.FileExists());
        REQUIRE(testFileInfo.consentedFileName.GetSize() == testContent.size());
        std::string actualContent = fileReadAll(testFileInfo.consentedFileName);
        REQUIRE(actualContent == testContent);
    }
    SECTION("unhappy path - invalid consent token or file index")
    {
        FileConsentRequestInfo::RequestedFileInfo testFileInfo;
        testFileInfo.filename = wxT("testFile.txt");
        testFileInfo.fileSize = testContent.size();
        testFileInfo.consentedFileName = wxT("testFileConsented2.txt");

        {
            WriterReadersLock<FileConsentTokenService::TokenMap>::WritableReference
                    tokens(consentEndpoint.tokenWRRef);
            tokens->insert({ testToken, { testFileInfo } });
        }

        mg_connection testConn;
        testConn.requestInfo = mg_request_info { "consentToken=4&fileIndex=0", "/api/saveFile", "::1" };
        testConn.inputBuffer = testContent;

        REQUIRE(saveEndpoint.handlePost(&testServer, &testConn));
        REQUIRE(testConn.responseStatus == 403);
        REQUIRE(!testConn.isOpen);
        REQUIRE(!testFileInfo.consentedFileName.FileExists());

        mg_connection testConn2;
        testConn2.requestInfo = mg_request_info { "consentToken=3&fileIndex=1", "/api/saveFile", "::1" };
        testConn2.inputBuffer = testContent;

        REQUIRE(saveEndpoint.handlePost(&testServer, &testConn2));
        REQUIRE(testConn2.responseStatus == 403);
        REQUIRE(!testConn2.isOpen);
        REQUIRE(!testFileInfo.consentedFileName.FileExists());
    }
}