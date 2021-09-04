#include <regex>

#include "WebServer.h"

#include "AppGUI.h"

std::string URLDecode(const std::string& encodedStr, bool decodePlus)
{
	std::ostringstream outputStream;

	for (size_t i = 0; i < encodedStr.size();)
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
		for (int i = 0; i < bytesRead; ++i)
		{
			switch (bodyBuffer[i])
			{
			case '&':
				readingValue = false;
				resultMap.insert({URLDecode(currentKey, true), URLDecode(currentValue, true)});
				currentKey = currentValue = "";
				break;
			case '=':
				readingValue = true;
				break;
			default:
				if (readingValue)
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

	resultMap.insert({URLDecode(currentKey, true), URLDecode(currentValue, true)});

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
			resultMap.insert({URLDecode(currentKey, false), URLDecode(currentValue, false)});
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

	resultMap.insert({URLDecode(currentKey, false), URLDecode(currentValue, false)});

	return resultMap;
}

void sendJSONResponse(mg_connection* conn, int status, const nlohmann::json& json)
{
	mg_response_header_start(conn, status);
	mg_response_header_add(conn, "Content-Type", "application/json", -1);
	mg_response_header_send(conn);
	std::string jsonString = json.dump();
	mg_write(conn, jsonString.c_str(), jsonString.size());
	mg_close_connection(conn);
}

bool StaticHandler::handleGet(CivetServer* server, mg_connection* conn)
{
	const mg_request_info* rqInfo = mg_get_request_info(conn);
	std::string_view rqURI(rqInfo->request_uri);

	if (rqURI.find("..") != std::string::npos)
	{
		mg_send_http_error(conn, 400, "The path requested contains invalid sequences (\"..\").");
		return true;
	}

	if (startsWith(rqURI, this->staticPrefix))
	{
		std::string_view truncPath = rqURI;
		truncPath.remove_prefix(this->staticPrefix.size());

		std::filesystem::path resolvedPath = (STATIC_PATH / truncPath).lexically_normal();
		if (!resolvedPath.has_extension())
		{
			if (!resolvedPath.has_filename())
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

bool OpenWebpageAPIEndpoint::handlePost(CivetServer* server, mg_connection* conn)
{
	auto postParams = parseFormEncodedBody(conn);

	if (postParams.count("url") > 0)
	{
		std::string& url = postParams["url"];
		if (std::regex_match(url, std::regex("^https?:(\\/\\/)?[a-z|\\d|-|\\.]+($|\\/\\S*$)",
			std::regex_constants::icase | std::regex_constants::ECMAScript)))
		{
			if (promptForWebpageOpen(wxAppRef, url))
			{
				wxString wxURL = wxString::FromUTF8(postParams["url"]);

				{
					WriterReadersLock<AppConfig>::ReadableReference readRef(*configLock);

					if (readRef->browserID.empty())
					{
						if (readRef->customBrowserPath.empty())
						{
							auto defaultBrowserCommand = substituteFormatString(
								getDefaultBrowserCommandLine(), wxUniChar('%'), { wxURL }, {});
							startSubprocess(defaultBrowserCommand);
						}
						else
						{
							auto browserCommand = substituteFormatString(
								readRef->customBrowserPath, wxUniChar('$'), {}, { {"url", wxURL} });
							startSubprocess(browserCommand);
						}
					}
					else
					{
						startSubprocess(getBrowserCommandLine(readRef->browserID) + " \"" + wxURL + "\"");
					}
				}

				wxAppRef.CallAfter([this, wxURL]
				{
					this->wxAppRef.getTrayWindow()->addWebpageOpenedActivity(wxURL);
				});

				std::cout << "Will open " << wxURL << std::endl;


				mg_send_http_ok(conn, "text/plain", 0);
			}
			else
			{
				auto jsonErrorInfo = nlohmann::json(FormErrorList{
					{
						{"", "Opening of the webpage was denied by the user."}
					}
					});
				sendJSONResponse(conn, 403, jsonErrorInfo);
			}
		}
		else
		{
			auto jsonErrorInfo = nlohmann::json(FormErrorList{
				{
					{"url", "This URL is not valid."}
				}
				});

			sendJSONResponse(conn, 400, jsonErrorInfo);
		}
	}
	else
	{
		auto jsonErrorInfo = nlohmann::json(FormErrorList{
			{
				{"url", "The URL is missing."}
			}
			});
		sendJSONResponse(conn, 400, jsonErrorInfo);
	}

	return true;
	// mg_read
	// auto systemShell = boost::process::shell();
	// boost::process::child(systemShell, "foo");
}

bool FileConsentTokenService::handlePost(CivetServer* server, mg_connection* conn)
{
	std::string bodyStr = MGReadAll(conn);
	auto rqFileInfo = FileConsentRequestInfo(nlohmann::json::parse(bodyStr));

	if(rqFileInfo.fileList.empty())
	{
		sendJSONResponse(conn, 400, FormErrorList {{
				{"fileList", "At least one file must be specified."}
		} });
		return true;
	}

	wxFileName defaultDestDir;
	{
		WriterReadersLock<AppConfig>::ReadableReference configRef(*configLock);
		defaultDestDir = configRef->fileSavePath;
	}

	//for (auto& thisFile : rqFileInfo.fileList)
	//{
	//	thisFile.suggestedFileName = defaultDestDir;
	//	thisFile.suggestedFileName.SetFullName(thisFile.filename);
	//}

	//std::optional<FileOpenSaveConsentDialog::ResultCode> resultCode;
	//std::condition_variable dialogResult;
	//std::mutex dialogResultMutex;
	//std::unique_lock<std::mutex> dialogResultLock(dialogResultMutex);

	auto consentDlgLambda = [this, &defaultDestDir, &rqFileInfo]
	{
		auto consentDlg = FileOpenSaveConsentDialog(defaultDestDir, rqFileInfo, this->configLock);
		consentDlg.Show();
		consentDlg.RequestUserAttention();
		auto resultVal = static_cast<FileOpenSaveConsentDialog::ResultCode>(consentDlg.ShowModal());
		std::vector<wxFileName> consentedFileNames = consentDlg.getConsentedFilenames();

		for(size_t i = 0; i < consentedFileNames.size(); ++i)
		{
			rqFileInfo.fileList[i].consentedFileName = consentedFileNames[i];
		}

		return resultVal;
	};

	FileOpenSaveConsentDialog::ResultCode result =
		wxCallAfterSync<QuickOpenApplication, decltype(consentDlgLambda), FileOpenSaveConsentDialog::ResultCode>(
			wxAppRef, consentDlgLambda);
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
		std::vector<ConsentToken> newTokens;
		for (const auto& thisFile : rqFileInfo.fileList)
		{
			// rqFileInfo.consentedFileName = consentDlg.getConsentedFilename();
			ConsentToken newToken = generateCryptoRandomInteger<ConsentToken>();
			newTokens.push_back(newToken);

			{
				WriterReadersLock<TokenMap>::WritableReference writeRef(tokenWRRef);
				writeRef->insert({ newToken, thisFile } );
			}
		}

		sendJSONResponse(conn, 200, nlohmann::json{ {"consentTokens", newTokens} });
	}
	else
	{
		auto jsonErrorInfo = nlohmann::json(FormErrorList{
			{
				{"uploadFile", "The user declined to receive the file."}
			}
		});
		sendJSONResponse(conn, 403, jsonErrorInfo);
	}

	return true;
}

void OpenSaveFileAPIEndpoint::MGStoreBodyChecked(mg_connection* conn, const wxFileName& fileName, unsigned long long targetFileSize,
	TrayStatusWindow::FileUploadActivityEntry* uploadActivityEntryRef, std::atomic<bool>& cancelRequestFlag)
{
	static const long long CHUNK_SIZE = 1LL << 20;
	unsigned long long bytesWritten = 0;
	auto bodyBuffer = std::make_unique<char[]>(CHUNK_SIZE);

	std::ofstream outFile;
	outFile.exceptions(std::ofstream::failbit);

	try
	{
#ifdef WIN32
		outFile.open(fileName.GetFullPath().ToStdWstring(), std::ofstream::binary);
#else
        outFile.open(fileName.GetFullPath(), std::ofstream::binary);
#endif

		int bytesRead;
		while ((bytesRead = mg_read(conn, bodyBuffer.get(), CHUNK_SIZE)) > 0)
		{
			bytesWritten += bytesRead;

			if (bytesWritten > targetFileSize)
			{
				throw IncorrectFileLengthException();
			}
			else if (cancelRequestFlag)
			{
				progressReportingApp.CallAfter([uploadActivityEntryRef]
				{
					uploadActivityEntryRef->setCancelCompleted();
				});

				throw OperationCanceledException();
			}

			outFile.write(bodyBuffer.get(), bytesRead);

			progressReportingApp.CallAfter([uploadActivityEntryRef, bytesWritten, targetFileSize]
			{
				uploadActivityEntryRef->setProgress((static_cast<double>(bytesWritten) / targetFileSize) * 100.0);
			});
		}

		progressReportingApp.CallAfter([uploadActivityEntryRef]
		{
			uploadActivityEntryRef->setCompleted(true);
		});
	}
	catch (const std::ios_base::failure& ex)
	{
#ifdef WIN32
		WindowsException detailedError = getWinAPIError(ERROR_SUCCESS);
#else
        const auto& detailedError = ex;
#endif
		progressReportingApp.CallAfter([uploadActivityEntryRef, detailedError]
		{
			uploadActivityEntryRef->setError(&detailedError);
		});

		throw detailedError;
	}

	// delete[] bodyBuffer;
	outFile.close();

	if (bytesWritten != targetFileSize)
	{
		throw IncorrectFileLengthException();
	}
}

bool OpenSaveFileAPIEndpoint::handlePost(CivetServer* server, mg_connection* conn)
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

	if (queryStringMap.count("consentToken") > 0)
	{
		ConsentToken parsedToken = atoll(queryStringMap["consentToken"].c_str());
		FileConsentRequestInfo::RequestedFileInfo consentedFileInfo;
		bool fileFound = false;

		{
			WriterReadersLock<FileConsentTokenService::TokenMap>::WritableReference
				tokens(consentServiceRef.tokenWRRef);

			if (tokens->count(parsedToken) > 0)
			{
				consentedFileInfo = tokens->at(parsedToken);
				fileFound = true;

				tokens->erase(parsedToken);
			}
		}

		if (fileFound)
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
			std::atomic<bool> cancelFlag = false;
			auto createActivity = [this, consentedFileInfo, &cancelFlag]
			{
				return this->progressReportingApp.getTrayWindow()->addFileUploadActivity(consentedFileInfo.consentedFileName, cancelFlag);
			};

			TrayStatusWindow::FileUploadActivityEntry* activityEntryRef =
				wxCallAfterSync<QuickOpenApplication, decltype(createActivity), TrayStatusWindow::
				                FileUploadActivityEntry*>(progressReportingApp, createActivity);

			try
			{
				MGStoreBodyChecked(conn, consentedFileInfo.consentedFileName, consentedFileInfo.fileSize,
				                   activityEntryRef, cancelFlag);
				mg_send_http_ok(conn, "text/plain", 0);
			}
			catch (const std::system_error& ex)
			{
				auto jsonErrorInfo = nlohmann::json(FormErrorList{
					{
						{
							"uploadFile",
							std::string("An error occurred while attempting to write the file: ") + ex.what()
						}
					}
				});
				sendJSONResponse(conn, 500, jsonErrorInfo);
			}
			catch (const IncorrectFileLengthException&)
			{
				auto jsonErrorInfo = nlohmann::json(FormErrorList{
					{
						{"uploadFile", "The file sent did not have the length specified by the consent token used."}
					}
				});
				sendJSONResponse(conn, 400, jsonErrorInfo);
			}
			catch (const OperationCanceledException&)
			{
				auto jsonErrorInfo = nlohmann::json(FormErrorList{
					{
						{"uploadFile", "The upload was canceled by the receiving user."}
					}
				});
				sendJSONResponse(conn, 500, jsonErrorInfo);
			}
		}
		else
		{
			// mg_send_http_error(conn, 403, "The consent token provided was not valid.");
			auto jsonErrorInfo = nlohmann::json(FormErrorList{
				{
					{"consentToken", "The consent token provided was not valid."}
				}
			});
			sendJSONResponse(conn, 403, jsonErrorInfo);
		}
	}
	else
	{
		// mg_send_http_error(conn, 401, "No consent token was provided.");
		auto jsonErrorInfo = nlohmann::json(FormErrorList{
			{
				{"consentToken", "No consent token was provided."}
			}
		});
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
