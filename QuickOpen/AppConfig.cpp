#include "AppConfig.h"

using namespace SettingRetrieval;

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
	const wxFileName& effectiveFilePath = (filePath != wxFileName()) ? filePath : defaultConfigPath();

	nlohmann::json jsonConfig = {
		// {"runAtStartup", runAtStartup},
		{"serverPort", serverPort},
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

	wxFileName dirName = getDirName(effectiveFilePath);
	if(!dirName.DirExists() && !dirName.Mkdir())
	{
		throw std::runtime_error("The configuration folder does not exist and could not be created.");
	}

	std::ofstream fileOutput;
	fileOutput.exceptions(std::ofstream::failbit);
#ifdef WIN32
	fileOutput.open(wxStringToTString(effectiveFilePath.GetFullPath()));
#else
	fileOutput.open(effectiveFilePath.GetFullPath());
#endif
	fileOutput << jsonConfig;

	//wxFileConfig config;
	//config.Write(CONFIG_KEY_NAMES[RUN_AT_STARTUP], runAtStartup);

	//std::ofstream fileOutput(filePath);
	//config.Save(fileOutput);
}

AppConfig AppConfig::loadConfig(const wxFileName& filePath)
{
	const wxFileName& effectiveFilePath = (filePath != wxFileName()) ? filePath : defaultConfigPath();
	nlohmann::json jsonConfig;

	std::ifstream fileInput;
	fileInput.exceptions(std::ifstream::failbit);
#ifdef WIN32
	fileInput.open(wxStringToTString(effectiveFilePath.GetFullPath()));
#else
	fileInput.open(effectiveFilePath.GetFullPath());
#endif
	fileInput >> jsonConfig;

	AppConfig newConfig;
	// getSettingWarn(jsonConfig, "runAtStartup", newConfig.runAtStartup);

	getSettingWarn(jsonConfig, "serverPort", newConfig.serverPort);

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
