#pragma once

#define UNICODE

#include <wx/string.h>
#include <wx/filename.h>

#include <nlohmann/json.hpp>

// #include "AppGUI.h"
#include "TrayStatusWindow.h"
#include "AppConfig.h"
#include "Utils.h"

#include <atomic>
#include <string>


#include <CivetServer.h>

// #include <boost/process.hpp>

// #include <Poco/URI.h>

// #include <fstream>
#include <filesystem>
#include <map>
#include <sstream>
#include <condition_variable>
#include <iostream>
#include <optional>
#include <set>

#include "PlatformUtils.h"

const std::filesystem::path STATIC_PATH = std::filesystem::current_path() / "static";
const int PORT = 8080;

typedef uint32_t ConsentToken;

class TrayStatusWindow;
class QuickOpenApplication;

std::string URLDecode(const std::string& encodedStr, bool decodePlus);

#ifdef _WIN32

inline void openURL(const std::string& URL, const tstring& browserCommandLineFormat = getDefaultBrowserCommandLine())
{
#ifdef UNICODE
	std::wstring URLWStr = UTF8StrToWideStr(URL);
	tstring browserCommandLine = substituteWinShellFormatString(browserCommandLineFormat, { URLWStr });
	// startSubprocess(browserCommandLine);
#else
	// startSubprocess(browserExecPath, { URL });
	tstring browserCommandLine = substituteWinShellFormatString(browserCommandLineFormat, { URL });
#endif
	startSubprocess(browserCommandLine);
}
#endif

std::map<std::string, std::string> parseFormEncodedBody(mg_connection* conn);

std::map<std::string, std::string> parseQueryString(mg_connection* conn);

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

void sendJSONResponse(mg_connection* conn, int status, const nlohmann::json& json);

class TestHandler : public CivetHandler
{
	bool handleGet(CivetServer* server, mg_connection* conn) override
	{
		std::string content = "This is a test.";
		mg_send_http_ok(conn, "text/plain", content.size());
		mg_write(conn, content.c_str(), content.size());

		return true;
	}
};

class StaticHandler : public CivetHandler
{
private:
	const std::string staticPrefix;
    const wxFileName baseStaticPath;
public:
	StaticHandler(const std::string& staticPrefix) : staticPrefix(staticPrefix), baseStaticPath(InstallationInfo::detectInstallation().staticFolder)
	{}

	bool handleGet(CivetServer* server, mg_connection* conn) override;
};

class OpenWebpageAPIEndpoint : public CivetHandler
{
	std::shared_ptr<WriterReadersLock<AppConfig>> configLock;
	QuickOpenApplication& wxAppRef;
	std::mutex& consentDialogMutex;
	WriterReadersLock<std::set<wxString>>& bannedIPRef;

public:
	OpenWebpageAPIEndpoint(const std::shared_ptr<WriterReadersLock<AppConfig>>& configLock, std::mutex& consentDialogMutex, QuickOpenApplication& wxAppRef, WriterReadersLock<std::set<wxString>>& bannedIPRef):
		configLock(configLock),
		consentDialogMutex(consentDialogMutex),
		wxAppRef(wxAppRef),
		bannedIPRef(bannedIPRef)
	{}


	bool handlePost(CivetServer* server, mg_connection* conn) override;
};

struct FileConsentRequestInfo
{
	struct RequestedFileInfo
	{
		wxString filename;
		unsigned long long fileSize;

		wxFileName consentedFileName;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(RequestedFileInfo, filename, fileSize)

			//static RequestedFileInfo fromJSON(const nlohmann::json& json)
			//{
			//	return { wxString::FromUTF8(json["filename"]), json["fileSize"] };
			//}

			//nlohmann::json toJSON()
			//{
			//	return {
			//		{ "filename", filename.ToUTF8() },
			//		{ "fileSize", fileSize }
			//	};
			//}
	};

	std::vector<RequestedFileInfo> fileList;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(FileConsentRequestInfo, fileList)
};

class FileConsentTokenService : public CivetHandler
{
	// static const size_t MAX_REQUEST_BODY_SIZE = 1 << 16;

public:
	typedef std::map<ConsentToken, FileConsentRequestInfo::RequestedFileInfo> TokenMap;
	WriterReadersLock<TokenMap> tokenWRRef;
private:
	// TokenMap tokens;
	std::shared_ptr<WriterReadersLock<AppConfig>> configLock;
	QuickOpenApplication& wxAppRef;
	std::mutex& consentDialogMutex;
	WriterReadersLock<std::set<wxString>>& bannedIPRef;

public:
	FileConsentTokenService(const std::shared_ptr<WriterReadersLock<AppConfig>>& configLock, std::mutex& consentDialogMutex, QuickOpenApplication& wxAppRef, WriterReadersLock<std::set<wxString>>& bannedIPRef) :
		tokenWRRef(std::make_unique<TokenMap>()), configLock(configLock),
		consentDialogMutex(consentDialogMutex),
		wxAppRef(wxAppRef),
		bannedIPRef(bannedIPRef)
	{}

	bool handlePost(CivetServer* server, mg_connection* conn) override;
};

class OpenSaveFileAPIEndpoint : public CivetHandler
{
	// WriterReadersLock<AppConfig>& configLock;
	FileConsentTokenService& consentServiceRef;
	QuickOpenApplication& progressReportingApp;

	class IncorrectFileLengthException : std::exception
	{};

	class OperationCanceledException : std::exception
	{};

	void MGStoreBodyChecked(mg_connection* conn, const wxFileName& fileName, unsigned long long targetFileSize,
		TrayStatusWindow::FileUploadActivityEntry* uploadActivityEntryRef, std::atomic<bool>& cancelRequestFlag);

	// TrayStatusWindow* statusWindow = nullptr;
public:
	OpenSaveFileAPIEndpoint(FileConsentTokenService& consentServiceRef, QuickOpenApplication& progressReportingApp) : consentServiceRef(consentServiceRef),
		progressReportingApp(progressReportingApp)
	{}

	bool handlePost(CivetServer* server, mg_connection* conn) override;
};

class QuickOpenWebServer : public CivetServer
{
	std::shared_ptr<WriterReadersLock<AppConfig>> wrLock;
	std::mutex consentDialogMutex;

	WriterReadersLock<std::set<wxString>> bannedIPs;

	TestHandler testHandler;
	StaticHandler staticHandler;
	OpenWebpageAPIEndpoint webpageAPIEndpoint;
	FileConsentTokenService fileConsentTokenService;
	OpenSaveFileAPIEndpoint fileAPIEndpoint;
public:
	QuickOpenWebServer(const std::shared_ptr<WriterReadersLock<AppConfig>>& wrLock, QuickOpenApplication& wxAppRef):
		CivetServer({
			"document_root", STATIC_PATH.generic_string(),
			"listening_ports", '+' + std::to_string(PORT)
		}),
		wrLock(wrLock),
		staticHandler("/"),
		webpageAPIEndpoint(wrLock, consentDialogMutex, wxAppRef, bannedIPs),
		fileConsentTokenService(wrLock, consentDialogMutex, wxAppRef, bannedIPs),
		fileAPIEndpoint(fileConsentTokenService, wxAppRef),
		bannedIPs(std::make_unique<std::set<wxString>>())
	{
		this->addHandler("/test", testHandler);
		this->addHandler("/", staticHandler);
		this->addHandler("/api/openWebpage", webpageAPIEndpoint);
		this->addHandler("/api/openSaveFile", fileAPIEndpoint);
		this->addHandler("/api/openSaveFile/getConsent", fileConsentTokenService);
	}
};

//int updateSessionCookie(mg_connection* conn)
//{
//	static const std::string SESSION_COOKIE_NAME = "_session_id", SESSION_VAR_NAME = "id";
//	static const size_t SESSION_ID_STR_SIZE = std::numeric_limits<SessionID>::digits10 + 1;
//	char existingSessIDStr[SESSION_ID_STR_SIZE];
//	mg_get_request_info(conn)->http_headers[0].
//	int getCookieResult = mg_get_cookie(SESSION_COOKIE_NAME.c_str(), SESSION_VAR_NAME.c_str(), existingSessIDStr, SESSION_ID_STR_SIZE);
//	mg_context* ctx = mg_get_context(conn);
//	ctx->
//	if(getCookieResult < 0)
//	{
//		SessionID newID = generateCryptoRandomInteger<SessionID>();
//
//		mg_s
//	}
//}

//int main(int argc, char** argv)
//{

	// appGUI.getTrayWindow()->addWebpageOpenedActivity(wxT("foo.com"));
	// appGUI.getTrayWindow()->addWebpageOpenedActivity(wxT("bar.com"));
	//appGUI.OnRun();

	//wxEntryCleanup();
	
//}