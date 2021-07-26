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
#include <iostream>

#ifdef _WIN32
#include "WinUtils.h"
#endif

const std::filesystem::path STATIC_PATH = std::filesystem::current_path() / "static";
const int PORT = 80;


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

public:
	OpenWebpageAPIEndpoint(WriterReadersLock<AppConfig>& configLock): configLock(configLock)
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
					
					std::cout << "Will open " << wxURL << std::endl;
					

					mg_send_http_ok(conn, "text/plain", 0);
				}
				else
				{
					mg_send_http_error(conn, 403, "Opening of the webpage was denied by the user.");
				}
			}
			else
			{
				mg_send_http_error(conn, 400, "Argument for parameter \"url\" is not a valid HTTP/HTTPS URL.");
			}
		}
		else
		{
			mg_send_http_error(conn, 400, "Argument for parameter \"url\" is missing from request.");
		}

		return true;
		// mg_read
		// auto systemShell = boost::process::shell();
		// boost::process::child(systemShell, "foo");
	}
};

class OpenSaveFileAPIEndpoint : public CivetHandler
{
	WriterReadersLock<AppConfig>& configLock;

	static int getFieldInfo(const char *key, const char *filename, char *path, size_t pathlen, void *user_data)
	{
		std::filesystem::path destPath("C:\\Users\\saama\\Downloads\\test.txt");
		strncpy_s(path, pathlen, WideStrToUTF8Str(destPath).c_str(), _TRUNCATE);
		return MG_FORM_FIELD_STORAGE_STORE;
	}

	static int onFileStore(const char *path, long long file_size, void *user_data)
	{
		std::cout << "Wrote " << file_size << " bytes of data to " << path << std::endl;
		return MG_FORM_FIELD_HANDLE_NEXT;
	}
	
public:
	OpenSaveFileAPIEndpoint(WriterReadersLock<AppConfig>& configLock) : configLock(configLock)
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

		mg_form_data_handler formDataHandler;
		formDataHandler.field_found = getFieldInfo;
		formDataHandler.field_get = nullptr;
		formDataHandler.field_store = onFileStore;
		formDataHandler.user_data = nullptr;

		mg_handle_form_request(conn, &formDataHandler);
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

int main(int argc, char** argv)
{
	AppConfig config = AppConfig::loadConfig();
	WriterReadersLock<AppConfig> wrLock(&config);
	
	std::vector<std::string> options = {
			"document_root", STATIC_PATH.generic_string(),
			"listening_ports", std::to_string(PORT)
	};

	CivetServer server(options);
	TestHandler testHandler;
	StaticHandler staticHandler("/");
	OpenWebpageAPIEndpoint webpageAPIEndpoint(wrLock);
	OpenSaveFileAPIEndpoint fileAPIEndpoint(wrLock);
	
	server.addHandler("/test", testHandler);
	server.addHandler("/", staticHandler);
	server.addHandler("/api/openWebpage", webpageAPIEndpoint);
	server.addHandler("/api/openSaveFile", fileAPIEndpoint);
	wxGetApp().setConfigRef(wrLock);
	wxEntry(argc, argv);

	mg_exit_library();
}