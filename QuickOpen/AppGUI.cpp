#include "AppGUI.h"

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
	WriterReadersLock<AppConfig>::ReadableReference config(configRef);

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
	topLevelSizer->Add(systemGroupSizer);

	topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	this->runAtStartupCheckbox = new wxCheckBox(topLevelPanel, wxID_ANY, wxT("Run QuickOpen at startup"));
	this->runAtStartupCheckbox->SetValue(config->runAtStartup);
	this->systemGroupSizer->Add(runAtStartupCheckbox);

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
		makeLabeledSizer(new wxSpinCtrl(topLevelPanel, wxID_ANY, (wxString() << config->maxSaveFileSize), wxDefaultPosition,
		                                wxDefaultSize, wxSP_ARROW_KEYS, 0, 1024),
		                 wxT("Maximum size for uploaded files:"), topLevelPanel),
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

		config->runAtStartup = runAtStartupCheckbox->IsChecked();

		int selectionIndex = this->browserSelection->GetSelection();

		bool selectionIsInstalledBrowser = selectionIndex >= this->browserSelInstalledListStartIndex
			&& selectionIndex < this->browserSelInstalledListStartIndex + this->installedBrowsers.size();

		config->browserID = selectionIsInstalledBrowser
			                    ? this->installedBrowsers[selectionIndex - this->browserSelInstalledListStartIndex].
			                    browserID
			                    : "";

		config->customBrowserPath = (selectionIndex == this->browserSelCustomBrowserIndex)
			                            ? this->customBrowserCommandText->GetValue().ToUTF8()
			                            : ""; // TODO

		config->fileSavePath = dynamic_cast<FilePathValidator*>(this->saveFolderPicker->GetValidator())->fileName;
		config->saveUseLastFolder = this->saveUseLastFolderCheckbox->IsChecked();

		config->saveConfig();

		this->Close();
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
			config->saveConfig();
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
	ConsentDialog(nullptr, wxID_ANY, wxT("Receiving File"), requesterName), requestInfo(requestInfo),
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
		for(const auto& thisFile : requestInfo.fileList)
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

wxBEGIN_EVENT_TABLE(QuickOpenTaskbarIcon::TaskbarMenu, wxMenu)
	EVT_MENU(QuickOpenTaskbarIcon::TaskbarMenu::MenuItems::SETTINGS, QuickOpenTaskbarIcon::TaskbarMenu::OnSettingsItemSelected)
	EVT_MENU(QuickOpenTaskbarIcon::TaskbarMenu::MenuItems::EXIT, QuickOpenTaskbarIcon::TaskbarMenu::OnExitItemSelected)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP(QuickOpenApplication);