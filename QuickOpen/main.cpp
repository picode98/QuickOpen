//
// Created by sdk on 1/4/22.
//

#ifdef MOCK_GUI
#error "Cannot define entry point for mock GUI."
#else
#include "AppGUI.h"

wxIMPLEMENT_APP(QuickOpenApplication);
#endif