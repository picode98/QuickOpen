#pragma once

#define UNICODE

#include <wx/string.h>
#include <wx/filename.h>

#include <nlohmann/json.hpp>

#include "AppGUI.h"
#include "TrayStatusWindow.h"
#include "AppConfig.h"
#include "Utils.h"

#include <atomic>
#include <string>


#include "CivetWebIncludes.h"

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

typedef uint32_t ConsentToken;

class TrayStatusWindow;
class QuickOpenApplication;

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

class StaticHandler : public CivetHandler
{
private:
	const std::string staticPrefix;
    const wxFileName baseStaticPath;
public:
	StaticHandler(const std::string& staticPrefix) : staticPrefix(staticPrefix), baseStaticPath(InstallationInfo::detectInstallation().dataFolder / wxFileName("static", ""))
	{}

	bool handleGet(CivetServer* server, mg_connection* conn) override;
};

class OpenWebpageAPIEndpoint : public CivetHandler
{
	IQuickOpenApplication& wxAppRef;
	std::shared_ptr<WriterReadersLock<AppConfig>> configLock;
	std::mutex& consentDialogMutex;
	WriterReadersLock<std::set<wxString>>& bannedIPRef;
	// IQuickOpenApplication 

public:
	OpenWebpageAPIEndpoint(const std::shared_ptr<WriterReadersLock<AppConfig>>& configLock, IQuickOpenApplication& wxAppRef, std::mutex& consentDialogMutex, WriterReadersLock<std::set<wxString>>& bannedIPRef):
		configLock(configLock),
		wxAppRef(wxAppRef),
		consentDialogMutex(consentDialogMutex),
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
		bool uploadStarted = false,
			uploadEnded = false;

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
	typedef std::map<ConsentToken, std::vector<FileConsentRequestInfo::RequestedFileInfo>> TokenMap;
	WriterReadersLock<TokenMap> tokenWRRef;
private:
	// TokenMap tokens;
	std::shared_ptr<WriterReadersLock<AppConfig>> configLock;
	IQuickOpenApplication& wxAppRef;
	std::mutex& consentDialogMutex;
	WriterReadersLock<std::set<wxString>>& bannedIPRef;

public:
	FileConsentTokenService(const std::shared_ptr<WriterReadersLock<AppConfig>>& configLock, std::mutex& consentDialogMutex, IQuickOpenApplication& wxAppRef, WriterReadersLock<std::set<wxString>>& bannedIPRef) :
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

	class IncorrectFileLengthException : public std::runtime_error
	{
	public:
		IncorrectFileLengthException() : std::runtime_error("The length of the uploaded file differs from the consented length")
		{}
	};

	class OperationCanceledException : public std::runtime_error
	{
	public:
		OperationCanceledException() : std::runtime_error("The operation was canceled by the user")
		{}
	};

	class ConnectionClosedException : public std::runtime_error
	{
	public:
		ConnectionClosedException() : std::runtime_error("The connection was closed")
		{}
	};

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
	QuickOpenApplication& wxAppRef;

	std::shared_ptr<WriterReadersLock<AppConfig>> wrLock;
	std::mutex consentDialogMutex;

	WriterReadersLock<std::set<wxString>> bannedIPs;

	StaticHandler staticHandler;
	OpenWebpageAPIEndpoint webpageAPIEndpoint;
	FileConsentTokenService fileConsentTokenService;
	OpenSaveFileAPIEndpoint fileAPIEndpoint;

	void onWebpageOpened(const wxString& url);
	unsigned port;
public:
	QuickOpenWebServer(const std::shared_ptr<WriterReadersLock<AppConfig>>& wrLock, QuickOpenApplication& wxAppRef, unsigned port);

	unsigned getPort() const
	{
		return this->port;
	}

	~QuickOpenWebServer() override
	{
		this->close();
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