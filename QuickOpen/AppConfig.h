#pragma once

#include <nlohmann/json.hpp>
// #include <boost/serialization/arc>

#include <fstream>
#include <filesystem>
#include <map>

#ifdef _WIN32
#include "WinUtils.h"
#endif

struct AppConfig
{
	// static const std::map<ConfigKey, wxString> CONFIG_KEY_NAMES;
	
	static const std::filesystem::path DEFAULT_CONFIG_PATH;
	bool runAtStartup = false;

	wxString browserID;
	wxString customBrowserPath;

	long maxSaveFileSize = 1 << 20;

	template<typename T>
	static bool getSettingWarn(const nlohmann::json& config, const std::string& key, T& destination)
	{
		try
		{
			destination = config.at(key);
			return true;
		}
		catch (const nlohmann::json::out_of_range&)
		{
			std::cerr << "WARNING: Expected key \"" << key << "\" did not exist in the following settings object: " << config << std::endl;
			return false;
		}
	}

	template<>
	static bool getSettingWarn<wxString>(const nlohmann::json& config, const std::string& key, wxString& destination)
	{
		std::string strVal;
		bool successVal = getSettingWarn(config, key, strVal);
		if(successVal)
		{
			destination = wxString::FromUTF8(strVal);
		}
		return successVal;
	}
	
	void saveConfig(const std::filesystem::path& filePath = DEFAULT_CONFIG_PATH)
	{
		nlohmann::json jsonConfig = {
			{ "runAtStartup", runAtStartup },
			{ "webpageOpen", {
				{ "browserID", browserID.ToUTF8() },
				{ "customBrowserPath", customBrowserPath.ToUTF8() }
			} }
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

	static AppConfig loadConfig(const std::filesystem::path& filePath = DEFAULT_CONFIG_PATH)
	{
		nlohmann::json jsonConfig;
		
		std::ifstream fileInput;
		fileInput.exceptions(std::ifstream::failbit);
		fileInput.open(filePath);
		fileInput >> jsonConfig;

		AppConfig newConfig;
		getSettingWarn(jsonConfig, "runAtStartup", newConfig.runAtStartup);

		nlohmann::json openWebpageSettings;
		if(getSettingWarn(jsonConfig, "webpageOpen", openWebpageSettings))
		{
			getSettingWarn(openWebpageSettings, "browserID",  newConfig.browserID);
			getSettingWarn(openWebpageSettings, "customBrowserPath", newConfig.customBrowserPath);
		}

		return newConfig;
	}
};

const std::filesystem::path AppConfig::DEFAULT_CONFIG_PATH = getAppExecutablePath().replace_filename("config.json");
// const std::map<AppConfig::ConfigKey, wxString> AppConfig::CONFIG_KEY_NAMES = { { RUN_AT_STARTUP, wxT("runAtStartup") } };