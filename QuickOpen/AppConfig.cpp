#include "AppConfig.h"

using namespace SettingRetrieval;

const wxFileName AppConfig::DEFAULT_CONFIG_PATH = InstallationInfo::detectInstallation().configFolder / wxFileName(wxT("."), wxT("config.json"));

namespace SettingRetrieval
{
    template<>
    bool getSettingWarn<wxString>(const nlohmann::json& config, const std::string& key, wxString& destination)
    {
        std::string strVal;
        bool successVal = getSettingWarn(config, key, strVal);
        if(successVal)
        {
            destination = wxString::FromUTF8(strVal);
        }
        return successVal;
    }
}

bool getSettingWarn(const nlohmann::json& config, const std::string& key, wxFileName& destination,
                               bool isDir)
{
	wxString strVal;
	bool successVal = getSettingWarn(config, key, strVal);
	if (successVal)
	{
		if (isDir)
		{
			destination.AssignDir(strVal);
		}
		else
		{
			destination.Assign(strVal);
		}
	}
	return successVal;
}

void AppConfig::saveConfig(const wxFileName& filePath)
{
	nlohmann::json jsonConfig = {
		{"runAtStartup", runAtStartup},
		{
			"webpageOpen", {
				{"browserID", browserID},
				{"customBrowserPath", customBrowserPath}
			}
		},
		{
			"openSaveFile", {
				{"saveUseLastFolder", saveUseLastFolder},
				{"savePath", fileSavePath.GetPath()}
			}
		}
	};

	std::ofstream fileOutput;
	fileOutput.exceptions(std::ofstream::failbit);
	fileOutput.open(filePath.GetFullPath());
	fileOutput << jsonConfig;

	//wxFileConfig config;
	//config.Write(CONFIG_KEY_NAMES[RUN_AT_STARTUP], runAtStartup);

	//std::ofstream fileOutput(filePath);
	//config.Save(fileOutput);
}

AppConfig AppConfig::loadConfig(const wxFileName& filePath)
{
	nlohmann::json jsonConfig;

	std::ifstream fileInput;
	fileInput.exceptions(std::ifstream::failbit);
	fileInput.open(filePath.GetFullPath());
	fileInput >> jsonConfig;

	AppConfig newConfig;
	getSettingWarn(jsonConfig, "runAtStartup", newConfig.runAtStartup);

	nlohmann::json openWebpageSettings;
	if (getSettingWarn(jsonConfig, "webpageOpen", openWebpageSettings))
	{
		getSettingWarn(openWebpageSettings, "browserID", newConfig.browserID);
		getSettingWarn(openWebpageSettings, "customBrowserPath", newConfig.customBrowserPath);
	}

	nlohmann::json openSaveFileSettings;
	if (getSettingWarn(jsonConfig, "openSaveFile", openSaveFileSettings))
	{
		getSettingWarn(openSaveFileSettings, "saveUseLastFolder", newConfig.saveUseLastFolder);
		getSettingWarn(openSaveFileSettings, "savePath", newConfig.fileSavePath, true);
	}

	return newConfig;
}
