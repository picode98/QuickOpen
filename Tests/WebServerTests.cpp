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