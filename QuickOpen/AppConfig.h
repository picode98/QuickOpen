#pragma once

#include "PlatformUtils.h"

#include <wx/wx.h>
#include <wx/filename.h>

#include <nlohmann/json.hpp>
#include "Utils.h"
// #include <boost/serialization/arc>

#include <fstream>
#include <filesystem>
#include <map>

namespace SettingRetrieval
{
    template<typename T>
    bool getSettingWarn(const nlohmann::json& config, const std::string& key, T& destination)
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
}

struct AppConfig
{
	// static const std::map<ConfigKey, wxString> CONFIG_KEY_NAMES;

	static const wxFileName DEFAULT_CONFIG_PATH;
	bool runAtStartup = false;

	wxString browserID;
	wxString customBrowserPath;

	bool saveUseLastFolder = true;
	wxFileName fileSavePath;

	long maxSaveFileSize = 1 << 20;

	void saveConfig(const wxFileName& filePath = DEFAULT_CONFIG_PATH);

	static AppConfig loadConfig(const wxFileName& filePath = DEFAULT_CONFIG_PATH);
};

// const std::map<AppConfig::ConfigKey, wxString> AppConfig::CONFIG_KEY_NAMES = { { RUN_AT_STARTUP, wxT("runAtStartup") } };
