#include "TrayStatusWindow.h"

#include <wx/clipbrd.h>
#include <wx/hyperlink.h>
#include <wx/url.h>

#include "AppConfig.h"
#include "AppGUI.h"
#include "GUIUtils.h"
#include "PlatformUtils.h"

TrayStatusWindow::ActivityEntry::ActivityEntry(wxWindow* parent) : wxBoxSizer(wxVERTICAL)
{
}

void TrayStatusWindow::OnWindowActivationChanged(wxActivateEvent& event)
{
	// GTK generates spurious deactivation events
	if (wxPlatformInfo::Get().GetPortId() != wxPORT_GTK && !event.GetActive())
	{
		this->Hide();
	}

	event.Skip();
}

void TrayStatusWindow::OnShow(wxShowEvent& event)
{
	if (event.IsShown())
	{
		auto* newDisplay = new ServerURLDisplay(topLevelPanel, getPhysicalNetworkInterfaces(), 
			WriterReadersLock<AppConfig>::ReadableReference(*appRef.getConfigRef())->serverPort);
		bool replaced = topLevelSizer->Replace(URLDisplay, newDisplay);
		assert(replaced);
		topLevelPanel->RemoveChild(URLDisplay);
		URLDisplay->Destroy();
		URLDisplay = newDisplay;
		this->Layout();
	}
}

void TrayStatusWindow::fitActivityListWidth()
{
	static const int WIDTH_PADDING = 50;
	wxSize vSize = this->activityList->GetVirtualSize(),
	       phySize = this->GetSize();
	int minWidth = vSize.GetX() + FromDIP(WIDTH_PADDING);

	if (phySize.GetX() < minWidth)
	{
		this->SetSize(wxSize(minWidth, phySize.GetY()));
	}
}

TrayStatusWindow::TrayStatusWindow(IQuickOpenApplication& appRef) : wxFrame(nullptr, wxID_ANY, wxT("QuickOpen Tray Status Window"), wxDefaultPosition,
	wxSize(300, 300),
	(wxDEFAULT_FRAME_STYLE | wxSTAY_ON_TOP | wxFRAME_NO_TASKBAR) & ~(
		wxMINIMIZE_BOX | wxMAXIMIZE_BOX)), appRef(appRef)
{
	this->SetIcon(wxIcon(getAppIconPath().GetFullPath(), wxBITMAP_TYPE_ICO));

	topLevelPanel = new wxPanel(this);
	auto* panelSizer = new wxBoxSizer(wxVERTICAL);
	panelSizer->Add(topLevelPanel, wxSizerFlags(1).Expand());

	topLevelSizer = new wxBoxSizer(wxVERTICAL);
	topLevelSizer->Add(URLDisplay = new ServerURLDisplay(topLevelPanel, getPhysicalNetworkInterfaces(), 
		WriterReadersLock<AppConfig>::ReadableReference(*appRef.getConfigRef())->serverPort), wxSizerFlags(0).Expand());
	topLevelSizer->Add(activityList = new ActivityList(topLevelPanel), wxSizerFlags(1).Expand());
	setSizerWithPadding(topLevelPanel, topLevelSizer);
	topLevelPanel->Fit();
	this->SetSizer(panelSizer);
}

TrayStatusWindow::WebpageOpenedActivityEntry* TrayStatusWindow::addWebpageOpenedActivity(const wxString& url)
{
	auto* newActivity = new WebpageOpenedActivityEntry(this->activityList, url);
	this->activityList->addActivity(newActivity);
	// this->topLevelSizer->Add(new wxStaticText(this, wxID_ANY, wxT("A test label.")), wxSizerFlags(0).Expand());
	this->Layout();
	this->fitActivityListWidth();
	// this->Fit();

	return newActivity;
}

TrayStatusWindow::FileUploadActivityEntry* TrayStatusWindow::addFileUploadActivity(const wxFileName& filename,
	std::atomic<bool>& cancelRequestFlag)
{
	auto* newActivity = new FileUploadActivityEntry(this->activityList, filename.GetFullPath(), cancelRequestFlag);
	this->activityList->addActivity(newActivity);

	this->Layout();
	this->fitActivityListWidth();
	// this->Fit();

	return newActivity;
}

void TrayStatusWindow::SetFocus()
{
	wxFrame::SetFocus();
	this->activityList->SetFocus();
}

void TrayStatusWindow::OnClose(wxCloseEvent& event)
{
    if(event.CanVeto())
    {
        event.Veto();
        this->Hide();
    }
    else
    {
        event.Skip();
    }
}

wxBEGIN_EVENT_TABLE(TrayStatusWindow, wxFrame)
	EVT_ACTIVATE(TrayStatusWindow::OnWindowActivationChanged)
	EVT_SHOW(TrayStatusWindow::OnShow)
    EVT_CLOSE(TrayStatusWindow::OnClose)
wxEND_EVENT_TABLE()

void TrayStatusWindow::WebpageOpenedActivityEntry::OnCopyURLButtonClick(wxCommandEvent& event)
{
	if (wxTheClipboard->Open())
	{
		wxTheClipboard->SetData(new wxURLDataObject(URL));
		wxTheClipboard->Close();
	}
}

TrayStatusWindow::WebpageOpenedActivityEntry::
WebpageOpenedActivityEntry(wxWindow* parent, const wxString& url) : ActivityEntry(parent), URL(url)
{
	horizSizer = new wxBoxSizer(wxHORIZONTAL);

	horizSizer->Add(entryText = new wxStaticText(parent, wxID_ANY,
		wxString() << wxT("Opened the URL \"") << url << wxT("\"."),
		wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_MIDDLE), wxSizerFlags(1).CenterVertical());
	entryText->SetMinSize(wxSize(0, entryText->GetMinHeight()));
	horizSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
	horizSizer->Add(copyURLButton = new wxButton(parent, wxID_ANY, wxT("Copy URL")));
	this->Add(horizSizer, wxSizerFlags(1).Expand());

	copyURLButton->Bind(wxEVT_BUTTON, &WebpageOpenedActivityEntry::OnCopyURLButtonClick, this);

	// this->SetSizerAndFit(horizSizer);
}

wxString TrayStatusWindow::FileUploadActivityEntry::getEntryText() const
{
	return wxString() << (this->uploadCompleted ? wxT("Uploaded \"") : wxT("Uploading \"")) << this
		->filename.GetFullPath()
		<< wxT("\"")
		<< (this->uploadCompleted ? wxT(".") : wxT("..."));
}

void TrayStatusWindow::FileUploadActivityEntry::updateProgressBar()
{
	this->entryText->SetLabel(getEntryText());

	if (this->uploadCompleted)
	{
		this->fileUploadProgress->setProgress(100.0);
	}
	else
	{
		this->fileUploadProgress->setProgress(this->uploadProgress);
	}
}

void TrayStatusWindow::FileUploadActivityEntry::triggerOpen()
{
	shellExecuteFile(this->filename, this->parent);
}

void TrayStatusWindow::FileUploadActivityEntry::updateOpenButtonText()
{
	wxString newText;

	if (this->uploadCompleted)
	{
		newText = wxT("Open");
	}
	else
	{
		newText = (this->openWhenDone ? wxT("Don't Open When Done") : wxT("Open When Done"));
	}

	this->openButton->SetLabel(newText);
}

void TrayStatusWindow::FileUploadActivityEntry::OnOpenButtonClicked(wxCommandEvent& event)
{
	if (this->uploadCompleted)
	{
		triggerOpen();
	}
	else
	{
		this->openWhenDone = !this->openWhenDone;
		updateOpenButtonText();
	}
}

void TrayStatusWindow::FileUploadActivityEntry::OnOpenFolderButtonClicked(wxCommandEvent& event)
{
	wxFileName folderToOpen(this->filename);
	folderToOpen.SetFullName(wxT(""));

	if (this->uploadCompleted)
	{
		openExplorerFolder(folderToOpen, &this->filename);
	}
	else
	{
		openExplorerFolder(folderToOpen);
	}
}

void TrayStatusWindow::FileUploadActivityEntry::OnCancelButtonClicked(wxCommandEvent& event)
{
	this->cancelButton->Enable(false);
	this->openButton->Enable(false);
	this->cancelButton->SetLabel(wxT("Canceling..."));
	this->cancelRequestFlag = true;
}

TrayStatusWindow::FileUploadActivityEntry::FileUploadActivityEntry(wxWindow* parent, const wxString& filename,
	std::atomic<bool>& cancelRequestFlag,
	bool uploadCompleted,
	double uploadProgress) : ActivityEntry(parent),
	parent(parent),
	filename(filename),
	cancelRequestFlag(
		cancelRequestFlag),
	uploadCompleted(
		uploadCompleted),
	uploadProgress(
		uploadProgress)
{
	// vertSizer = new wxBoxSizer(wxVERTICAL);
	this->Add(entryText = new wxStaticText(parent, wxID_ANY, this->getEntryText(), wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_MIDDLE), wxSizerFlags(0).Expand());
	entryText->SetMinSize(wxSize(0, entryText->GetMinHeight()));
	this->AddSpacer(DEFAULT_CONTROL_SPACING);
	this->Add(fileUploadProgress = new ProgressBarWithText(parent), wxSizerFlags(0).Expand());
	this->AddSpacer(DEFAULT_CONTROL_SPACING);
	this->Add(bottomButtonSizer = new wxBoxSizer(wxHORIZONTAL));

	bottomButtonSizer->Add(openButton = new wxButton(parent, wxID_ANY, wxT("Open When Done")));
	bottomButtonSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
	bottomButtonSizer->Add(openFolderButton = new wxButton(parent, wxID_ANY, wxT("Open Folder")));
	bottomButtonSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
	bottomButtonSizer->Add(cancelButton = new wxButton(parent, wxID_CANCEL));
	cancelButton->Bind(wxEVT_BUTTON, &FileUploadActivityEntry::OnCancelButtonClicked, this);

	openButton->Bind(wxEVT_BUTTON, &FileUploadActivityEntry::OnOpenButtonClicked, this);
	openFolderButton->Bind(wxEVT_BUTTON, &FileUploadActivityEntry::OnOpenFolderButtonClicked, this);
	updateOpenButtonText();
	this->Add(errorText = new wxStaticText(parent, wxID_ANY, wxT("")));
	errorText->SetForegroundColour(ERROR_TEXT_COLOR);
	this->Hide(errorText);

	// this->SetSizerAndFit(vertSizer);
}

void TrayStatusWindow::FileUploadActivityEntry::setProgress(double progress)
{
	this->uploadProgress = progress;
	this->updateProgressBar();
}

double TrayStatusWindow::FileUploadActivityEntry::getProgress() const
{
	return this->uploadProgress;
}

void TrayStatusWindow::FileUploadActivityEntry::setCompleted(bool completed)
{
	this->uploadCompleted = completed;
	this->updateProgressBar();
	this->updateOpenButtonText();
	this->cancelButton->Enable(false);

	if (completed && this->openWhenDone)
	{
		triggerOpen();
	}
}

bool TrayStatusWindow::FileUploadActivityEntry::getCompleted() const
{
	return this->uploadCompleted;
}

void TrayStatusWindow::FileUploadActivityEntry::setError(const std::exception* error)
{
	const auto* systemErrorPtr = dynamic_cast<const std::system_error*>(error);

	if (systemErrorPtr != nullptr)
	{
		errorText->SetLabel(
			wxString() << wxT("An error occurred (error code ") << systemErrorPtr->code().value() << wxT("): ") <<
			systemErrorPtr->what());
	}
	else
	{
		errorText->SetLabel(wxString() << wxT("An error occurred: ") << error->what());
	}

	openButton->Enable(false);
	cancelButton->Enable(false);
	fileUploadProgress->setErrorStyle();
}

void TrayStatusWindow::FileUploadActivityEntry::setCancelCompleted()
{
	assert(this->cancelRequestFlag);
	this->cancelButton->SetLabel(wxT("Cancel"));
}

TrayStatusWindow::ActivityList::ActivityList(wxWindow* parent) : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL)
{
	topLevelSizer = new wxBoxSizer(wxVERTICAL);

	noItemsElement = new wxBoxSizer(wxVERTICAL);
	noItemsElement->Add(
		new wxStaticText(this, wxID_ANY, wxT("No activity items.")),
		wxSizerFlags(1).CenterHorizontal().Border(wxALL, 10));
	topLevelSizer->Add(noItemsElement, wxSizerFlags(0).Expand());

	items = new wxBoxSizer(wxVERTICAL);
	topLevelSizer->Add(items, wxSizerFlags(0).Expand());
	topLevelSizer->Hide(items, true);
	topLevelSizer->Layout();

	this->SetSizer(topLevelSizer);
	this->FitInside();
	this->SetScrollRate(5, 5);
	// this->Refresh();
	// this->Bind(EVT_SCROLLWIN(), &ActivityList::OnScroll, this);
	// this->SetSizerAndFit(topLevelSizer);
}

void TrayStatusWindow::ActivityList::addActivity(ActivityEntry* entry)
{
	wxStaticLine* separator = nullptr;
	if (!this->items->IsEmpty())
	{
		this->items->Add(separator = new wxStaticLine(this),
			wxSizerFlags(0).Expand().Border(wxTOP | wxBOTTOM, DEFAULT_CONTROL_SPACING));
	}

	// this->items->Add(new wxStaticText(parent, wxID_ANY, wxT("A test label.")), wxSizerFlags(0).Expand());
	this->items->Add(entry, wxSizerFlags(0).Expand().Border(wxBOTTOM, DEFAULT_CONTROL_SPACING));
	entry->topSeparator = separator;
	topLevelSizer->Hide(noItemsElement, true);
	topLevelSizer->Show(items, true, true);
	this->Layout();
}

void TrayStatusWindow::ActivityList::removeActivity(ActivityEntry* entry)
{
	this->items->Detach(entry);
	if (entry->topSeparator != nullptr)
	{
		this->items->Detach(entry->topSeparator);
		entry->topSeparator->Destroy();
		entry->topSeparator = nullptr;
	}

	if (this->items->GetItemCount() == 0)
	{
		topLevelSizer->Hide(items, true);
		topLevelSizer->Show(noItemsElement, true, true);
		this->Layout();
	}
}

//wxBEGIN_EVENT_TABLE(TrayStatusWindow::ActivityList, wxScrolledWindow)
//	EVT_SIZE(TrayStatusWindow::ActivityList::OnSize)
//wxEND_EVENT_TABLE()

TrayStatusWindow::ServerURLDisplay::ServerURLDisplay(wxWindow* parent, const std::vector<NetworkInterfaceInfo>& interfaces, int serverPort):
	wxWindow(parent, wxID_ANY)
{
	auto* topLevelSizer = new wxBoxSizer(wxVERTICAL);
	headerText = new AutoWrappingStaticText(this, wxID_ANY, interfaces.empty() ?
		wxT("No active LAN connections detected. Connect to a LAN to open content on this computer.") :
		wxT("To open content on this computer, share the following links with others on your network:"));
	topLevelSizer->Add(headerText, wxSizerFlags(0).Expand());

	for(const NetworkInterfaceInfo& thisInterface : interfaces)
	{
		if (!thisInterface.IPAddresses.empty())
		{
#if NetworkInterfaceInfo_DRIVER_NAME_AVAILABLE
            auto* interfaceText = new AutoWrappingStaticText(this, wxID_ANY, wxString() << thisInterface.interfaceName << wxT(" (") << thisInterface.driverName << wxT("):"));
#else
            auto* interfaceText = new AutoWrappingStaticText(this, wxID_ANY, wxString() << thisInterface.interfaceName << wxT(":"));
#endif

			interfaceText->SetFont(interfaceText->GetFont().Bold());
			topLevelSizer->Add(
				interfaceText,
				wxSizerFlags(0).Expand()
			);

			for (const IPAddress& thisAddress : thisInterface.IPAddresses)
			{
                if(!thisAddress.isLinkLocal)
                {
                    wxString thisURL = wxString() << wxT("http://")
                        << ((thisAddress.type == IPAddress::IPV6) ? (wxT("[") + thisAddress.addressStr + wxT("]")) : thisAddress.addressStr) << wxT(":") << serverPort;

                    topLevelSizer->Add(
                        new wxHyperlinkCtrl(this, wxID_ANY, thisURL, thisURL),
                        wxSizerFlags(0).Expand().Border(wxLEFT, this->FromDIP(10))
                    );
                }
			}
		}
	}

	this->SetSizerAndFit(topLevelSizer);
}

//void TrayStatusWindow::ServerURLDisplay::OnSize(wxSizeEvent& event)
//{
//	this->SetSize(this->GetSize().GetWidth(), 800 - event.GetSize().GetHeight());
//
//	event.Skip();
//}
//
//BEGIN_EVENT_TABLE(TrayStatusWindow::ServerURLDisplay, wxWindow)
//	EVT_SIZE(TrayStatusWindow::ServerURLDisplay::OnSize)
//END_EVENT_TABLE()