#pragma once

#include <wx/wx.h>

template<typename T>
T& getValidator(wxWindow* obj)
{
	return dynamic_cast<T&>(*(obj->GetValidator()));
}

const int DEFAULT_CONTROL_SPACING = 7;
const wxColour ERROR_TEXT_COLOR = wxColour(0x0000a0);

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