#pragma once

#include <wx/wx.h>

#include <wx/filename.h>
#include <wx/statline.h>


#include <atomic>

#include "GUIUtils.h"
#include "PlatformUtils.h"

struct AppConfig;

class TrayStatusWindow : public wxFrame
{
public:
	class ActivityEntry : public wxBoxSizer
	{
	public:
		wxStaticLine* topSeparator = nullptr;

		ActivityEntry(wxWindow* parent);
	};

	class WebpageOpenedActivityEntry : public ActivityEntry
	{
		wxBoxSizer* horizSizer = nullptr;
		wxStaticText* entryText = nullptr;
		wxButton* copyURLButton = nullptr;
		wxString URL;

		void OnCopyURLButtonClick(wxCommandEvent& event);
	public:
		WebpageOpenedActivityEntry(wxWindow* parent, const wxString& url);
	};

	class FileUploadActivityEntry : public ActivityEntry
	{
		wxWindow* parent = nullptr;

		// wxBoxSizer* vertSizer = nullptr;
		wxStaticText* entryText = nullptr;
		ProgressBarWithText* fileUploadProgress = nullptr;
		wxBoxSizer* bottomButtonSizer = nullptr;
		wxButton* openButton = nullptr;
		wxButton* openFolderButton = nullptr;
		wxButton* cancelButton = nullptr;
		std::atomic<bool>& cancelRequestFlag;
		wxStaticText* errorText = nullptr;

		wxFileName filename;
		double uploadProgress = 0.0;
		bool uploadCompleted = false,
			openWhenDone = false;

		wxString getEntryText() const;

		void updateProgressBar();

		void triggerOpen();

		void updateOpenButtonText();

		void OnOpenButtonClicked(wxCommandEvent& event);

		void OnOpenFolderButtonClicked(wxCommandEvent& event);

		void OnCancelButtonClicked(wxCommandEvent& event);
	public:
		FileUploadActivityEntry(wxWindow* parent, const wxString& filename, std::atomic<bool>& cancelRequestFlag,
			bool uploadCompleted = false, double uploadProgress = 0.0);

		void setProgress(double progress);

		double getProgress() const;

		void setCompleted(bool completed);

		bool getCompleted() const;

		void setError(const std::exception* error);

		void setCancelCompleted();
	};

	class ActivityList : public wxScrolledWindow
	{
		// wxWindow* parent = nullptr;
		wxBoxSizer* topLevelSizer = nullptr;
		wxBoxSizer* noItemsElement = nullptr;
		wxBoxSizer* items = nullptr;

		//void OnScroll(wxScrollWinEvent& event)
		//{
		//	this->Refresh();
		//}

		//void OnSize(wxSizeEvent& event)
		//{
		//	this->SetVirtualSize(1500, this->GetVirtualSize().GetHeight());
		//	event.Skip();
		//}

	public:
		ActivityList(wxWindow* parent);

		void addActivity(ActivityEntry* entry);

		void removeActivity(ActivityEntry* entry);

		// wxDECLARE_EVENT_TABLE();
	};

	class ServerURLDisplay : public wxWindow
	{
		AutoWrappingStaticText* headerText = nullptr;

		// void OnSize(wxSizeEvent& event);
	public:
		ServerURLDisplay(wxWindow* parent, const std::vector<NetworkInterfaceInfo>& interfaces, int serverPort);

		// wxDECLARE_EVENT_TABLE();
	};
private:
	WriterReadersLock<AppConfig>& configRef;

	wxPanel* topLevelPanel = nullptr;
	wxBoxSizer* topLevelSizer = nullptr;
	ServerURLDisplay* URLDisplay = nullptr;
	ActivityList* activityList = nullptr;

	void OnWindowActivationChanged(wxActivateEvent& event);
	void OnShow(wxShowEvent& event);
    void OnClose(wxCloseEvent& event);
	void fitActivityListWidth();

public:
	TrayStatusWindow(WriterReadersLock<AppConfig>& configRef);

    void showAtCursor()
    {
        this->SetPosition(wxGetMousePosition() - this->GetSize());
        this->Show();
        activateWindow(this);
    }

	WebpageOpenedActivityEntry* addWebpageOpenedActivity(const wxString& url);

	FileUploadActivityEntry* addFileUploadActivity(const wxFileName& filename, std::atomic<bool>& cancelRequestFlag);

	void SetFocus() override;

	wxDECLARE_EVENT_TABLE();
};
