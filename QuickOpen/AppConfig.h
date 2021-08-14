#pragma once

#ifdef _WIN32
#include "WinUtils.h"
#endif

#include <nlohmann/json.hpp>
// #include <boost/serialization/arc>

#include <fstream>
#include <filesystem>
#include <map>

struct AppConfig
{
	// static const std::map<ConfigKey, wxString> CONFIG_KEY_NAMES;
	
	static const std::filesystem::path DEFAULT_CONFIG_PATH;
	bool runAtStartup = false;

	wxString browserID;
	wxString customBrowserPath;

	bool alwaysPromptSave = true;
	wxFileName fileSavePath;

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

	static bool getSettingWarn(const nlohmann::json& config, const std::string& key, wxFileName& destination,
	                           bool isDir);

	void saveConfig(const std::filesystem::path& filePath = DEFAULT_CONFIG_PATH);

	static AppConfig loadConfig(const std::filesystem::path& filePath = DEFAULT_CONFIG_PATH);
};

// const std::map<AppConfig::ConfigKey, wxString> AppConfig::CONFIG_KEY_NAMES = { { RUN_AT_STARTUP, wxT("runAtStartup") } };