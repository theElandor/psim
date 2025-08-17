#include <iostream>
#include <string>
#include <curl/curl.h>

/*
  Author: Matteo Lugli
  AI generated code to send requests to scryfall and get card information
  in JSON format. Feels like it's working pretty well. Generated with
  Claude Sonnet 4.
*/

size_t WriteStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append((char*)contents, totalSize);
    return totalSize;
}

size_t WriteVectorCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    auto* buffer = static_cast<std::vector<unsigned char>*>(userp);
    buffer->insert(buffer->end(),
                   (unsigned char*)contents,
                   (unsigned char*)contents + totalSize);
    return totalSize;
}

class ScryfallAPI {
private:
    CURL* curlJson;
    CURL* curlImage;
    std::string baseUrl = "https://api.scryfall.com";

public:
    ScryfallAPI() {
      curl_global_init(CURL_GLOBAL_DEFAULT);

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

    std::string makeRequest(const std::string& endpoint) {
      if (!curlJson) {
          return "Error: CURL initialization failed";
      }

      std::string response;
      std::string url = baseUrl + endpoint;

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


    // Get a card by name
    std::string getCardByName(const std::string& cardName) {
      std::string encodedName = cardName;
      for (size_t pos = 0; (pos = encodedName.find(' ', pos)) != std::string::npos; ++pos) {
          encodedName.replace(pos, 1, "%20");
      }
      return makeRequest("/cards/named?fuzzy=" + encodedName);
    }

    // Get a random card
    std::string getRandomCard() {
        return makeRequest("/cards/random");
    }

    // Search for cards
    std::string searchCards(const std::string& query) {
      std::string encodedQuery = query;
      // Simple URL encoding for spaces
      size_t pos = 0;
      while ((pos = encodedQuery.find(" ", pos)) != std::string::npos) {
        encodedQuery.replace(pos, 1, "%20");
        pos += 3;
      }
      return makeRequest("/cards/search?q=" + encodedQuery);
    }

    // Get card by Scryfall ID
    std::string getCardById(const std::string& id) {
      return makeRequest("/cards/" + id);
    }
};

void printSeparator(const std::string& title) {
  std::cout << "\n" << std::string(50, '=') << std::endl;
  std::cout << title << std::endl;
  std::cout << std::string(50, '=') << std::endl;
}
