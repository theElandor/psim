#include <iostream>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;
namespace fs = std::filesystem;

/*
  Author: Matteo Lugli
  AI enhanced code to send requests to scryfall and get card information
  in JSON format with local caching support.
*/

class ScryfallAPI {
private:
    CURL* curlJson;
    CURL* curlImage;
    std::string baseUrl = "https://api.scryfall.com";
    std::string cacheDir = "../data";
    std::string jsonDir;
    std::string imageDir;

public:
    ScryfallAPI() {
        curl_global_init(CURL_GLOBAL_DEFAULT);

        // Setup cache directories
        jsonDir = cacheDir + "/json";
        imageDir = cacheDir + "/images";
        
        // Create directories if they don't exist
        try {
            fs::create_directories(jsonDir);
            fs::create_directories(imageDir);
            std::cout << "Cache directories initialized: " << cacheDir << std::endl;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error creating cache directories: " << e.what() << std::endl;
        }

        // JSON handle
        curlJson = curl_easy_init();
        if (curlJson) {
            curl_easy_setopt(curlJson, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curlJson, CURLOPT_USERAGENT, "ScryfallCppClient/1.0");
        }

        // Image handle
        curlImage = curl_easy_init();
        if (curlImage) {
            curl_easy_setopt(curlImage, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curlImage, CURLOPT_USERAGENT, "ScryfallCppClient/1.0");
        }
    }

    ~ScryfallAPI() {
        if (curlJson)  curl_easy_cleanup(curlJson);
        if (curlImage) curl_easy_cleanup(curlImage);
        curl_global_cleanup();
    }

    static size_t WriteStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t totalSize = size * nmemb;
        std::string* str = static_cast<std::string*>(userp);
        str->append((char*)contents, totalSize);
        return totalSize;
    }

    static size_t WriteVectorCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t totalSize = size * nmemb;
        auto* buffer = static_cast<std::vector<unsigned char>*>(userp);
        buffer->insert(buffer->end(),
                       (unsigned char*)contents,
                       (unsigned char*)contents + totalSize);
        return totalSize;
    }

    // Helper function to sanitize filename
    std::string sanitizeFilename(const std::string& name) {
        std::string sanitized = name;
        // Replace invalid filename characters with underscores
        const std::string invalidChars = "\\/:*?\"<>|";
        for (char& c : sanitized) {
            if (invalidChars.find(c) != std::string::npos || c < 32) {
                c = '_';
            }
        }
        return sanitized;
    }

    // Generate cache filename based on card name or ID
    std::string generateCacheKey(const std::string& identifier) {
        // Create a hash-like key from the identifier
        std::hash<std::string> hasher;
        size_t hash = hasher(identifier);
        
        std::stringstream ss;
        ss << std::hex << hash;
        return ss.str();
    }

    // Load JSON from cache
    std::string loadJsonFromCache(const std::string& cacheKey) {
        std::string filepath = jsonDir + "/" + cacheKey + ".json";
        
        if (!fs::exists(filepath)) {
            return "";
        }

        std::ifstream file(filepath);
        if (!file.is_open()) {
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::cout << "Loaded JSON from cache: " << cacheKey << std::endl;
        return buffer.str();
    }

    // Save JSON to cache
    void saveJsonToCache(const std::string& cacheKey, const std::string& jsonData) {
        std::string filepath = jsonDir + "/" + cacheKey + ".json";
        
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << jsonData;
            std::cout << "Saved JSON to cache: " << cacheKey << std::endl;
        } else {
            std::cerr << "Failed to save JSON to cache: " << filepath << std::endl;
        }
    }

    // Load image from cache
    std::vector<unsigned char> loadImageFromCache(const std::string& cacheKey) {
        std::string filepath = imageDir + "/" + cacheKey + ".png";
        
        std::vector<unsigned char> buffer;
        if (!fs::exists(filepath)) {
            return buffer;
        }

        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return buffer;
        }

        // Get file size
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Read file into buffer
        buffer.resize(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        
        std::cout << "Loaded image from cache: " << cacheKey << std::endl;
        return buffer;
    }

    // Save image to cache
    void saveImageToCache(const std::string& cacheKey, const std::vector<unsigned char>& imageData) {
        std::string filepath = imageDir + "/" + cacheKey + ".png";
        
        std::ofstream file(filepath, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
            std::cout << "Saved image to cache: " << cacheKey << std::endl;
        } else {
            std::cerr << "Failed to save image to cache: " << filepath << std::endl;
        }
    }

    std::string makeRequest(const std::string& endpoint) {
        if (!curlJson) {
            return "Error: CURL initialization failed";
        }

        std::string response;
        std::string url = baseUrl + endpoint;
        std::cout<<url<<std::endl;

        curl_easy_setopt(curlJson, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curlJson, CURLOPT_WRITEFUNCTION, WriteStringCallback);
        curl_easy_setopt(curlJson, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curlJson);
        if (res != CURLE_OK) {
            return "Error: " + std::string(curl_easy_strerror(res));
        }

        long responseCode;
        curl_easy_getinfo(curlJson, CURLINFO_RESPONSE_CODE, &responseCode);
        if (responseCode != 200) {
            return "HTTP Error: " + std::to_string(responseCode) + "\n" + response;
        }

        return response;
    }

    std::vector<unsigned char> downloadImage(const std::string& url) {
        std::vector<unsigned char> buffer;

        if (!curlImage) return buffer;

        curl_easy_setopt(curlImage, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curlImage, CURLOPT_WRITEFUNCTION, WriteVectorCallback);
        curl_easy_setopt(curlImage, CURLOPT_WRITEDATA, &buffer);

        CURLcode res = curl_easy_perform(curlImage);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: "
                      << curl_easy_strerror(res) << "\n";
        }

        return buffer;
    }

    std::string urlEncode(const std::string& value) {
      std::ostringstream escaped;
      escaped.fill('0');
      escaped << std::hex;
      for (unsigned char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
          escaped << c;
        } else {
          escaped << '%' << std::uppercase << std::setw(2) << int(c);
          escaped << std::nouppercase;
        }
      }
      return escaped.str();
    }

    std::string trim(const std::string& s) {
        size_t start = 0;
        while (start < s.size() && (s[start] == '\r' || s[start] == '\n')) {
            ++start;
        }
        size_t end = s.size();
        while (end > start && (s[end - 1] == '\r' || s[end - 1] == '\n')) {
            --end;
        }
        return s.substr(start, end - start);
    }

    // Enhanced method with caching for card by name
    std::string getCardByName(const std::string& rawCardName) {
        std::string cardName = trim(rawCardName); 
        std::string cacheKey = generateCacheKey("name_" + cardName);
        std::string cachedData = loadJsonFromCache(cacheKey);
        if (!cachedData.empty()) {
            return cachedData;
        }
        std::string encodedName = urlEncode(cardName);
        std::string response = makeRequest("/cards/named?fuzzy=" + encodedName);
        if (response.find("Error:") != 0 && response.find("HTTP Error:") != 0) {
          saveJsonToCache(cacheKey, response);
        }
        return response; 
    }

    // Enhanced method with caching for random card
    std::string getRandomCard() {
        // For random cards, we'll still make API calls since they should be random
        // But we can cache them with a timestamp-based key for potential reuse
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::string cacheKey = "random_" + std::to_string(time_t / 3600); // Cache for 1 hour
        
        // Don't load from cache for random cards to maintain randomness
        std::string response = makeRequest("/cards/random");
        
        // Save to cache if request was successful
        if (response.find("Error:") != 0 && response.find("HTTP Error:") != 0) {
            // Extract card ID and cache with that key for future direct access
            try {
                auto j = json::parse(response);
                if (j.contains("id")) {
                    std::string cardId = j["id"].get<std::string>();
                    std::string idCacheKey = generateCacheKey("id_" + cardId);
                    saveJsonToCache(idCacheKey, response);
                }
            } catch (const json::exception& e) {
                std::cerr << "Error parsing JSON for caching: " << e.what() << std::endl;
            }
        }
        
        return response;
    }

    // Enhanced method with caching for search
    std::string searchCards(const std::string& query) {
        std::string cacheKey = generateCacheKey("search_" + query);
        
        // Try to load from cache first
        std::string cachedData = loadJsonFromCache(cacheKey);
        if (!cachedData.empty()) {
            return cachedData;
        }

        // If not in cache, make API request
        std::string encodedQuery = query;
        size_t pos = 0;
        while ((pos = encodedQuery.find(" ", pos)) != std::string::npos) {
            encodedQuery.replace(pos, 1, "%20");
            pos += 3;
        }
        
        std::string response = makeRequest("/cards/search?q=" + encodedQuery);
        
        // Save to cache if request was successful
        if (response.find("Error:") != 0 && response.find("HTTP Error:") != 0) {
            saveJsonToCache(cacheKey, response);
        }
        
        return response;
    }

    // Enhanced method with caching for card by ID
    std::string getCardById(const std::string& id) {
        std::string cacheKey = generateCacheKey("id_" + id);
        
        // Try to load from cache first
        std::string cachedData = loadJsonFromCache(cacheKey);
        if (!cachedData.empty()) {
            return cachedData;
        }

        // If not in cache, make API request
        std::string response = makeRequest("/cards/" + id);
        
        // Save to cache if request was successful
        if (response.find("Error:") != 0 && response.find("HTTP Error:") != 0) {
            saveJsonToCache(cacheKey, response);
        }
        
        return response;
    }

    // Enhanced image download with caching
    std::vector<unsigned char> downloadImageCached(const std::string& url) {
        // Generate cache key from URL
        std::string cacheKey = generateCacheKey(url);
        
        // Try to load from cache first
        std::vector<unsigned char> cachedImage = loadImageFromCache(cacheKey);
        if (!cachedImage.empty()) {
            return cachedImage;
        }

        // If not in cache, download from URL
        std::vector<unsigned char> imageData = downloadImage(url);
        
        // Save to cache if download was successful
        if (!imageData.empty()) {
            saveImageToCache(cacheKey, imageData);
        }
        
        return imageData;
    }

    // Updated helper methods that work with cached data

    std::string getCardImageURL(const std::string& jsonString) {
        auto j = json::parse(jsonString);

        // Handle double-faced / modal cards
        if (j.contains("card_faces") && j["card_faces"].is_array() && !j["card_faces"].empty()) {
            const auto& frontFace = j["card_faces"][0];
            if (frontFace.contains("image_uris") && frontFace["image_uris"].contains("png")) {
                return frontFace["image_uris"]["png"].get<std::string>();
            }
        }

        // Handle single-faced cards
        if (j.contains("image_uris") && j["image_uris"].contains("png")) {
            return j["image_uris"]["png"].get<std::string>();
        }

        return "";
    }

    std::string getCardName(const std::string& jsonString) {
        auto j = json::parse(jsonString);
        if (j.contains("name")) {
            return j["name"].get<std::string>();
        }
        return "";
    }

    unsigned getCardCmc(const std::string& jsonString) {
        auto j = json::parse(jsonString);
        if (j.contains("cmc")) {
            return j["cmc"].get<int>();
        }
        return 0;
    }

     std::string getCardType(const std::string& jsonString) {
        auto j = json::parse(jsonString);
        if (j.contains("type_line")) {
            return j["type_line"].get<std::string>();
        }
        return "";
    }

    // Utility methods for cache management
    void clearCache() {
        try {
            fs::remove_all(cacheDir);
            fs::create_directories(jsonDir);
            fs::create_directories(imageDir);
            std::cout << "Cache cleared successfully." << std::endl;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error clearing cache: " << e.what() << std::endl;
        }
    }

    size_t getCacheSize() {
        size_t totalSize = 0;
        try {
            for (const auto& entry : fs::recursive_directory_iterator(cacheDir)) {
                if (entry.is_regular_file()) {
                    totalSize += entry.file_size();
                }
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error calculating cache size: " << e.what() << std::endl;
        }
        return totalSize;
    }

    void printCacheStats() {
        size_t jsonCount = 0, imageCount = 0;
        
        try {
            for (const auto& entry : fs::directory_iterator(jsonDir)) {
                if (entry.is_regular_file()) jsonCount++;
            }
            for (const auto& entry : fs::directory_iterator(imageDir)) {
                if (entry.is_regular_file()) imageCount++;
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error reading cache stats: " << e.what() << std::endl;
        }

        std::cout << "Cache Statistics:" << std::endl;
        std::cout << "  JSON files: " << jsonCount << std::endl;
        std::cout << "  Image files: " << imageCount << std::endl;
        std::cout << "  Total size: " << (getCacheSize() / 1024.0 / 1024.0) << " MB" << std::endl;
    }
};

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << title << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}
