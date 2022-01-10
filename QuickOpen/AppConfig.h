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
            destination = static_cast<const T&>(config.at(key));
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

	// bool runAtStartup = false;

	wxString browserID;
	wxString customBrowserPath;

	bool saveUseLastFolder = true;
	wxFileName fileSavePath;

	WithStaticDefault<unsigned, 8080> serverPort;

	static inline wxFileName defaultConfigPath()
	{
		return InstallationInfo::detectInstallation().configFolder / wxFileName(wxT("."), wxT("config.json"));
	}

	void saveConfig(const wxFileName& filePath = wxFileName());

	static AppConfig loadConfig(const wxFileName& filePath = wxFileName());
};

// const std::map<AppConfig::ConfigKey, wxString> AppConfig::CONFIG_KEY_NAMES = { { RUN_AT_STARTUP, wxT("runAtStartup") } };
