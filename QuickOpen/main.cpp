#define UNICODE

#include "AppGUI.h"
#include "Utils.h"

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

#ifdef _WIN32
#include "WinUtils.h"
#endif

const std::filesystem::path STATIC_PATH = std::filesystem::current_path() / "static";
const int PORT = 80;

typedef uint32_t ConsentToken;

template<typename ObjType, typename PrefixType>
bool startsWith(const ObjType& obj, const PrefixType& prefix)
{
	auto objIter = obj.begin();
	auto prefixIter = prefix.begin();
	
	for (; objIter != obj.end() && prefixIter != prefix.end();
			++objIter, ++prefixIter)
	{
		if (*objIter != *prefixIter) return false;
	}

	return (prefixIter == prefix.end());
}

std::string URLDecode(const std::string& encodedStr, bool decodePlus)
{
	std::ostringstream outputStream;

	for(size_t i = 0; i < encodedStr.size();)
	{
		switch (encodedStr[i])
		{
		case '%':
			outputStream << char(std::stoi(encodedStr.substr(i + 1, 2), nullptr, 16));
			i += 3;
			break;
		case '+':
			if (decodePlus)
			{
				outputStream << ' ';
				++i;
				break;
			}
		default:
			outputStream << encodedStr[i];
			++i;
			break;
		}
	}

	return outputStream.str();
}

#ifdef _WIN32

void openURL(const std::string& URL, const tstring& browserCommandLineFormat = getDefaultBrowserCommandLine())
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

std::map<std::string, std::string> parseFormEncodedBody(mg_connection* conn)
{
	static const long long CHUNK_SIZE = 1LL << 16;
	char bodyBuffer[CHUNK_SIZE];
	std::string currentKey, currentValue;
	bool readingValue = false;
	// const mg_request_info* rqInfo = mg_get_request_info(conn);

	std::map<std::string, std::string> resultMap;

	// long long maxContentLength = rqInfo->content_length == -1 ? std::numeric_limits<long long>::max() : rqInfo->content_length;
	int bytesRead;
	while ((bytesRead = mg_read(conn, bodyBuffer, CHUNK_SIZE)) > 0)
	{
		for(int i = 0; i < bytesRead; ++i)
		{
			switch (bodyBuffer[i])
			{
			case '&':
				readingValue = false;
				resultMap.insert({ URLDecode(currentKey, true), URLDecode(currentValue, true) });
				currentKey = currentValue = "";
				break;
			case '=':
				readingValue = true;
				break;
			default:
				if(readingValue)
				{
					currentValue += bodyBuffer[i];
				}
				else
				{
					currentKey += bodyBuffer[i];
				}

				break;
			}
		}
	}

	resultMap.insert({ URLDecode(currentKey, true), URLDecode(currentValue, true) });

	return resultMap;
}

std::map<std::string, std::string> parseQueryString(mg_connection* conn)
{
	const char* queryStringPtr = mg_get_request_info(conn)->query_string;
	if (queryStringPtr == nullptr) return {};
	
	std::string queryString = queryStringPtr;
	
	std::string currentKey, currentValue;
	bool readingValue = false;

	std::map<std::string, std::string> resultMap;

	for (size_t i = 0; i < queryString.size(); ++i)
	{
		switch (queryString[i])
		{
		case '&':
			readingValue = false;
			resultMap.insert({ URLDecode(currentKey, false), URLDecode(currentValue, false) });
			currentKey = currentValue = "";
			break;
		case '=':
			readingValue = true;
			break;
		default:
			if (readingValue)
			{
				currentValue += queryString[i];
			}
			else
			{
				currentKey += queryString[i];
			}

			break;
		}
	}

	resultMap.insert({ URLDecode(currentKey, false), URLDecode(currentValue, false) });

	return resultMap;
}

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

void sendJSONResponse(mg_connection* conn, int status, const nlohmann::json& json)
{
	mg_response_header_start(conn, status);
	mg_response_header_add(conn, "Content-Type", "application/json", -1);
	mg_response_header_send(conn);
	std::string jsonString = json.dump();
	mg_write(conn, jsonString.c_str(), jsonString.size());
}

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
public:
	StaticHandler(const std::string& staticPrefix) : staticPrefix(staticPrefix)
	{}
	
	bool handleGet(CivetServer* server, mg_connection* conn) override
	{
		const mg_request_info* rqInfo = mg_get_request_info(conn);
		std::string_view rqURI(rqInfo->request_uri);

		if(rqURI.find("..") != std::string::npos)
		{
			mg_send_http_error(conn, 400, "The path requested contains invalid sequences (\"..\").");
			return true;
		}
		
		if(startsWith(rqURI, this->staticPrefix))
		{
			std::string_view truncPath = rqURI;
			truncPath.remove_prefix(this->staticPrefix.size());
			
			std::filesystem::path resolvedPath = (STATIC_PATH / truncPath).lexically_normal();
			if(!resolvedPath.has_extension())
			{
				if(!resolvedPath.has_filename())
				{
					resolvedPath.replace_filename("index.html");
				}
				else
				{
					resolvedPath.replace_extension("html");
				}
			}
			
			mg_send_mime_file(conn, resolvedPath.generic_string().c_str(), nullptr);
		}
		else
		{
			mg_send_http_error(conn, 400, "The path requested is invalid for serving static files.");
		}

		return true;
	}
};

class OpenWebpageAPIEndpoint : public CivetHandler
{
	WriterReadersLock<AppConfig>& configLock;
	TrayStatusWindow* statusWindowRef;

public:
	OpenWebpageAPIEndpoint(WriterReadersLock<AppConfig>& configLock, TrayStatusWindow* statusWindowRef): configLock(configLock),
		statusWindowRef(statusWindowRef)
	{}

	
	bool handlePost(CivetServer* server, mg_connection* conn) override
	{
		auto postParams = parseFormEncodedBody(conn);

		if(postParams.count("url") > 0)
		{
			std::string& url = postParams["url"];
			if (std::regex_match(url, std::regex("^https?:(\\/\\/)?[a-z|\\d|-|\\.]+($|\\/\\S*$)",
				std::regex_constants::icase | std::regex_constants::ECMAScript)))
			{
				if (promptForWebpageOpen(url))
				{
					wxString wxURL = wxString::FromUTF8(postParams["url"]);
					
					{
						WriterReadersLock<AppConfig>::ReadableReference readRef(configLock);

						if(readRef->browserID.empty())
						{
							if (readRef->customBrowserPath.empty())
							{
								auto defaultBrowserCommand = substituteFormatString(getDefaultBrowserCommandLine(), wxUniChar('%'), { wxURL }, { });
								startSubprocess(defaultBrowserCommand);
							}
							else
							{
								auto browserCommand = substituteFormatString(readRef->customBrowserPath, wxUniChar('$'), { }, { { "url", wxURL } });
								startSubprocess(browserCommand);
							}
						}
						else
						{
							startSubprocess(getBrowserCommandLine(readRef->browserID) + " \"" + wxURL + "\"");
						}
					}

					wxGetApp().CallAfter([this, wxURL]
					{
						this->statusWindowRef->addWebpageOpenedActivity(wxURL);
					});
					
					std::cout << "Will open " << wxURL << std::endl;
					

					mg_send_http_ok(conn, "text/plain", 0);
				}
				else
				{
					auto jsonErrorInfo = nlohmann::json(FormErrorList{ {
						{ "", "Opening of the webpage was denied by the user." }
					} });
					sendJSONResponse(conn, 403, jsonErrorInfo);
				}
			}
			else
			{
				auto jsonErrorInfo = nlohmann::json(FormErrorList{ {
					{ "url", "This URL is not valid." }
				} });

				sendJSONResponse(conn, 400, jsonErrorInfo);
			}
		}
		else
		{
			auto jsonErrorInfo = nlohmann::json(FormErrorList{ {
					{ "url", "The URL is missing." }
				} });
			sendJSONResponse(conn, 400, jsonErrorInfo);
		}

		return true;
		// mg_read
		// auto systemShell = boost::process::shell();
		// boost::process::child(systemShell, "foo");
	}
};

struct RequestedFileInfo
{
	wxString filename;
	unsigned long long fileSize;

	wxFileName consentedFileName;

	static RequestedFileInfo fromJSON(const nlohmann::json& json)
	{
		return { wxString::FromUTF8(json["filename"]), json["fileSize"] };
	}

	nlohmann::json toJSON()
	{
		return {
			{ "filename", filename.ToUTF8() },
			{ "fileSize", fileSize }
		};
	}
};

class FileConsentTokenService : public CivetHandler
{
	static const size_t MAX_REQUEST_BODY_SIZE = 1 << 16;

public:
	typedef std::map<ConsentToken, RequestedFileInfo> TokenMap;
	WriterReadersLock<TokenMap> tokenWRRef;
private:
	TokenMap tokens;
	WriterReadersLock<AppConfig>& configLock;

public:
	FileConsentTokenService(WriterReadersLock<AppConfig>& configLock) : tokenWRRef(&tokens), configLock(configLock)
	{}

	bool handlePost(CivetServer* server, mg_connection* conn) override
	{
		std::string bodyStr = MGReadAll(conn);
		auto rqFileInfo = RequestedFileInfo::fromJSON(nlohmann::json::parse(bodyStr));

		wxFileName destPath;
		{
			WriterReadersLock<AppConfig>::ReadableReference configRef(configLock);
			destPath = wxFileName(configRef->fileSavePath);
			destPath.SetFullName(rqFileInfo.filename);
		}
		
		//std::optional<FileOpenSaveConsentDialog::ResultCode> resultCode;
		//std::condition_variable dialogResult;
		//std::mutex dialogResultMutex;
		//std::unique_lock<std::mutex> dialogResultLock(dialogResultMutex);

		auto consentDlgLambda = [&destPath, &rqFileInfo]
		{
			auto consentDlg = FileOpenSaveConsentDialog(destPath, rqFileInfo.fileSize);
			auto resultVal = static_cast<FileOpenSaveConsentDialog::ResultCode>(consentDlg.ShowModal());
			rqFileInfo.consentedFileName = consentDlg.getConsentedFilename();
			return resultVal;
		};
		
		FileOpenSaveConsentDialog::ResultCode result =
			wxCallAfterSync<QuickOpenApplication, decltype(consentDlgLambda), FileOpenSaveConsentDialog::ResultCode>(wxGetApp(), consentDlgLambda);
		//wxGetApp().CallAfter([&dialogResultMutex, &dialogResult, &resultCode, &destPath, &rqFileInfo]
		//{
		//	auto consentDlg = FileOpenSaveConsentDialog(destPath, rqFileInfo.fileSize);
		//	auto dlgResult = static_cast<FileOpenSaveConsentDialog::ResultCode>(consentDlg.ShowModal());
		//	// consentDlg.Show();
		//	std::lock_guard<std::mutex> resultLock(dialogResultMutex);
		//	resultCode = dlgResult;

		//	if(resultCode.value() == FileOpenSaveConsentDialog::ACCEPT)
		//	{
		//		rqFileInfo.consentedFileName = consentDlg.getConsentedFilename();
		//	}
		//	dialogResult.notify_all();
		//});
		
		if (result == FileOpenSaveConsentDialog::ACCEPT)
		{
			// rqFileInfo.consentedFileName = consentDlg.getConsentedFilename();
			ConsentToken newToken = generateCryptoRandomInteger<ConsentToken>();

			{
				WriterReadersLock<TokenMap>::WritableReference writeRef(tokenWRRef);
				writeRef->insert({ newToken, rqFileInfo });
			}

			std::string jsonResponse = nlohmann::json{ { "consentToken", newToken } }.dump();
			mg_send_http_ok(conn, "application/json", jsonResponse.size());
			mg_write(conn, jsonResponse.c_str(), jsonResponse.size());
		}
		else
		{
			auto jsonErrorInfo = nlohmann::json(FormErrorList{ {
					{ "uploadFile", "The user declined to receive the file." }
			} });
			sendJSONResponse(conn, 403, jsonErrorInfo);
		}

		return true;
	}
};

class OpenSaveFileAPIEndpoint : public CivetHandler
{
	// WriterReadersLock<AppConfig>& configLock;
	FileConsentTokenService& consentServiceRef;

	/*
	static int getFieldInfo(const char *key, const char *filename, char *path, size_t pathlen, void *user_data)
	{
		auto* consentedFileInfo = reinterpret_cast<RequestedFileInfo*>(user_data);

		if (std::string(filename) == consentedFileInfo->filename)
		{
			std::filesystem::path destPath = std::filesystem::path("C:\\Users\\saama\\Downloads") / filename;
			strncpy_s(path, pathlen, WideStrToUTF8Str(destPath).c_str(), _TRUNCATE);
			return MG_FORM_FIELD_STORAGE_STORE;
		}
		else
		{
			return MG_FORM_FIELD_STORAGE_ABORT;
		}
	}

	static int onFileStore(const char *path, long long file_size, void *user_data)
	{
		std::cout << "Wrote " << file_size << " bytes of data to " << path << std::endl;
		return MG_FORM_FIELD_HANDLE_NEXT;
	}
	*/

	//int readHalfBuffer(mg_connection* conn, char* buffer, size_t bufSize)
	//{
	//	auto maxAmtToRead = bufSize / 2;
	//	char* readBuf = new char[maxAmtToRead];

	//	int bytesRead = mg_read(conn, readBuf, maxAmtToRead);
	//	memcpy(buffer, buffer + bytesRead, bufSize - bytesRead);
	//	memcpy(buffer + (bufSize - bytesRead), readBuf, bytesRead);

	//	return bytesRead;
	//}

	//void writeChunk(size_t partNum, std::string_view chunk)
	//{
	//	std::ofstream partOutStream(std::to_string(partNum) + ".txt", std::ofstream::app | std::ofstream::binary);
	//	partOutStream << chunk;
	//	partOutStream.close();
	//}

	//void parseMultipartBodyChunked(mg_connection* conn)
	//{
	//	static const long long CHUNK_SIZE = 1LL << 16;
	//	char* bodyBuffer = new char[CHUNK_SIZE + 1];
	//	char* bodyBufferEnd = bodyBuffer + CHUNK_SIZE;
	//	bodyBuffer[CHUNK_SIZE] = '\0';

	//	std::string contentType = mg_get_header(conn, "Content-Type");
	//	const std::string BOUNDARY_ATTR = "boundary=", HEADER_END_STR = "\r\n\r\n";
	//	auto boundaryAttrStart = contentType.find(BOUNDARY_ATTR);

	//	if(boundaryAttrStart == std::string::npos)
	//	{
	//		return;
	//	}

	//	std::string boundaryStr = "--" + contentType.substr(boundaryAttrStart + BOUNDARY_ATTR.size());
	//	
	//	/*mg_read(conn, bodyBuffer, CHUNK_SIZE);

	//	char* unprocessedStart = nullptr;
	//	while((unprocessedStart = std::search(bodyBuffer, bodyBufferEnd, boundaryStr.begin(), boundaryStr.end())) == bodyBufferEnd)
	//	{
	//		if(readHalfBuffer(conn, bodyBuffer, CHUNK_SIZE) == 0)
	//		{
	//			return;
	//		}
	//	}

	//	unprocessedStart += boundaryStr.size();*/
	//	char* unprocessedStart = bodyBuffer;
	//	char* copyStart = unprocessedStart;
	//	size_t partNum = 0;
	//	int bytesRead;
	//	bool headerSection = false;
	//	do
	//	{
	//		if(headerSection)
	//		{
	//			if((unprocessedStart = std::search(copyStart, bodyBufferEnd, HEADER_END_STR.begin(), HEADER_END_STR.end())) == bodyBufferEnd)
	//			{
	//				
	//			}
	//		}
	//		while ((unprocessedStart = std::search(copyStart, bodyBufferEnd, boundaryStr.begin(), boundaryStr.end())) != bodyBufferEnd)
	//		{
	//			if (partNum > 0)
	//			{
	//				writeChunk(partNum, std::string_view(copyStart, unprocessedStart - copyStart));
	//			}
	//			++partNum;
	//			copyStart = unprocessedStart + boundaryStr.size();
	//		}

	//		// std::string_view(copyStart, bodyBufferEnd - copyStart);

	//		bytesRead = readHalfBuffer(conn, bodyBuffer, CHUNK_SIZE);
	//		unprocessedStart = ((unprocessedStart - bytesRead) - bodyBuffer > 0) ? (unprocessedStart - bytesRead) : bodyBuffer;
	//	} while (bytesRead > 0);

	//	delete[] bodyBuffer;
	//	
	//	//std::string_view boundaryFirstHalf(boundaryStr.c_str(), boundaryStr.size() / 2),
	//	//	boundarySecondHalf(boundaryStr.c_str() + (boundaryStr.size() / 2));
	//	//
	//	//int bytesRead;
	//	//while ((bytesRead = mg_read(conn, bodyBuffer.data(), CHUNK_SIZE + 1)) > 0)
	//	//{
	//	//	auto firstPartStart = bodyBuffer.find(boundaryFirstHalf)
	//	//}
	//}

	class IncorrectFileLengthException : std::exception
	{};

	void MGStoreBodyChecked(mg_connection* conn, const wxFileName& fileName, unsigned long long targetFileSize,
		TrayStatusWindow::FileUploadActivityEntry* uploadActivityEntryRef)
	{
		static const long long CHUNK_SIZE = 1LL << 20;
		unsigned long long bytesWritten = 0;
		auto bodyBuffer = std::make_unique<char[]>(CHUNK_SIZE);
		
		std::ofstream outFile;
		outFile.exceptions(std::ofstream::failbit);

		try
		{
			outFile.open(fileName.GetFullPath().ToStdWstring(), std::ofstream::binary);

			int bytesRead;
			while ((bytesRead = mg_read(conn, bodyBuffer.get(), CHUNK_SIZE)) > 0)
			{
				bytesWritten += bytesRead;

				if(bytesWritten > targetFileSize)
				{
					throw IncorrectFileLengthException();
				}
				
				outFile.write(bodyBuffer.get(), bytesRead);

				wxGetApp().CallAfter([uploadActivityEntryRef, bytesWritten, targetFileSize]
				{
					uploadActivityEntryRef->setProgress((static_cast<double>(bytesWritten) / targetFileSize) * 100.0);
				});
			}

			wxGetApp().CallAfter([uploadActivityEntryRef]
			{
				uploadActivityEntryRef->setCompleted(true);
			});
		}
		catch (const std::ios_base::failure& ex)
		{
#ifdef WIN32
			WindowsException detailedError = getWinAPIError(ERROR_SUCCESS);
#endif
			wxGetApp().CallAfter([uploadActivityEntryRef, detailedError]
			{
				uploadActivityEntryRef->setError(&detailedError);
			});

			throw detailedError;
		}
		
		// delete[] bodyBuffer;
		outFile.close();

		if(bytesWritten != targetFileSize)
		{
			throw IncorrectFileLengthException();
		}
	}

	TrayStatusWindow* statusWindow = nullptr;
public:
	OpenSaveFileAPIEndpoint(FileConsentTokenService& consentServiceRef, TrayStatusWindow* statusWindow) : consentServiceRef(consentServiceRef),
		statusWindow(statusWindow)
	{}
	
	bool handlePost(CivetServer* server, mg_connection* conn) override
	{
		//auto* rq_info = mg_get_request_info(conn);
		//
		//{
		//	WriterReadersLock<AppConfig>::ReadableReference readRef(configLock);

		//	if(rq_info->content_length > readRef->maxSaveFileSize)
		//	{
		//		mg_send_http_error(conn, 413, "The file included with this request is too large.");
		//		return true;
		//	}
		//}

		auto queryStringMap = parseQueryString(conn);

		if(queryStringMap.count("consentToken") > 0)
		{
			ConsentToken parsedToken = atoll(queryStringMap["consentToken"].c_str());
			RequestedFileInfo consentedFileInfo;
			bool fileFound = false;

			{
				WriterReadersLock<FileConsentTokenService::TokenMap>::WritableReference tokens(consentServiceRef.tokenWRRef);

				if (tokens->count(parsedToken) > 0)
				{
					consentedFileInfo = tokens->at(parsedToken);
					fileFound = true;

					tokens->erase(parsedToken);
				}
			}

			if(fileFound)
			{
				/*mg_form_data_handler formDataHandler;
				formDataHandler.field_found = getFieldInfo;
				formDataHandler.field_get = nullptr;
				formDataHandler.field_store = onFileStore;
				formDataHandler.user_data = &consentedFileInfo;
				

				mg_handle_form_request(conn, &formDataHandler);*/

				//wxFileName destPath;
				//{
				//	WriterReadersLock<AppConfig>::ReadableReference configRef(configLock);
				//	destPath = wxFileName(configRef->fileSavePath);
				//	destPath.SetFullName(consentedFileInfo.filename);
				//}

				// TrayStatusWindow::FileUploadActivityEntry* activityEntryRef = nullptr;
				auto createActivity = [this, consentedFileInfo]
				{
					return this->statusWindow->addFileUploadActivity(consentedFileInfo.consentedFileName);
				};
				
				TrayStatusWindow::FileUploadActivityEntry* activityEntryRef = 
					wxCallAfterSync<QuickOpenApplication, decltype(createActivity), TrayStatusWindow::FileUploadActivityEntry*>(wxGetApp(), createActivity);
				
				try
				{
					MGStoreBodyChecked(conn, consentedFileInfo.consentedFileName, consentedFileInfo.fileSize, activityEntryRef);
					mg_send_http_ok(conn, "text/plain", 0);
				}
				catch(const std::system_error& ex)
				{
					auto jsonErrorInfo = nlohmann::json(FormErrorList{ {
						{"uploadFile", std::string("An error occurred while attempting to write the file: ") + ex.what()}
					} });
					sendJSONResponse(conn, 500, jsonErrorInfo);
				}
				catch (const IncorrectFileLengthException&)
				{
					auto jsonErrorInfo = nlohmann::json(FormErrorList{ {
						{"uploadFile", "The file sent did not have the length specified by the consent token used."}
					} });
					sendJSONResponse(conn, 400, jsonErrorInfo);
				}
			}
			else
			{
				// mg_send_http_error(conn, 403, "The consent token provided was not valid.");
				auto jsonErrorInfo = nlohmann::json(FormErrorList{ {
						{"consentToken", "The consent token provided was not valid."}
					} });
				sendJSONResponse(conn, 403, jsonErrorInfo);
			}
		}
		else
		{
			// mg_send_http_error(conn, 401, "No consent token was provided.");
			auto jsonErrorInfo = nlohmann::json(FormErrorList{ {
						{"consentToken", "No consent token was provided."}
					} });
			sendJSONResponse(conn, 401, jsonErrorInfo);
		}
		
		return true;
		
		// std::ofstream outFile(destPath, std::ofstream::binary);
		
		/*static const long long CHUNK_SIZE = 1LL << 20;
		char* bodyBuffer = new char[CHUNK_SIZE];
		
		int bytesRead;
		while ((bytesRead = mg_read(conn, bodyBuffer, CHUNK_SIZE)) > 0)
		{
			outFile.write(bodyBuffer, bytesRead);
		}

		delete[] bodyBuffer;
		outFile.close();
		return true;*/
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

int main(int argc, char** argv)
{
	AppConfig config;

	try
	{
		config = AppConfig::loadConfig();
	}
	catch (const std::ios_base::failure&)
	{
		std::cerr << "WARNING: Could not open configuration file \"" << AppConfig::DEFAULT_CONFIG_PATH << "\"." << std::endl;
	}
	
	WriterReadersLock<AppConfig> wrLock(&config);
	
	std::vector<std::string> options = {
			"document_root", STATIC_PATH.generic_string(),
			"listening_ports", std::to_string(PORT)
	};

	// CivetCallbacks serverCallbacks;
	// serverCallbacks.begin_request

	wxEntryStart(argc, argv);
	QuickOpenApplication& appGUI = wxGetApp();
	appGUI.setConfigRef(wrLock);
	appGUI.CallOnInit();
	
	CivetServer server(options);
	TestHandler testHandler;
	StaticHandler staticHandler("/");
	OpenWebpageAPIEndpoint webpageAPIEndpoint(wrLock, appGUI.getTrayWindow());
	FileConsentTokenService fileConsentTokenService(wrLock);
	OpenSaveFileAPIEndpoint fileAPIEndpoint(fileConsentTokenService, appGUI.getTrayWindow());
	
	server.addHandler("/test", testHandler);
	server.addHandler("/", staticHandler);
	server.addHandler("/api/openWebpage", webpageAPIEndpoint);
	server.addHandler("/api/openSaveFile", fileAPIEndpoint);
	server.addHandler("/api/openSaveFile/getConsent", fileConsentTokenService);

	// appGUI.getTrayWindow()->addWebpageOpenedActivity(wxT("foo.com"));
	// appGUI.getTrayWindow()->addWebpageOpenedActivity(wxT("bar.com"));
	appGUI.OnRun();

	wxEntryCleanup();
	mg_exit_library();
}