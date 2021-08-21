#pragma once

#include <wx/wx.h>

const int DEFAULT_CONTROL_SPACING = 7;
const wxColour ERROR_TEXT_COLOR = wxColour(0x0000a0);

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