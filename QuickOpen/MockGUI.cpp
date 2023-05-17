//
// Created by sdk on 5/15/2023.
//

#include "MockGUI.h"
#include "WebServer.h"

std::pair<ConsentDialog::ResultCode, bool>
QuickOpenApplication::promptForFileSave(const wxFileName &defaultDestDir, const wxString &requesterName,
                                        FileConsentRequestInfo &rqFileInfo) {
    this->promptedForFileSave = true;
    wxFileName destFolder = fileDestFolder.value_or(defaultDestDir);

    if(confirmPrompts)
    {
        for (auto& thisFile : rqFileInfo.fileList)
        {
            thisFile.consentedFileName = destFolder / wxFileName("", thisFile.filename);
        }

        return { ConsentDialog::ResultCode::ACCEPT, requestBan };
    }
    else
    {
        return { ConsentDialog::ResultCode::DECLINE, requestBan };
    }
}
