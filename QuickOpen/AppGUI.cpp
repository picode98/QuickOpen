#include "WebServer.h"

#include <wx/cmdline.h>

#include "AppGUI.h"

wxIMPLEMENT_APP(QuickOpenApplication);

wxBoxSizer* makeLabeledSizer(wxWindow* control, const wxString& labelText, wxWindow* parent, int spacing)
{
	auto sizer = new wxBoxSizer(wxHORIZONTAL);
	auto label = new wxStaticText(parent, wxID_ANY, labelText);
	sizer->Add(label, wxSizerFlags(0).Align(wxRIGHT).CenterVertical().Border(wxRIGHT, spacing));
	sizer->Add(control, wxSizerFlags(1).Expand());

	return sizer;
}

wxFileName FilePathValidator::getControlValue() const
{
	auto* textCtrl = dynamic_cast<wxTextEntryBase*>(m_validatorWindow);

	if (textCtrl != nullptr)
	{
		if (dirPath)
		{
			wxFileName parsedPath;
			parsedPath.AssignDir(textCtrl->GetValue());
			return parsedPath;
		}
		else
		{
			return wxFileName(textCtrl->GetValue());
		}
	}


	auto* pickerCtrl = dynamic_cast<wxFilePickerCtrl*>(m_validatorWindow);

	if (pickerCtrl != nullptr && !dirPath)
	{
		return pickerCtrl->GetFileName();
	}

	auto* dirPickerCtrl = dynamic_cast<wxDirPickerCtrl*>(m_validatorWindow);

	if (dirPickerCtrl != nullptr && dirPath)
	{
		return dirPickerCtrl->GetDirName();
	}

	throw std::bad_cast();
}

void FilePathValidator::setControlValue(const wxFileName& path)
{
	auto* textCtrl = dynamic_cast<wxTextEntryBase*>(m_validatorWindow);

	if (textCtrl != nullptr)
	{
		textCtrl->SetValue(fileName.GetFullPath());
		return;
	}

	auto* pickerCtrl = dynamic_cast<wxFilePickerCtrl*>(m_validatorWindow);

	if (pickerCtrl != nullptr && !dirPath)
	{
		pickerCtrl->SetFileName(path);
		return;
	}

	auto* dirPickerCtrl = dynamic_cast<wxDirPickerCtrl*>(m_validatorWindow);

	if (dirPickerCtrl != nullptr && dirPath)
	{
		dirPickerCtrl->SetDirName(path);
		return;
	}

	throw std::bad_cast();
}

bool FilePathValidator::Validate(wxWindow* parent)
{
	auto value = getControlValue();
	std::cout << "Validating " << value.GetFullPath() << std::endl;
	// bool isValid = value.IsOk() && (!value.GetFullPath().IsEmpty()) && (!absolutePath || value.IsAbsolute());

	if(!m_validatorWindow->IsEnabled())
	{
		return true;
	}

	if (value.IsOk() && (!value.GetFullPath().IsEmpty()))
	{
		if (absolutePath && !value.IsAbsolute())
		{
			auto* toolTip = new wxRichToolTip(wxT("Invalid path"), wxT("Please enter an absolute path."));
			toolTip->SetIcon(wxICON_ERROR);
			toolTip->ShowFor(m_validatorWindow);
			return false;
		}
		else
		{
			return true;
		}
	}
	else
	{
		auto* toolTip = new wxRichToolTip(wxT("Invalid path"), wxT("Please enter a valid path."));
		toolTip->SetIcon(wxICON_ERROR);
		toolTip->ShowFor(m_validatorWindow);
		return false;
	}
}

QuickOpenSettings::
QuickOpenSettings(WriterReadersLock<AppConfig>& configRef): wxFrame(nullptr, wxID_ANY, wxT("Settings")),
                                                            configRef(configRef)
{
	this->SetIcon(wxIcon(getAppIconPath().GetFullPath(), wxBITMAP_TYPE_ICO));
	WriterReadersLock<AppConfig>::ReadableReference config(configRef);

#ifdef PLATFORM_STARTUP_ENTRY_SUPPORTED
	StartupEntryState startupState = getStartupEntryState();

	if(startupState == StartupEntryState::DIFFERENT_APPLICATION)
	{
		int result = wxMessageBox(wxT("The startup entry for QuickOpen points to a different application.\n")
			wxT("This may indicate a corrupted installation. Would you like to reset it?"), wxT("Startup Entry"), wxICON_WARNING | wxYES_NO, this);
		if(result == wxYES)
		{
			addUserStartupEntry();
			startupState = getStartupEntryState();
		}
	}
#endif


	wxPanel* topLevelPanel = new wxPanel(this);
	wxBoxSizer* panelSizer = new wxBoxSizer(wxVERTICAL);
	panelSizer->Add(topLevelPanel, wxSizerFlags(1).Expand());

	wxBoxSizer* topLevelSizer = new wxBoxSizer(wxVERTICAL);
	// this->panel = new wxPanel(this);
	// topLevelSizer->Add(panel, wxSizerFlags(1).Expand());

	// wxBoxSizer* panelSizer = new wxBoxSizer(wxHORIZONTAL);

	// this->systemGroup = new wxStaticBox(this, wxID_ANY, wxT("System"));
	// panelSizer->Add(systemGroup, wxSizerFlags(1).Expand());
	// panel->SetSizerAndFit(panelSizer);

	this->systemGroup = new wxStaticBox(topLevelPanel, wxID_ANY, wxT("System"));
	//(new wxBoxSizer(wxVERTICAL))->Add(systemGroup);
	// (new wxBoxSizer(wxVERTICAL))->Add(panel);
	// test->Add(systemGroup);
	// this->systemGroup->SetPosition({ 100, 50 });
	this->systemGroupSizer = new wxStaticBoxSizer(this->systemGroup, wxVERTICAL);
	topLevelSizer->Add(systemGroupSizer, wxSizerFlags(0).Expand());

	topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	this->runAtStartupCheckbox = new wxCheckBox(topLevelPanel, wxID_ANY, wxT("Run QuickOpen at startup"));
	this->systemGroupSizer->Add(runAtStartupCheckbox);

#ifdef PLATFORM_STARTUP_ENTRY_SUPPORTED
	if(InstallationInfo::detectInstallation().installType == InstallationInfo::INSTALLED_SYSTEM)
	{
		this->runAtStartupCheckbox->SetValue(startupState == StartupEntryState::PRESENT);
	}
	else
	{
		this->runAtStartupCheckbox->Disable();
		auto userInfoText = new AutoWrappingStaticText(topLevelPanel, wxID_ANY, wxT("To avoid misconfiguration, system settings are only")
			wxT(" available for installed instances of QuickOpen."));
		userInfoText->SetForegroundColour(ERROR_TEXT_COLOR);
		this->systemGroupSizer->Add(userInfoText, wxSizerFlags(0).Expand());
	}
#else
	this->runAtStartupCheckbox->Disable();
	auto userInfoText = new AutoWrappingStaticText(topLevelPanel, wxID_ANY, wxT("Feature not currently supported on this platform"));
	userInfoText->SetForegroundColour(ERROR_TEXT_COLOR);
	this->systemGroupSizer->Add(userInfoText, wxSizerFlags(0).Expand());
#endif

	this->webpageOpenGroup = new wxStaticBox(topLevelPanel, wxID_ANY, wxT("Opening Webpages"));
	this->webpageOpenGroupSizer = new wxStaticBoxSizer(this->webpageOpenGroup, wxVERTICAL);
	topLevelSizer->Add(webpageOpenGroupSizer, wxSizerFlags(0).Expand());

	auto browserChoices = wxArrayString();
	browserChoices.Add(wxT("System default web browser"));
	// browserChoices.Add(wxT("foo"));

	this->installedBrowsers = getInstalledWebBrowsers();

	this->browserSelInstalledListStartIndex = browserChoices.size();
	for (const auto& thisBrowser : installedBrowsers)
	{
		browserChoices.Add(thisBrowser.browserName);
	}

	browserSelCustomBrowserIndex = browserChoices.size();
	browserChoices.Add(wxT("Custom command..."));

	this->browserSelection = new wxChoice(topLevelPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, browserChoices);
	this->browserSelection->Bind(wxEVT_CHOICE, &QuickOpenSettings::OnBrowserSelectionChanged, this);

	if (config->browserID.empty())
	{
		if (config->customBrowserPath.empty())
		{
			this->browserSelection->SetSelection(0);
		}
		else
		{
			this->browserSelection->SetSelection(static_cast<int>(browserChoices.size()) - 1);
		}
	}
	else
	{
		auto browserIndex = this->installedBrowsers.end();

		for (auto thisBrowser = this->installedBrowsers.begin(); thisBrowser != this->installedBrowsers.end(); ++
		     thisBrowser)
		{
			if (thisBrowser->browserID == config->browserID)
			{
				browserIndex = thisBrowser;
				break;
			}
		}

		if (browserIndex != this->installedBrowsers.end())
		{
			this->browserSelection->SetSelection(
				browserIndex - this->installedBrowsers.begin() + this->browserSelInstalledListStartIndex);
		}
		else
		{
			std::cerr << "WARNING: Browser ID \"" << config->browserID << "\" is not valid." << std::endl;
		}
	}

	webpageOpenGroupSizer->Add(makeLabeledSizer(browserSelection, wxT("Web browser for opening webpages:"), topLevelPanel));
	// panelSizer->Add(runAtStartupCheckbox, wxSizerFlags(1).Expand());
	//topLevelSizer->Add(runAtStartupCheckbox, 1);

	this->customBrowserCommandText = new wxTextCtrl(topLevelPanel, wxID_ANY);
	customBrowserCommandText->SetValue(config->customBrowserPath);
	this->customBrowserFileBrowseButton = new wxButton(topLevelPanel, wxID_ANY, wxT("Browse..."));
	customBrowserFileBrowseButton->Bind(wxEVT_BUTTON, &QuickOpenSettings::OnCustomBrowserBrowseButtonClicked, this);
	this->customBrowserSizer = new wxBoxSizer(wxHORIZONTAL);
	customBrowserSizer->Add(customBrowserCommandText, wxSizerFlags(1).Expand());
	customBrowserSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
	customBrowserSizer->Add(customBrowserFileBrowseButton);
	webpageOpenGroupSizer->Add(customBrowserSizer, wxSizerFlags(0).Expand().Border(wxTOP, DEFAULT_CONTROL_SPACING));

	topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	this->fileOpenSaveGroup = new wxStaticBox(topLevelPanel, wxID_ANY, wxT("Opening/Saving Files"));
	this->fileOpenSaveGroupSizer = new wxStaticBoxSizer(fileOpenSaveGroup, wxVERTICAL);
	topLevelSizer->Add(fileOpenSaveGroupSizer, wxSizerFlags(0).Expand());

	fileOpenSaveGroupSizer->Add(
		makeLabeledSizer(serverPortCtrl = new wxSpinCtrl(topLevelPanel, wxID_ANY,
									(config->serverPort.isSet() ? (wxString() << config->serverPort) : wxEmptyString), wxDefaultPosition,
		                                wxDefaultSize, wxSP_ARROW_KEYS, 0, 65535),
		                 wxString(wxT("Server port (default: ")) << decltype(config->serverPort)::DEFAULT_VALUE << wxT(")"), topLevelPanel),
		wxSizerFlags(0).Expand());

	fileOpenSaveGroupSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	fileOpenSaveGroupSizer->Add(
		makeLabeledSizer(
			saveFolderPicker = new wxDirPickerCtrl(topLevelPanel, wxID_ANY, wxEmptyString, wxT("Select Download Folder")),
			wxT("Folder for downloads:"), topLevelPanel),
		wxSizerFlags(0).Expand());
	// saveFolderPicker->SetDirName(config->fileSavePath);
	saveFolderPicker->SetValidator(FilePathValidator(config->fileSavePath, true));

	fileOpenSaveGroupSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	fileOpenSaveGroupSizer->Add(
		saveUseLastFolderCheckbox = new wxCheckBox(topLevelPanel, wxID_ANY, wxT("Use the last folder selected")));
	saveUseLastFolderCheckbox->Bind(wxEVT_CHECKBOX, &QuickOpenSettings::OnSaveUseLastFolderCheckboxChecked, this);
	saveUseLastFolderCheckbox->SetValue(config->saveUseLastFolder);

	topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	this->saveButton = new wxButton(topLevelPanel, wxID_OK);
	this->cancelButton = new wxButton(topLevelPanel, wxID_CANCEL);
	this->saveButton->Bind(wxEVT_BUTTON, &QuickOpenSettings::OnSaveButton, this);
	this->cancelButton->Bind(wxEVT_BUTTON, &QuickOpenSettings::OnCancelButton, this);
	auto bottomButtonSizer = new wxBoxSizer(wxHORIZONTAL);
	bottomButtonSizer->AddStretchSpacer(1);
	bottomButtonSizer->Add(saveButton, wxSizerFlags(0));
	bottomButtonSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
	bottomButtonSizer->Add(cancelButton, wxSizerFlags(0));

	topLevelSizer->AddStretchSpacer(1);
	topLevelSizer->Add(bottomButtonSizer, wxSizerFlags(0).Expand());

	this->TransferDataToWindow();
	setSizerWithPadding(topLevelPanel, topLevelSizer);
	topLevelPanel->Fit();
	this->SetSizerAndFit(panelSizer);
	this->updateCustomBrowserHidden();
	this->updateSaveFolderEnabledState();
	//this->runAtStartupCheckbox->SetSize(300, 50);
}

void QuickOpenSettings::OnSaveButton(wxCommandEvent& event)
{
	if (this->Validate() && this->TransferDataFromWindow())
	{
		WriterReadersLock<AppConfig>::WritableReference config(this->configRef);

		std::cout << "Saving settings." << std::endl;

#ifdef PLATFORM_STARTUP_ENTRY_SUPPORTED
		// Apply system integration changes if this is a system-wide installation
		if(InstallationInfo::detectInstallation().installType == InstallationInfo::INSTALLED_SYSTEM)
		{
			if (runAtStartupCheckbox->IsChecked())
			{
				addUserStartupEntry();
			}
			else if (getStartupEntryState() == StartupEntryState::PRESENT)
			{
				removeUserStartupEntry();
			}
		}
#endif

		// config->runAtStartup = runAtStartupCheckbox->IsChecked();

		int selectionIndex = this->browserSelection->GetSelection();

		bool selectionIsInstalledBrowser = selectionIndex >= this->browserSelInstalledListStartIndex
			&& selectionIndex < this->browserSelInstalledListStartIndex + this->installedBrowsers.size();

		auto oldServerPort = config->serverPort;
		if(this->serverPortCtrl->GetValue() == 0)
		{
			config->serverPort.reset();
		}
		else
		{
			config->serverPort = this->serverPortCtrl->GetValue();
		}

		if(oldServerPort.effectiveValue() != config->serverPort.effectiveValue())
		{
			wxGetApp().setupServer(config->serverPort);
		}

		config->browserID = selectionIsInstalledBrowser
			                    ? this->installedBrowsers[selectionIndex - this->browserSelInstalledListStartIndex].
			                    browserID
			                    : "";

		config->customBrowserPath = (selectionIndex == this->browserSelCustomBrowserIndex)
			                            ? this->customBrowserCommandText->GetValue().ToUTF8()
			                            : ""; // TODO

		config->fileSavePath = dynamic_cast<FilePathValidator*>(this->saveFolderPicker->GetValidator())->fileName;
		config->saveUseLastFolder = this->saveUseLastFolderCheckbox->IsChecked();

		bool saveSuccessful = false;
		try
		{
			config->saveConfig();
			saveSuccessful = true;
		}
		catch (const std::exception& ex)
		{
			wxMessageBox(wxT("Could not save the configuration file:\n") + wxString::FromUTF8(ex.what()), wxT("Configuration Save Error"), wxICON_ERROR | wxOK, this);
		}

		if(saveSuccessful)
		{
			this->Close();
		}
	}
}

void QuickOpenSettings::OnCancelButton(wxCommandEvent& event)
{
	this->Close();
}

void QuickOpenSettings::updateCustomBrowserHidden()
{
	if (browserSelection->GetSelection() == this->browserSelCustomBrowserIndex)
	{
		this->webpageOpenGroupSizer->Show(this->customBrowserSizer);
	}
	else
	{
		this->webpageOpenGroupSizer->Hide(this->customBrowserSizer);
	}

	this->Layout();
}

void QuickOpenSettings::OnBrowserSelectionChanged(wxCommandEvent& event)
{
	this->updateCustomBrowserHidden();
}

void QuickOpenSettings::OnCustomBrowserBrowseButtonClicked(wxCommandEvent& event)
{
	auto dialog = wxFileDialog(this, wxT("Select Browser Executable"), wxEmptyString, wxEmptyString,
	                           wxT("Browser Executables (*.exe)|*.exe"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if (dialog.ShowModal() != wxID_CANCEL)
	{
		this->customBrowserCommandText->SetValue(wxT("\"") + dialog.GetPath() + wxT("\" \"$url\""));
	}
}

void QuickOpenSettings::OnSaveUseLastFolderCheckboxChecked(wxCommandEvent& event)
{
	updateSaveFolderEnabledState();
}

void QuickOpenSettings::updateSaveFolderEnabledState()
{
	if (saveUseLastFolderCheckbox->GetValue())
	{
		saveFolderPicker->Disable();
	}
	else
	{
		saveFolderPicker->Enable();
	}
}

ConsentDialog::ConsentDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxString& requesterName): wxDialog(parent, id, title, wxDefaultPosition, wxDefaultSize,
                                                                                                                              wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	this->SetIcon(wxIcon(getAppIconPath().GetFullPath()));

	topLevelSizer = new wxBoxSizer(wxVERTICAL);
	topLevelSizer->Add(content = new wxWindow(this, wxID_ANY), wxSizerFlags(1).Expand());

	topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	topLevelSizer->Add(denyFutureRequestsCheckbox = new wxCheckBox(this, wxID_ANY, wxT("Decline future requests from ") + requesterName + wxT(" (until QuickOpen is restarted)")),
	                   wxSizerFlags(0).Expand());
	denyFutureRequestsCheckbox->Bind(wxEVT_CHECKBOX, &ConsentDialog::OnDenyFutureRequestsCheckboxChecked, this);

	auto* bottomButtonSizer = new wxBoxSizer(wxHORIZONTAL);

	bottomButtonSizer->AddStretchSpacer(1);
	bottomButtonSizer->Add(acceptButton = new wxButton(this, wxID_ANY, wxT("Accept")));
	bottomButtonSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
	bottomButtonSizer->Add(rejectButton = new wxButton(this, wxID_ANY, wxT("Decline")));

	topLevelSizer->Add(bottomButtonSizer, wxSizerFlags(0).Expand());

	acceptButton->Bind(wxEVT_BUTTON, &ConsentDialog::OnAcceptClicked, this);
	rejectButton->Bind(wxEVT_BUTTON, &ConsentDialog::OnDeclineClicked, this);

	setSizerWithPadding(this, topLevelSizer);
	this->Fit();
}

WebpageOpenConsentDialog::WebpageOpenConsentDialog(const wxString& URL, const wxString& requesterName):
	ConsentDialog(nullptr, wxID_ANY, wxT("Webpage Open Request"), requesterName)
{
	wxWindow* contentWindow = new wxWindow(this, wxID_ANY);
	wxBoxSizer* contentSizer = new wxBoxSizer(wxVERTICAL);
	contentSizer->Add(headerText = new wxStaticText(contentWindow, wxID_ANY,
	                                                wxString() << wxT("A user at " << requesterName << wxT(" is requesting that you open the following URL:\n\n") << URL
		                                                << wxT("\n\nWould you like to proceed?"))), wxSizerFlags(0).Expand());

	contentWindow->SetSizerAndFit(contentSizer);
	this->setContent(contentWindow);
}

void FileOpenSaveConsentDialog::OnAcceptClicked(wxCommandEvent& event)
{
	if (this->Validate() && this->TransferDataFromWindow())
	{
		bool useLastPath = false;
		{
			WriterReadersLock<AppConfig>::ReadableReference config(*configRef);
			useLastPath = config->saveUseLastFolder;
		}

		if(useLastPath)
		{
			wxFileName newLastPath = (this->multiFileLayout ? getValidator<FilePathValidator>(destFolderNameInput).fileName
															: getDirName(getValidator<FilePathValidator>(destFilenameInput).fileName));

			WriterReadersLock<AppConfig>::WritableReference config(*configRef);
			config->fileSavePath = newLastPath;

			try
			{
				config->saveConfig();
			}
			catch (const std::exception& ex)
			{
				std::cerr << "WARNING: Could not save last download folder: " << ex.what() << std::endl;
			}
		}

		ConsentDialog::OnAcceptClicked(event);
	}
	else
	{
		std::cout << "Validation failed." << std::endl;
	}
}

FileOpenSaveConsentDialog::FileOpenSaveConsentDialog(const wxFileName& defaultDestinationFolder, const FileConsentRequestInfo& requestInfo,
	std::shared_ptr<WriterReadersLock<AppConfig>> configRef, const wxString& requesterName) :
	ConsentDialog(nullptr, wxID_ANY, wxT("Receiving File"), requesterName), requestInfo(std::make_unique<FileConsentRequestInfo>(requestInfo)),
	configRef(configRef), defaultDestinationFolder(defaultDestinationFolder)
{
	wxWindow* contentWindow = new wxWindow(this, wxID_ANY);
	wxBoxSizer* contentSizer = new wxBoxSizer(wxVERTICAL);

	contentSizer->Add(new wxStaticText(contentWindow, wxID_ANY,
		wxString() << wxT("A user is sending the following ")
		<< (requestInfo.fileList.size() > 1 ? (wxString() << requestInfo.fileList.size() << wxT(" files:")) : wxT("file:"))/* << defaultDestination.
		GetFullName() << wxT("\" (") << wxFileName::GetHumanReadableSize(fileSize) <<
		wxT(").\n")
		<< wxT("Would you like to accept it?")*/
	));
	contentSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	auto itemListPanel = new wxScrolled<wxPanel>(contentWindow, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(400, 200)));
	auto itemListSizer = new wxBoxSizer(wxVERTICAL);
	contentSizer->Add(itemListPanel, wxSizerFlags(1).Expand());
	for (const auto& thisFile : requestInfo.fileList)
	{
		itemListSizer->Add(new wxStaticText(itemListPanel, wxID_ANY, wxString(wxT('\"')) << thisFile.filename << wxT("\" (")
			<< wxFileName::GetHumanReadableSize(wxULongLong(thisFile.fileSize)) << wxT(")")));
		itemListSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
	}
	itemListPanel->SetSizer(itemListSizer);
	itemListPanel->FitInside();
	itemListPanel->SetScrollRate(5, 5);

	contentSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	if (requestInfo.fileList.size() == 1)
	{
		wxFileName defaultDestination(defaultDestinationFolder);
		defaultDestination.SetFullName(requestInfo.fileList[0].filename);

		contentSizer->Add(makeLabeledSizer(destFilenameInput =
			new wxFilePickerCtrl(contentWindow, wxID_ANY, wxEmptyString,
				wxT("Select Save Destination"),
				wxString() << wxT(".") << defaultDestination.GetExt() <<
				wxT(" files|*.") << defaultDestination.GetExt() << wxT(
					"|All files|*.*"),
				wxDefaultPosition, wxDefaultSize,
				wxFLP_SAVE | wxFLP_USE_TEXTCTRL | wxFLP_OVERWRITE_PROMPT),
			wxT("Save this file to:"), contentWindow), wxSizerFlags(0).Expand());

		destFilenameInput->SetValidator(FilePathValidator(defaultDestination));
	}
	else
	{
		multiFileLayout = true;

		contentSizer->Add(makeLabeledSizer(
			destFolderNameInput = new wxDirPickerCtrl(contentWindow, wxID_ANY, defaultDestinationFolder.GetFullPath(), wxT("Select Save Destination Folder")),
			wxT("Save files to:"), contentWindow), wxSizerFlags(0).Expand());

		destFolderNameInput->SetValidator(FilePathValidator(defaultDestinationFolder, true));
	}

	contentWindow->SetSizerAndFit(contentSizer);
	contentWindow->TransferDataToWindow();
	this->setContent(contentWindow);
}

std::vector<wxFileName> FileOpenSaveConsentDialog::getConsentedFilenames() const
{
	if(multiFileLayout)
	{
		std::vector<wxFileName> fileNames;
		wxFileName destFolder = dynamic_cast<FilePathValidator*>(destFolderNameInput->GetValidator())->fileName;
		for(const auto& thisFile : requestInfo->fileList)
		{
			fileNames.push_back(destFolder);
			fileNames.back().SetFullName(thisFile.filename);
		}

		return fileNames;
	}
	else
	{
		return { dynamic_cast<FilePathValidator*>(destFilenameInput->GetValidator())->fileName };
	}
}

QuickOpenTaskbarIcon::TaskbarMenu::TaskbarMenu(WriterReadersLock<AppConfig>& configRef, TrayStatusWindow* statusWindow): configRef(configRef),
	statusWindow(statusWindow)
{
	this->Append(STATUS, wxT("Open Status"));
	this->Append(CLIENT_PAGE, wxT("Open Client Webpage"));
	this->Append(SETTINGS, wxT("Open Settings"));
	this->AppendSeparator();
	this->Append(ABOUT, wxT("About QuickOpen"));
	this->Append(EXIT, wxT("Exit QuickOpen"));
}

void QuickOpenTaskbarIcon::TaskbarMenu::OnClientPageItemSelected(wxCommandEvent& event)
{
	unsigned currentPort = WriterReadersLock<AppConfig>::ReadableReference(configRef)->serverPort;
	openURL(std::string("http://localhost") + (currentPort != 80 ? ":" + std::to_string(currentPort) : ""));
}

void QuickOpenTaskbarIcon::TaskbarMenu::OnAboutItemSelected(wxCommandEvent& event)
{
	wxString depStr = wxT("with additional credit to developers of the following dependencies:\n");

	auto depVersions = getDependencyVersions();
	for(auto item = depVersions.begin(); item != depVersions.end(); ++item)
	{
		auto nextItem = std::next(item);

		depStr += item->first + wxT(" (version ") + item->second + wxT(")");
				
		if(nextItem != depVersions.end())
		{
			if (std::next(nextItem) == depVersions.end())
			{
				depStr += wxT(", and ");
			}
			else
			{
				depStr += wxT(", ");
			}
		}
	}

	wxAboutDialogInfo aboutDialogInfo;
	aboutDialogInfo.AddDeveloper(wxT(QUICKOPEN_DEVELOPER));
	aboutDialogInfo.AddDeveloper(depStr);
	// aboutDialogInfo.AddDeveloper(wxT(""))
	aboutDialogInfo.SetVersion(wxString(wxT("Version ")) + wxT(QUICKOPEN_VERSION_STR));
	aboutDialogInfo.SetDescription(wxT(QUICKOPEN_SHORT_DESC));
	aboutDialogInfo.SetCopyright(wxT(QUICKOPEN_LICENSE));
	aboutDialogInfo.SetWebSite(wxT(QUICKOPEN_REPO_URL));
	aboutDialogInfo.SetIcon(wxIcon(getAppIconPath().GetFullPath(), wxBITMAP_TYPE_ICO));
	// aboutDialogInfo.GetDescriptionAndCredits()

	wxAboutBox(aboutDialogInfo);
}

const std::vector<wxCmdLineEntryDesc> QuickOpenApplication::COMMAND_LINE_DESC = {
	{ wxCMD_LINE_OPTION, nullptr, "set-default-upload-folder", "Updates the default upload folder", wxCMD_LINE_VAL_STRING, 0 },
	{ wxCMD_LINE_NONE }
};

void QuickOpenApplication::triggerConfigUpdate()
{
	unsigned newPort;
	{
		WriterReadersLock<AppConfig>::WritableReference configLock(*configRef);
		*configLock = AppConfig::loadConfig();
		newPort = configLock->serverPort;
	}

	if(newPort != server->getPort())
	{
		setupServer(newPort);
	}
}

bool QuickOpenApplication::OnInit()
{
	singleInstanceChecker = std::make_unique<wxSingleInstanceChecker>(wxT("Single Instance Checker"),
		(InstallationInfo::detectInstallation().configFolder / wxFileName(".", "run.lock")).GetFullPath());
	if (!wxApp::OnInit()) return false;
	// mainWindow = new MainWindow(*this);

	this->SetAppName(wxT("QuickOpen"));
	this->SetAppDisplayName(wxT("QuickOpen"));

	std::unique_ptr<AppConfig> config = std::make_unique<AppConfig>();

	try
	{
		*config = AppConfig::loadConfig();
	}
	catch (const std::ios_base::failure&)
	{
		std::cerr << "WARNING: Could not open configuration file \"" << AppConfig::defaultConfigPath().GetFullPath() << "\"." << std::endl;
	}

	if(cliNewDefaultUploadFolder.has_value())
	{
		config->fileSavePath = cliNewDefaultUploadFolder.value();
		config->saveConfig();
	}

	mg_init_library(0);

	if (singleInstanceChecker->IsAnotherRunning())
	{
		ManagementClient::sendReload();
		return false;
	}

	configRef = std::make_shared<WriterReadersLock<AppConfig>>(config);

	setupServer(configRef->obj->serverPort);

	unsigned attempts = 0;
	while (true)
	{
		try
		{
			mgmtServer = std::make_unique<ManagementServer>(*this, generateCryptoRandomInteger<uint16_t>() % 32768 + 32768);
			break;
		}
		catch (const CivetException& ex)
		{
			++attempts;

			if (std::string(ex.what()).find("binding to port") == std::string::npos)
			{
				throw;
			}
			else if(attempts >= 100)
			{
				throw std::runtime_error("Could not find free port to bind management server");
			}
		}
	}
		
	assert(configRef != nullptr);
	this->icon = new QuickOpenTaskbarIcon(*configRef);
	// this->settingsWindow->Show();
	return true;
}

void QuickOpenApplication::OnInitCmdLine(wxCmdLineParser& parser)
{
	wxApp::OnInitCmdLine(parser);

	parser.SetDesc(COMMAND_LINE_DESC.data());
}

bool QuickOpenApplication::OnCmdLineParsed(wxCmdLineParser& parser)
{
	wxApp::OnCmdLineParsed(parser);

	wxString uploadFolder;
	if (parser.Found(wxT("set-default-upload-folder"), &uploadFolder))
	{
		if (uploadFolder.EndsWith(">")) uploadFolder.RemoveLast(1); // Allow a trailing > to prevent issues with quoting file paths that end with
																			// a backslash (e.g. "C:\" escapes the ending quote).
		this->cliNewDefaultUploadFolder = wxFileName(uploadFolder, "");
	}

	return true;
}

void QuickOpenApplication::notifyUser(MessageSeverity severity, const wxString& title, const wxString& text)
{
	bool notificationShown = false;
#if wxUSE_TASKBARICON_BALLOONS
	notificationShown = this->icon->ShowBalloon(title, text, 0, wxICON_INFORMATION);
#endif

	if(!notificationShown)
	{
		auto notifyWindow = new NotificationWindow(nullptr, severity, text, title, [this]
		{
			this->getTrayWindow()->showAtCursor();
		});
		notifyWindow->Show();
		notifyWindow->RequestUserAttention();
	}
}

void QuickOpenApplication::setupServer(unsigned newPort)
{
	server = std::make_unique<QuickOpenWebServer>(configRef, *this, newPort);
}

bool QuickOpenApplication::promptForWebpageOpen(std::string URL, const wxString& requesterName, bool& banRequested)
{
	auto messageBoxLambda = [URL, &requesterName, &banRequested] {
		auto dialog = WebpageOpenConsentDialog(URL, requesterName);
		dialog.RequestUserAttention();
		auto result = static_cast<ConsentDialog::ResultCode>(dialog.ShowModal());
		banRequested = dialog.denyFutureRequestsRequested();
		return result;
		// return wxMessageBox(wxT("Do you want to open the following webpage?\n") + wxString::FromUTF8(URL), wxT("Webpage Open Request"), wxYES_NO);
	};

	auto response = wxCallAfterSync<QuickOpenApplication, decltype(messageBoxLambda), ConsentDialog::ResultCode>(*this, messageBoxLambda);
	return (response == ConsentDialog::ACCEPT);
}

std::pair<ConsentDialog::ResultCode, bool> QuickOpenApplication::promptForFileSave(const wxFileName& defaultDestDir,
	const wxString& requesterName, FileConsentRequestInfo& rqFileInfo)
{
	auto dlgLambda = [this, &defaultDestDir, &requesterName, &rqFileInfo]
	{
		auto consentDlg = FileOpenSaveConsentDialog(defaultDestDir, rqFileInfo, this->configRef, requesterName);
		consentDlg.Show();
		consentDlg.RequestUserAttention();
		auto resultVal = static_cast<FileOpenSaveConsentDialog::ResultCode>(consentDlg.ShowModal());
		std::vector<wxFileName> consentedFileNames = consentDlg.getConsentedFilenames();

		for (size_t i = 0; i < consentedFileNames.size(); ++i)
		{
			rqFileInfo.fileList[i].consentedFileName = consentedFileNames[i];
		}

		return std::pair{ resultVal, consentDlg.denyFutureRequestsRequested() };
	};

	if (wxIsMainThread())
	{
		return dlgLambda();
	}
	else
	{
		return wxCallAfterSync<decltype(*this), decltype(dlgLambda), std::pair<ConsentDialog::ResultCode, bool>>(*this, dlgLambda);
	}
}

int QuickOpenApplication::OnExit()
{
	if (this->icon != nullptr)
	{
		this->icon->RemoveIcon();
		this->icon->Destroy();
		this->icon = nullptr;
	}

	mg_exit_library();

	return 0;
}

wxBEGIN_EVENT_TABLE(QuickOpenTaskbarIcon::TaskbarMenu, wxMenu)
	EVT_MENU(QuickOpenTaskbarIcon::TaskbarMenu::MenuItems::STATUS, QuickOpenTaskbarIcon::TaskbarMenu::OnStatusItemSelected)
	EVT_MENU(QuickOpenTaskbarIcon::TaskbarMenu::MenuItems::CLIENT_PAGE, QuickOpenTaskbarIcon::TaskbarMenu::OnClientPageItemSelected)
	EVT_MENU(QuickOpenTaskbarIcon::TaskbarMenu::MenuItems::SETTINGS, QuickOpenTaskbarIcon::TaskbarMenu::OnSettingsItemSelected)
    EVT_MENU(QuickOpenTaskbarIcon::TaskbarMenu::MenuItems::ABOUT, QuickOpenTaskbarIcon::TaskbarMenu::OnAboutItemSelected)
	EVT_MENU(QuickOpenTaskbarIcon::TaskbarMenu::MenuItems::EXIT, QuickOpenTaskbarIcon::TaskbarMenu::OnExitItemSelected)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(NotificationWindow, wxGenericMessageDialog)
	EVT_BUTTON(wxID_YES, NotificationWindow::OnOKClicked)
	EVT_BUTTON(wxID_NO, NotificationWindow::OnMoreInfoClicked)
wxEND_EVENT_TABLE()