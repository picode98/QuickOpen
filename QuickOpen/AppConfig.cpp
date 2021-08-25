#include "AppConfig.h"

const std::filesystem::path AppConfig::DEFAULT_CONFIG_PATH = getAppExecutablePath().replace_filename("config.json");

bool AppConfig::getSettingWarn(const nlohmann::json& config, const std::string& key, wxFileName& destination,
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

void AppConfig::saveConfig(const std::filesystem::path& filePath)
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
				{"alwaysPromptSave", alwaysPromptSave},
				{"savePath", fileSavePath.GetPath()}
			}
		}
	};

	std::ofstream fileOutput;
	fileOutput.exceptions(std::ofstream::failbit);
	fileOutput.open(filePath);
	fileOutput << jsonConfig;

	//wxFileConfig config;
	//config.Write(CONFIG_KEY_NAMES[RUN_AT_STARTUP], runAtStartup);

	//std::ofstream fileOutput(filePath);
	//config.Save(fileOutput);
}

AppConfig AppConfig::loadConfig(const std::filesystem::path& filePath)
{
	nlohmann::json jsonConfig;

	std::ifstream fileInput;
	fileInput.exceptions(std::ifstream::failbit);
	fileInput.open(filePath);
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
		getSettingWarn(openSaveFileSettings, "alwaysPromptSave", newConfig.alwaysPromptSave);
		getSettingWarn(openSaveFileSettings, "savePath", newConfig.fileSavePath, true);
	}

	return newConfig;
}
