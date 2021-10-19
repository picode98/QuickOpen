#include "GUIUtils.h"

wxString ProgressBarWithText::getText()
{
	return wxString::Format("%.1f%%", this->progress);
}

ProgressBarWithText::ProgressBarWithText(wxWindow* parent, double progress) : wxBoxSizer(wxHORIZONTAL),
progress(progress),
gaugeValue(
	static_cast<int>(progress * gaugeRange
		))
{
	this->Add(progressBar = new wxGauge(parent, wxID_ANY, gaugeRange), wxSizerFlags(1).Expand());
	this->AddSpacer(DEFAULT_CONTROL_SPACING);
	this->Add(progressText = new wxStaticText(parent, wxID_ANY, this->getText()), wxSizerFlags(0).CenterVertical());
}

void ProgressBarWithText::setValue(int value)
{
	this->gaugeValue = value;
	this->progress = (value * 100.0) / gaugeRange;

	this->progressBar->SetValue(gaugeValue);
	this->progressText->SetLabel(getText());
}

void ProgressBarWithText::setProgress(double progress)
{
	this->gaugeValue = static_cast<int>(std::round((progress / 100.0) * gaugeRange));
	this->progress = progress;

	this->progressBar->SetValue(gaugeValue);
	this->progressText->SetLabel(getText());
}

void ProgressBarWithText::setErrorStyle()
{
	// this->progressBar->SetForegroundColour(ERROR_TEXT_COLOR);
	this->progressText->SetForegroundColour(ERROR_TEXT_COLOR);
}

void AutoWrappingStaticText::OnSize(wxSizeEvent& event)
{
	if (!this->wrappingInProgress)
	{
		this->wrappingInProgress = true;
		this->SetLabel(this->unwrappedLabel);
		this->Wrap(event.GetSize().GetWidth());
		auto size = event.GetSize();
		this->wrappingInProgress = false;
	}

	event.Skip();
}

BEGIN_EVENT_TABLE(AutoWrappingStaticText, wxStaticText)
	EVT_SIZE(AutoWrappingStaticText::OnSize)
END_EVENT_TABLE()