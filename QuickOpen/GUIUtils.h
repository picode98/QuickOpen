#pragma once

#include "Utils.h"

#include <wx/wx.h>

template<typename T>
T& getValidator(wxWindow* obj)
{
	return dynamic_cast<T&>(*(obj->GetValidator()));
}

const int DEFAULT_CONTROL_SPACING = 7;
const wxColour ERROR_TEXT_COLOR = wxColour(0x0000a0);

inline int getMessageSeverityIconStyle(MessageSeverity severity)
{
	switch (severity)
	{
		case MessageSeverity::MSG_ERROR:
			return wxICON_INFORMATION;
		case MessageSeverity::MSG_WARNING:
			return wxICON_WARNING;
		case MessageSeverity::MSG_INFO:
		case MessageSeverity::MSG_DEBUG:
			return wxICON_INFORMATION;
		default:
			throw std::domain_error("Invalid message severity");
	}
}

inline wxSizer* setSizerWithPadding(wxWindow* window, wxSizer* content, int paddingWidth = DEFAULT_CONTROL_SPACING)
{
	auto* paddingSizer = new wxBoxSizer(wxVERTICAL);
	paddingSizer->Add(content, wxSizerFlags(1).Expand().Border(wxALL, paddingWidth));
	window->SetSizer(paddingSizer);

	return paddingSizer;
}

class ProgressBarWithText : public wxBoxSizer
{
	wxGauge* progressBar = nullptr;
	wxStaticText* progressText = nullptr;

	int gaugeValue = 0;
	int gaugeRange = 100;

	double progress = 0.0;

	wxString getText();
public:
	ProgressBarWithText(wxWindow* parent, double progress = 0.0);

	void setValue(int value);

	void setProgress(double progress);

	void setErrorStyle();
};

class AutoWrappingStaticText : public wxStaticText
{
	wxString unwrappedLabel;
	bool wrappingInProgress = false;

	void OnSize(wxSizeEvent& event);

public:
	AutoWrappingStaticText(wxWindow *parent,
		wxWindowID id,
		const wxString& label,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = 0,
		const wxString& name = wxASCII_STR(wxStaticTextNameStr)) : wxStaticText(parent, id, label, pos, size, style, name),
			unwrappedLabel(label)
	{
		this->Wrap(this->GetClientSize().GetWidth());
	}

	void setUnwrappedLabel(const wxString& newLabel)
	{
		this->unwrappedLabel = newLabel;
		this->SetLabel(newLabel);
		this->Wrap(this->GetClientSize().GetWidth());
	}

	wxString getUnwrappedLabel() const
	{
		return this->unwrappedLabel;
	}

	DECLARE_EVENT_TABLE()
};