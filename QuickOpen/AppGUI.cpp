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

	wxBoxSizer* windowPaddingSizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* topLevelSizer = new wxBoxSizer(wxVERTICAL);
	windowPaddingSizer->Add(topLevelSizer, wxSizerFlags(1).Expand().Border(wxALL, DEFAULT_CONTROL_SPACING));
	// this->panel = new wxPanel(this);
	// topLevelSizer->Add(panel, wxSizerFlags(1).Expand());

	// wxBoxSizer* panelSizer = new wxBoxSizer(wxHORIZONTAL);

	// this->systemGroup = new wxStaticBox(this, wxID_ANY, wxT("System"));
	// panelSizer->Add(systemGroup, wxSizerFlags(1).Expand());
	// panel->SetSizerAndFit(panelSizer);

	this->systemGroup = new wxStaticBox(this, wxID_ANY, wxT("System"));
	//(new wxBoxSizer(wxVERTICAL))->Add(systemGroup);
	// (new wxBoxSizer(wxVERTICAL))->Add(panel);
	// test->Add(systemGroup);
	// this->systemGroup->SetPosition({ 100, 50 });
	this->systemGroupSizer = new wxStaticBoxSizer(this->systemGroup, wxVERTICAL);
	topLevelSizer->Add(systemGroupSizer);

	topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	this->runAtStartupCheckbox = new wxCheckBox(this, wxID_ANY, wxT("Run QuickOpen at startup"));
	this->runAtStartupCheckbox->SetValue(config->runAtStartup);
	this->systemGroupSizer->Add(runAtStartupCheckbox);

	this->webpageOpenGroup = new wxStaticBox(this, wxID_ANY, wxT("Opening Webpages"));
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

	this->browserSelection = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, browserChoices);
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

	webpageOpenGroupSizer->Add(makeLabeledSizer(browserSelection, wxT("Web browser for opening webpages:"), this));
	// panelSizer->Add(runAtStartupCheckbox, wxSizerFlags(1).Expand());
	//topLevelSizer->Add(runAtStartupCheckbox, 1);

	this->customBrowserCommandText = new wxTextCtrl(this, wxID_ANY);
	customBrowserCommandText->SetValue(config->customBrowserPath);
	this->customBrowserFileBrowseButton = new wxButton(this, wxID_ANY, wxT("Browse..."));
	customBrowserFileBrowseButton->Bind(wxEVT_BUTTON, &QuickOpenSettings::OnCustomBrowserBrowseButtonClicked, this);
	this->customBrowserSizer = new wxBoxSizer(wxHORIZONTAL);
	customBrowserSizer->Add(customBrowserCommandText, wxSizerFlags(1).Expand());
	customBrowserSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
	customBrowserSizer->Add(customBrowserFileBrowseButton);
	webpageOpenGroupSizer->Add(customBrowserSizer, wxSizerFlags(0).Expand().Border(wxTOP, DEFAULT_CONTROL_SPACING));

	topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	this->fileOpenSaveGroup = new wxStaticBox(this, wxID_ANY, wxT("Opening/Saving Files"));
	this->fileOpenSaveGroupSizer = new wxStaticBoxSizer(fileOpenSaveGroup, wxVERTICAL);
	topLevelSizer->Add(fileOpenSaveGroupSizer, wxSizerFlags(0).Expand());

	fileOpenSaveGroupSizer->Add(
		makeLabeledSizer(new wxSpinCtrl(this, wxID_ANY, (wxString() << config->maxSaveFileSize), wxDefaultPosition,
		                                wxDefaultSize, wxSP_ARROW_KEYS, 0, 1024),
		                 wxT("Maximum size for uploaded files:"), this),
		wxSizerFlags(0).Expand());

	fileOpenSaveGroupSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	fileOpenSaveGroupSizer->Add(
		makeLabeledSizer(
			saveFolderPicker = new wxDirPickerCtrl(this, wxID_ANY, wxEmptyString, wxT("Select Download Folder")),
			wxT("Folder for downloads:"), this),
		wxSizerFlags(0).Expand());
	// saveFolderPicker->SetDirName(config->fileSavePath);
	saveFolderPicker->SetValidator(FilePathValidator(config->fileSavePath, true));

	fileOpenSaveGroupSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	fileOpenSaveGroupSizer->Add(
		savePromptEachFileCheckbox = new wxCheckBox(this, wxID_ANY, wxT("Prompt for a save location for each file")));
	savePromptEachFileCheckbox->Bind(wxEVT_CHECKBOX, &QuickOpenSettings::OnSavePromptCheckboxChecked, this);
	savePromptEachFileCheckbox->SetValue(config->alwaysPromptSave);

	topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	this->saveButton = new wxButton(this, wxID_OK);
	this->cancelButton = new wxButton(this, wxID_CANCEL);
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
	this->SetSizerAndFit(windowPaddingSizer);
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
		config->alwaysPromptSave = this->savePromptEachFileCheckbox->IsChecked();

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

void QuickOpenSettings::OnSavePromptCheckboxChecked(wxCommandEvent& event)
{
	updateSaveFolderEnabledState();
}

void QuickOpenSettings::updateSaveFolderEnabledState()
{
	if (savePromptEachFileCheckbox->GetValue())
	{
		saveFolderPicker->Disable();
	}
	else
	{
		saveFolderPicker->Enable();
	}
}

FileOpenSaveConsentDialog::FileOpenSaveConsentDialog(const wxFileName& defaultDestinationFolder, const FileConsentRequestInfo& requestInfo):
	wxDialog(nullptr, wxID_ANY, wxT("Receiving File"), wxDefaultPosition, wxDefaultSize), requestInfo(requestInfo)
{
	wxBoxSizer* windowPaddingSizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* topLevelSizer = new wxBoxSizer(wxVERTICAL);
	windowPaddingSizer->Add(topLevelSizer, wxSizerFlags(1).Expand().Border(wxALL, DEFAULT_CONTROL_SPACING));

	topLevelSizer->Add(new wxStaticText(this, wxID_ANY,
	                                    wxString() << wxT("A user is sending the following ")
														<< (requestInfo.fileList.size() > 1 ? wxT("files:") : wxT("file:"))/* << defaultDestination.
	                                    GetFullName() << wxT("\" (") << wxFileName::GetHumanReadableSize(fileSize) <<
	                                    wxT(").\n")
	                                    << wxT("Would you like to accept it?")*/
	));

	for(const auto& thisFile : requestInfo.fileList)
	{
		topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
		topLevelSizer->Add(new wxStaticText(this, wxID_ANY, wxString(wxT('\"')) << thisFile.filename << wxT("\" (")
			<< wxFileName::GetHumanReadableSize(thisFile.fileSize) << wxT(")")));
	}

	topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

	if(requestInfo.fileList.size() == 1)
	{
		wxFileName defaultDestination(defaultDestinationFolder);
		defaultDestination.SetFullName(requestInfo.fileList[0].filename);

		topLevelSizer->Add(makeLabeledSizer(destFilenameInput =
			new wxFilePickerCtrl(this, wxID_ANY, wxEmptyString,
				wxT("Select Save Destination"),
				wxString() << wxT(".") << defaultDestination.GetExt() <<
				wxT(" files|*.") << defaultDestination.GetExt() << wxT(
					"|All files|*.*"),
				wxDefaultPosition, wxDefaultSize,
				wxFLP_SAVE | wxFLP_USE_TEXTCTRL | wxFLP_OVERWRITE_PROMPT),
			wxT("Save this file to:"), this), wxSizerFlags(0).Expand());

		destFilenameInput->SetValidator(FilePathValidator(defaultDestination));
	}
	else
	{
		multiFileLayout = true;

		topLevelSizer->Add(makeLabeledSizer(
			destFolderNameInput = new wxDirPickerCtrl(this, wxID_ANY, defaultDestinationFolder.GetFullPath(), wxT("Select Save Destination Folder")),
			wxT("Save files to:"), this), wxSizerFlags(0).Expand());

		destFolderNameInput->SetValidator(FilePathValidator(defaultDestinationFolder, true));
	}

	topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
	topLevelSizer->AddStretchSpacer(1);

	auto* bottomButtonSizer = new wxBoxSizer(wxHORIZONTAL);

	bottomButtonSizer->AddStretchSpacer(1);
	bottomButtonSizer->Add(acceptButton = new wxButton(this, wxID_ANY, wxT("Accept")));
	bottomButtonSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
	bottomButtonSizer->Add(rejectButton = new wxButton(this, wxID_ANY, wxT("Decline")));

	topLevelSizer->Add(bottomButtonSizer, wxSizerFlags(0).Expand());

	acceptButton->Bind(wxEVT_BUTTON, &FileOpenSaveConsentDialog::OnAcceptClicked, this);
	rejectButton->Bind(wxEVT_BUTTON, &FileOpenSaveConsentDialog::OnDeclineClicked, this);

	this->SetSizerAndFit(windowPaddingSizer);
	this->TransferDataToWindow();
}

void FileOpenSaveConsentDialog::OnAcceptClicked(wxCommandEvent& event)
{
	if (this->Validate() && this->TransferDataFromWindow())
	{
		this->EndModal(ACCEPT);
	}
	else
	{
		std::cout << "Validation failed." << std::endl;
	}
}

void FileOpenSaveConsentDialog::OnDeclineClicked(wxCommandEvent& event)
{
	this->EndModal(DECLINE);
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