#include <iostream>
#include <string>
#include <curl/curl.h>

/*
  Author: Matteo Lugli
  AI generated code to send requests to scryfall and get card information
  in JSON format. Feels like it's working pretty well. Generated with
  Claude Sonnet 4.
*/

// Callback function to write response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append((char*)contents, totalSize);
    return totalSize;
}

class ScryfallAPI {
private:
    CURL* curl;
    std::string baseUrl = "https://api.scryfall.com";

public:
    ScryfallAPI() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
        
        if (curl) {
            // Set common options
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "ScryfallCppClient/1.0");
        }
    }

    ~ScryfallAPI() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
    }

    std::string makeRequest(const std::string& endpoint) {
        if (!curl) {
            return "Error: CURL initialization failed";
        }

        std::string response;
        std::string url = baseUrl + endpoint;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            return "Error: " + std::string(curl_easy_strerror(res));
        }

        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        
        if (responseCode != 200) {
            return "HTTP Error: " + std::to_string(responseCode) + "\n" + response;
        }

        return response;
    }

    // Get a card by name
    std::string getCardByName(const std::string& cardName) {
        std::string encodedName = cardName;
        // Simple URL encoding for spaces (replace with proper encoding in production)
        size_t pos = 0;
        while ((pos = encodedName.find(" ", pos)) != std::string::npos) {
            encodedName.replace(pos, 1, "%20");
            pos += 3;
        }
        return makeRequest("/cards/named?exact=" + encodedName);
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

int main() {
    std::cout << "Scryfall API C++ Example\n";

    ScryfallAPI api;

    // Example 1: Get a specific card by name
    printSeparator("Getting 'Lightning Bolt' by name:");
    std::string lightningBolt = api.getCardByName("Lightning Bolt");
    std::cout << lightningBolt << std::endl;
    /* Some more AI generated examples:
      // Example 2: Get a random card
       printSeparator("Getting a random card:");
       std::string randomCard = api.getRandomCard();
       std::cout << randomCard << std::endl;

       // Example 3: Search for cards
       printSeparator("Searching for blue creatures:");
       std::string searchResults = api.searchCards("c:blue t:creature");
       std::cout << searchResults << std::endl;

       // Example 4: Get card by specific ID (Black Lotus example)
       printSeparator("Getting Black Lotus by Scryfall ID:");
       std::string blackLotus = api.getCardById("bd8fa327-dd41-4737-8f19-2cf5eb1f7cdd");
       std::cout << blackLotus << std::endl;
    */
   
    return 0;
}

// Compilation instructions:
// On Ubuntu/Debian: sudo apt-get install libcurl4-openssl-dev
// On macOS with Homebrew: brew install curl
// On Windows: Download and install curl development libraries
//
// Compile with: g++ -std=c++11 -o scryfall_example scryfall_example.cpp -lcurl
// Run with: ./scryfall_example
