#include "WebServer.h"
#include "WebServerUtils.h"
#include "AppGUI.h"
#include "Utils.h"

#include <regex>

std::string StaticHandler::resolveServerExpression(mg_connection* conn, const std::string& expr)
{
	if(expr == "CSRF_TOKEN")
	{
		if(this->csrfHandler != nullptr)
		{
			return std::to_string(this->csrfHandler->addToken(mg_get_request_info(conn)->remote_addr));
		}
		else
		{
			return "ERROR: No CSRF handler specified";
		}
	}
	else
	{
		return "ERROR: Invalid server expression";
	}
}

void StaticHandler::sendProcessedPage(mg_connection* conn, const wxFileName& pagePath)
{
	const std::regex serverExpressionRegex("\\[\\[(.*)\\]\\]");

	try
	{
		std::string pageString = fileReadAll(pagePath);
		std::stringstream processedStream;

		auto it = std::sregex_iterator(pageString.begin(), pageString.end(), serverExpressionRegex);

		long long lastEndIndex = 0;
		for(; it != std::sregex_iterator(); ++it)
		{
			processedStream << std::string_view(pageString.c_str() + lastEndIndex, it->position() - lastEndIndex)
							<< resolveServerExpression(conn, (*it)[1]);

			lastEndIndex = it->position() + it->length();
		}

		processedStream << std::string_view(pageString.c_str() + lastEndIndex, pageString.size() - lastEndIndex);

		std::string processedStr = processedStream.str();
		mg_send_http_ok(conn, "text/html", processedStr.size());
		mg_write(conn, processedStr.c_str(), processedStr.size());
	}
	catch (const std::ios::failure&)
	{
		mg_send_http_error(conn, 404, "The requested file was not found.");
	}
}

bool StaticHandler::handleGet(CivetServer* server, mg_connection* conn)
{
	const mg_request_info* rqInfo = mg_get_request_info(conn);
	std::string rqURI(rqInfo->request_uri);

	if (rqURI.find("..") != std::string::npos)
	{
		mg_send_http_error(conn, 400, "The path requested contains invalid sequences (\"..\").");
		return true;
	}

	if (startsWith(rqURI, this->staticPrefix))
	{
		wxFileName resolvedPath = this->baseStaticPath / wxFileName(wxString::FromUTF8(rqURI.substr(this->staticPrefix.size())), wxPATH_UNIX);

		if (!resolvedPath.HasExt())
		{
			if (!resolvedPath.HasName())
			{
				resolvedPath.SetName("index");
			}

			resolvedPath.SetExt("html");
		}

		if(resolvedPath.GetExt() == "html")
		{
			this->sendProcessedPage(conn, resolvedPath);
		}
		else
		{
			mg_send_mime_file(conn, resolvedPath.GetFullPath().c_str(), nullptr);
		}
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
	wxString senderIP = mg_get_request_info(conn)->remote_addr;

	if (postParams.count("url") > 0)
	{
		std::string& url = postParams["url"];
		if (std::regex_match(url, std::regex("^https?:(\\/\\/)?[a-z|\\d|-|\\.]+($|\\/\\S*$)",
			std::regex_constants::icase | std::regex_constants::ECMAScript)))
		{
			bool openAllowed;
			{
				std::lock_guard<std::mutex> lock(consentDialogMutex);

				bool thisIPBanned = false, banRequested = false;
				{
					WriterReadersLock<std::set<wxString>>::ReadableReference ref(bannedIPRef);
					thisIPBanned = (ref->count(senderIP) > 0);
				}

				if (!thisIPBanned)
				{
					openAllowed = wxAppRef.promptForWebpageOpen(url, wxT("the IP address ") + senderIP, banRequested);
				}

				if(thisIPBanned || banRequested)
				{
					if(banRequested)
					{
						WriterReadersLock<std::set<wxString>>::WritableReference ref(bannedIPRef);

						if (ref->count(senderIP) == 0)
						{
							ref->insert(senderIP);
						}
					}

					auto jsonErrorInfo = nlohmann::json(FormErrorList{
					{
						{"", "This IP address is banned from sending or opening further content."}
					}
						});
					sendJSONResponse(conn, 403, jsonErrorInfo);

					return true;
				}
			}

			if (openAllowed)
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
				mg_close_connection(conn);
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

	wxString remoteIP = mg_get_request_info(conn)->remote_addr;

	FileOpenSaveConsentDialog::ResultCode result;
	bool denyFuture = false;
	{
		std::lock_guard<std::mutex> lock(consentDialogMutex);

		bool thisIPBanned = false;
		{
			WriterReadersLock<std::set<wxString>>::ReadableReference ref(bannedIPRef);
			thisIPBanned = (ref->count(remoteIP) > 0);
		}

		if (!thisIPBanned)
		{
			std::tie(result, denyFuture) = wxAppRef.promptForFileSave(defaultDestDir, wxT("the IP address ") + remoteIP, rqFileInfo);

			if(denyFuture)
			{
				WriterReadersLock<std::set<wxString>>::WritableReference ref(bannedIPRef);

				if(ref->count(remoteIP) == 0)
				{
					ref->insert(remoteIP);
				}
			}
		}

		if (thisIPBanned || denyFuture)
		{
			auto jsonErrorInfo = nlohmann::json(FormErrorList{
			{
				{"", "This IP address is banned from sending or opening further content."}
			}
				});
			sendJSONResponse(conn, 403, jsonErrorInfo);

			return true;
		}
	}
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
		ConsentToken newToken = generateCryptoRandomInteger<ConsentToken>();
		{
			WriterReadersLock<TokenMap>::WritableReference writeRef(tokenWRRef);
			writeRef->insert({ newToken, rqFileInfo.fileList });
		}

		sendJSONResponse(conn, 200, nlohmann::json{ {"consentToken", newToken } });
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
	}
	catch (const std::ios_base::failure& ex)
	{
#ifdef WIN32
		WindowsException detailedError = getWinAPIError(GetLastError());
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
	else
	{
		progressReportingApp.CallAfter([uploadActivityEntryRef]
		{
			uploadActivityEntryRef->setCompleted(true);
		});
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

	if (queryStringMap.count("consentToken") == 0)
	{
		// mg_send_http_error(conn, 401, "No consent token was provided.");
		auto jsonErrorInfo = nlohmann::json(FormErrorList{
			{
				{"consentToken", "No consent token was provided."}
			}
			});
		sendJSONResponse(conn, 401, jsonErrorInfo);

		return true;
	}
	ConsentToken parsedToken = atoll(queryStringMap["consentToken"].c_str());


	if (queryStringMap.count("fileIndex") == 0)
	{
		// mg_send_http_error(conn, 401, "No consent token was provided.");
		auto jsonErrorInfo = nlohmann::json(FormErrorList{
			{
				{"fileIndex", "No file index was provided."}
			}
			});
		sendJSONResponse(conn, 400, jsonErrorInfo);

		return true;
	}
	long long fileIndex = atoll(queryStringMap["fileIndex"].c_str());


	FileConsentRequestInfo::RequestedFileInfo consentedFileInfo;
	bool tokenValid = false, indexValid = false;

	{
		WriterReadersLock<FileConsentTokenService::TokenMap>::WritableReference
			tokens(consentServiceRef.tokenWRRef);

		if (tokens->count(parsedToken) > 0)
		{
			tokenValid = true;

			if (fileIndex >= 0 && fileIndex < tokens->at(parsedToken).size() && !tokens->at(parsedToken).at(fileIndex).uploadStarted)
			{
				indexValid = true;
				tokens->at(parsedToken).at(fileIndex).uploadStarted = true;
				consentedFileInfo = tokens->at(parsedToken).at(fileIndex);
			}
		}
	}

	if (!tokenValid)
	{
		// mg_send_http_error(conn, 403, "The consent token provided was not valid.");
		auto jsonErrorInfo = nlohmann::json(FormErrorList{
			{
				{"consentToken", "The consent token provided was not valid."}
			}
			});
		sendJSONResponse(conn, 403, jsonErrorInfo);
		return true;
	}

	if (!indexValid)
	{
		auto jsonErrorInfo = nlohmann::json(FormErrorList{
			{
				{"fileIndex", "The file index provided was not valid."}
			}
			});
		sendJSONResponse(conn, 403, jsonErrorInfo);
		return true;
	}

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
	catch (const IncorrectFileLengthException& ex)
	{
		progressReportingApp.CallAfter([activityEntryRef, ex] { activityEntryRef->setError(&ex); });
		auto jsonErrorInfo = nlohmann::json(FormErrorList{
			{
				{"uploadFile", "The file sent did not have the length specified by the consent token used."}
			}
		});
		sendJSONResponse(conn, 400, jsonErrorInfo);
	}
	catch (const OperationCanceledException& ex)
	{
		auto jsonErrorInfo = nlohmann::json(FormErrorList{
			{
				{"uploadFile", "The upload was canceled by the receiving user."}
			}
		});
		sendJSONResponse(conn, 500, jsonErrorInfo);
	}
	catch (const ConnectionClosedException& ex)
	{
		progressReportingApp.CallAfter([activityEntryRef, ex]{ activityEntryRef->setError(&ex); });
	}

	bool allEnded = true;
	size_t fileCount = 0;
	{
		WriterReadersLock<FileConsentTokenService::TokenMap>::WritableReference
			tokens(consentServiceRef.tokenWRRef);
		fileCount = tokens->at(parsedToken).size();

		tokens->at(parsedToken).at(fileIndex).uploadEnded = true;

		for(const FileConsentRequestInfo::RequestedFileInfo& thisFile : tokens->at(parsedToken))
		{
			if(!thisFile.uploadEnded)
			{
				allEnded = false;
				break;
			}
		}
	}

	if (allEnded)
	{
		QuickOpenApplication& appRef = this->progressReportingApp;
		this->progressReportingApp.CallAfter([&appRef, fileCount]
		{
			appRef.notifyUser(MessageSeverity::MSG_INFO, wxT("File Upload Completed"),
				wxString() << fileCount << (fileCount > 1 ? wxT(" files were ") : wxT(" file was "))
				<< wxT("uploaded."));
		});
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

QuickOpenWebServer::QuickOpenWebServer(const std::shared_ptr<WriterReadersLock<AppConfig>>& wrLock,
	QuickOpenApplication& wxAppRef, unsigned port):
	CivetServer({
		"document_root", STATIC_PATH.generic_string(),
		"listening_ports", '+' + std::to_string(port)
	}),
	wxAppRef(wxAppRef),
	wrLock(wrLock),
	staticHandler("/", &this->csrfHandler),
	webpageAPIEndpoint(wrLock, wxAppRef, consentDialogMutex, bannedIPs),
	fileConsentTokenService(wrLock, consentDialogMutex, wxAppRef, bannedIPs),
	fileAPIEndpoint(fileConsentTokenService, wxAppRef),
	bannedIPs(std::make_unique<std::set<wxString>>()),
	port(port)
{
	this->addHandler("/", staticHandler);
	this->addAuthHandler("/api/", this->csrfHandler);
	this->addHandler("/api/openWebpage", webpageAPIEndpoint);
	this->addHandler("/api/openSaveFile", fileAPIEndpoint);
	this->addHandler("/api/openSaveFile/getConsent", fileConsentTokenService);
}