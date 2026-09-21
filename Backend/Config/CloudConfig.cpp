#include "CloudConfig.h"
#include <Windows.h>
#include <winhttp.h>
#include <ShlObj.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

namespace cloud_config {
    namespace {
        std::string g_username;
        std::string g_token;

        std::string Trim(std::string value) {
            auto not_space = [](unsigned char c) { return !std::isspace(c); };
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
            value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
            return value;
        }

        std::string JsonEscape(const std::string& value) {
            std::string out;
            out.reserve(value.size() + 8);
            for (char c : value) {
                switch (c) {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
                }
            }
            return out;
        }

        std::string JsonGetString(const std::string& json, const char* key) {
            const std::string needle = std::string("\"") + key + "\"";
            size_t pos = json.find(needle);
            if (pos == std::string::npos)
                return {};

            pos = json.find(':', pos);
            if (pos == std::string::npos)
                return {};

            pos = json.find('"', pos);
            if (pos == std::string::npos)
                return {};

            ++pos;
            std::string out;
            bool escape = false;
            for (; pos < json.size(); ++pos) {
                const char c = json[pos];
                if (escape) {
                    out.push_back(c);
                    escape = false;
                    continue;
                }
                if (c == '\\') {
                    escape = true;
                    continue;
                }
                if (c == '"')
                    break;
                out.push_back(c);
            }
            return out;
        }

        bool JsonGetBool(const std::string& json, const char* key) {
            const std::string needle = std::string("\"") + key + "\"";
            size_t pos = json.find(needle);
            if (pos == std::string::npos)
                return false;

            pos = json.find(':', pos);
            if (pos == std::string::npos)
                return false;

            const size_t true_pos = json.find("true", pos);
            const size_t false_pos = json.find("false", pos);
            if (true_pos == std::string::npos)
                return false;
            if (false_pos != std::string::npos && false_pos < true_pos)
                return false;
            return true;
        }

        int SafeParseInt(const std::string& str, size_t start_pos) {
            int val = 0;
            size_t i = start_pos;
            while (i < str.size() && (std::isspace(static_cast<unsigned char>(str[i])) || str[i] == ':')) {
                i++;
            }
            bool negative = false;
            if (i < str.size() && str[i] == '-') {
                negative = true;
                i++;
            }
            while (i < str.size() && std::isdigit(static_cast<unsigned char>(str[i]))) {
                val = val * 10 + (str[i] - '0');
                i++;
            }
            return negative ? -val : val;
        }

        struct UrlParts {
            bool https = false;
            std::wstring host;
            INTERNET_PORT port = 0;
            std::wstring path;
        };

        bool ParseUrl(const std::string& url, UrlParts& out) {
            std::string work = url;
            if (work.rfind("https://", 0) == 0) {
                out.https = true;
                work = work.substr(8);
                out.port = INTERNET_DEFAULT_HTTPS_PORT;
            }
            else if (work.rfind("http://", 0) == 0) {
                out.https = false;
                work = work.substr(7);
                out.port = INTERNET_DEFAULT_HTTP_PORT;
            }
            else {
                return false;
            }

            const size_t slash = work.find('/');
            const std::string host_port = slash == std::string::npos ? work : work.substr(0, slash);
            std::string path = slash == std::string::npos ? "/" : work.substr(slash);

            const size_t colon = host_port.find(':');
            std::string host = colon == std::string::npos ? host_port : host_port.substr(0, colon);
            if (colon != std::string::npos) {
                out.port = static_cast<INTERNET_PORT>(std::stoi(host_port.substr(colon + 1)));
            }

            out.host = std::wstring(host.begin(), host.end());
            out.path = std::wstring(path.begin(), path.end());
            return !out.host.empty();
        }

        bool HttpPostJson(const std::string& endpoint, const std::string& json_body, std::string& response_body, DWORD& status_code) {
            UrlParts url{};
            if (!ParseUrl(std::string(kApiBaseUrl) + endpoint, url))
                return false;

            HINTERNET session = WinHttpOpen(L"gamesense-dll/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS, 0);
            if (!session)
                return false;

            HINTERNET connect = WinHttpConnect(session, url.host.c_str(), url.port, 0);
            if (!connect) {
                WinHttpCloseHandle(session);
                return false;
            }

            const DWORD flags = url.https ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET request = WinHttpOpenRequest(connect, L"POST", url.path.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            if (!request) {
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return false;
            }

            // Set timeouts to prevent DLL hang
            WinHttpSetTimeouts(request, 5000, 5000, 10000, 10000);

            const std::wstring secret(kApiSecret, kApiSecret + strlen(kApiSecret));
            const std::wstring headers =
                L"Content-Type: application/json\r\n"
                L"X-Api-Secret: " + secret + L"\r\n";

            const BOOL sent = WinHttpSendRequest(
                request,
                headers.c_str(),
                static_cast<DWORD>(headers.size()),
                const_cast<char*>(json_body.data()),
                static_cast<DWORD>(json_body.size()),
                static_cast<DWORD>(json_body.size()),
                0);

            if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return false;
            }

            DWORD status_size = sizeof(status_code);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);

            response_body.clear();
            DWORD available = 0;
            while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
                std::vector<char> buffer(available);
                DWORD read = 0;
                if (!WinHttpReadData(request, buffer.data(), available, &read))
                    break;
                response_body.append(buffer.data(), read);
            }

            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return true;
        }

        std::vector<CloudConfigEntry> ParseConfigList(const std::string& json) {
            std::vector<CloudConfigEntry> list;
            size_t array_start = json.find("[");
            if (array_start == std::string::npos)
                return list;

            size_t array_end = json.find("]", array_start);
            if (array_end == std::string::npos)
                return list;

            std::string array_content = json.substr(array_start + 1, array_end - array_start - 1);
            size_t pos = 0;
            while (true) {
                size_t obj_start = array_content.find("{", pos);
                if (obj_start == std::string::npos)
                    break;

                size_t obj_end = array_content.find("}", obj_start);
                if (obj_end == std::string::npos)
                    break;

                std::string obj_str = array_content.substr(obj_start, obj_end - obj_start + 1);
                CloudConfigEntry entry{};

                // Parse ID
                size_t id_pos = obj_str.find("\"id\"");
                if (id_pos != std::string::npos) {
                    entry.id = SafeParseInt(obj_str, id_pos + 4);
                }

                // Parse Name and Author
                entry.name = JsonGetString(obj_str, "name");
                entry.author = JsonGetString(obj_str, "author");

                // Parse Likes
                size_t likes_pos = obj_str.find("\"likes\"");
                if (likes_pos != std::string::npos) {
                    entry.likes = SafeParseInt(obj_str, likes_pos + 7);
                }

                // Parse Downloads
                size_t downloads_pos = obj_str.find("\"downloads\"");
                if (downloads_pos != std::string::npos) {
                    entry.downloads = SafeParseInt(obj_str, downloads_pos + 11);
                }

                list.push_back(entry);
                pos = obj_end + 1;
            }
            return list;
        }
    }

    bool Initialize() {
        char base[MAX_PATH]{};
        if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, base)))
            return false;

        std::string path = std::string(base) + "\\gamesense-cs2\\session.txt";
        std::ifstream file(path);
        if (!file)
            return false;

        std::getline(file, g_username);
        std::getline(file, g_token);
        g_username = Trim(g_username);
        g_token = Trim(g_token);

        return !g_token.empty();
    }

    const std::string& GetToken() {
        return g_token;
    }

    const std::string& GetUsername() {
        return g_username;
    }

    CloudConfigResult FetchList(std::vector<CloudConfigEntry>& out) {
        if (g_token.empty())
            return CloudConfigResult::AuthError;

        std::string body = "{\"action\":\"list\",\"token\":\"" + JsonEscape(g_token) + "\"}";
        std::string response;
        DWORD status = 0;

        if (!HttpPostJson("/configs.php", body, response, status))
            return CloudConfigResult::NetworkError;

        if (status == 401)
            return CloudConfigResult::AuthError;

        if (status != 200 || !JsonGetBool(response, "success"))
            return CloudConfigResult::InvalidData;

        out = ParseConfigList(response);
        return CloudConfigResult::Success;
    }

    CloudConfigResult FetchConfig(int config_id, std::string& out_data) {
        if (g_token.empty())
            return CloudConfigResult::AuthError;

        std::string body = "{\"action\":\"get\",\"token\":\"" + JsonEscape(g_token) + 
                           "\",\"config_id\":" + std::to_string(config_id) + "}";
        std::string response;
        DWORD status = 0;

        if (!HttpPostJson("/configs.php", body, response, status))
            return CloudConfigResult::NetworkError;

        if (status == 401)
            return CloudConfigResult::AuthError;
        if (status == 404)
            return CloudConfigResult::NotFound;

        if (status != 200 || !JsonGetBool(response, "success"))
            return CloudConfigResult::InvalidData;

        out_data = JsonGetString(response, "data");
        if (out_data.empty())
            return CloudConfigResult::InvalidData;

        return CloudConfigResult::Success;
    }

    CloudConfigResult UploadConfig(const std::string& name, const std::string& desc, const std::string& data) {
        if (g_token.empty())
            return CloudConfigResult::AuthError;

        std::string body = "{\"action\":\"upload\",\"token\":\"" + JsonEscape(g_token) + 
                           "\",\"name\":\"" + JsonEscape(name) + 
                           "\",\"description\":\"" + JsonEscape(desc) + 
                           "\",\"data\":\"" + JsonEscape(data) + "\"}";
        std::string response;
        DWORD status = 0;

        if (!HttpPostJson("/configs.php", body, response, status))
            return CloudConfigResult::NetworkError;

        if (status == 401)
            return CloudConfigResult::AuthError;

        if (status != 200 || !JsonGetBool(response, "success"))
            return CloudConfigResult::InvalidData;

        return CloudConfigResult::Success;
    }

    CloudConfigResult DeleteConfig(int config_id) {
        if (g_token.empty())
            return CloudConfigResult::AuthError;

        std::string body = "{\"action\":\"delete\",\"token\":\"" + JsonEscape(g_token) + 
                           "\",\"config_id\":" + std::to_string(config_id) + "}";
        std::string response;
        DWORD status = 0;

        if (!HttpPostJson("/configs.php", body, response, status))
            return CloudConfigResult::NetworkError;

        if (status == 401)
            return CloudConfigResult::AuthError;
        if (status == 404)
            return CloudConfigResult::NotFound;

        if (status != 200 || !JsonGetBool(response, "success"))
            return CloudConfigResult::InvalidData;

        return CloudConfigResult::Success;
    }
}
