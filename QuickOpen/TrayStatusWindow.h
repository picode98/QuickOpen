#pragma once

#include <wx/wx.h>

#include <wx/filename.h>
#include <wx/statline.h>


#include <atomic>

#include "GUIUtils.h"

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

	public:
		ActivityList(wxWindow* parent);

		void addActivity(ActivityEntry* entry);

		void removeActivity(ActivityEntry* entry);

		// wxDECLARE_EVENT_TABLE();
	};
private:
	wxBoxSizer* topLevelSizer = nullptr;
	ActivityList* activityList = nullptr;

	void OnWindowActivationChanged(wxActivateEvent& event);
	void fitActivityListWidth();

public:
	TrayStatusWindow();

	WebpageOpenedActivityEntry* addWebpageOpenedActivity(const wxString& url);

	FileUploadActivityEntry* addFileUploadActivity(const wxFileName& filename, std::atomic<bool>& cancelRequestFlag);

	void SetFocus() override;

	wxDECLARE_EVENT_TABLE();
};
