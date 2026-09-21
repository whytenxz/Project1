#pragma once
#include <string>
#include <vector>

namespace cloud_config {
    inline constexpr const char* kApiBaseUrl = "https://gamesense.ink/api";
    inline constexpr const char* kApiSecret = "Z28gZnVjayB5b3Vyc2VsZiA8Mw==";

    struct CloudConfigEntry {
        int id;
        std::string name;
        std::string author;
        int likes;
        int downloads;
    };

    enum class CloudConfigResult {
        Success,
        NetworkError,
        AuthError,
        NotFound,
        InvalidData
    };

    bool Initialize();
    const std::string& GetToken();
    const std::string& GetUsername();

    CloudConfigResult FetchList(std::vector<CloudConfigEntry>& out);
    CloudConfigResult FetchConfig(int config_id, std::string& out_data);
    CloudConfigResult UploadConfig(const std::string& name, const std::string& desc, const std::string& data);
    CloudConfigResult DeleteConfig(int config_id);
}
