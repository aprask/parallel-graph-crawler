#include <iostream>
#include <iostream>
#include <curl/curl.h>
#include <string.h>
#include <cstring>
#include <vector>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <unordered_set>
#include <thread>
#include "rapidjson/document.h"

#define ARGS 3

std::string base_url = "http://hollywood-graph-crawler.bridgesuncc.org/neighbors/";

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
std::string req_hand(std::string url, std::string& name);
void parse_json(const std::string& res_bod, std::unordered_set<std::string>* visited, std::vector<std::string>* next_level);
void crawl(std::string* initial_name, size_t& depth, std::unordered_set<std::string>* visited);

int main (int argc, char** argv) {
    if (argc != ARGS) {
        std::cerr << "Entered <" << argc << ">. Expected <" << ARGS << ">" << std::endl;
        return 1;
    }
    std::string name = argv[1];
    std::replace(name.begin(), name.end(), '_', ' ');
    size_t depth = std::stol(argv[2]);

    std::unordered_set<std::string> visited;

    crawl(&name, depth, &visited);
    return 0;
}

void crawl(std::string* initial_name, size_t& depth, std::unordered_set<std::string>* visited) {
    std::vector<std::vector<std::string>> levels;
    std::ofstream output_file("output.txt", std::ios::trunc);
    visited->insert(*initial_name);
    levels.push_back({*initial_name});
    auto global_time = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < depth; ++i) {
        levels.push_back({});
        for (std::string node : levels[i]) {
            std::string out = req_hand(base_url, node);
            output_file << node << std::endl;
            parse_json(out, visited, &levels[i+1]);
        }
    }
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - global_time).count();
    output_file << "Time: " << duration << std::endl;
    output_file.close();
}

void parse_json(const std::string& res_bod, std::unordered_set<std::string>* visited, std::vector<std::string>* next_level) {
    rapidjson::Document doc;
    doc.Parse(res_bod.c_str());
    bool res = doc.HasMember("neighbors");
    if (!res) {
        std::cerr << "Cannot locate neighbors key" << std::endl;
        exit(1);
    }
    res = doc.HasMember("node");
    if (!res) {
        std::cerr << "Cannot locate node key" << std::endl;
        exit(1);
    }
    visited->insert(doc["node"].GetString());
    const rapidjson::Value& neighbors = doc["neighbors"];
    for (size_t i = 0; i < neighbors.Size(); ++i) {
        if (visited->find(neighbors[i].GetString()) != visited->end()) continue;
        visited->insert(neighbors[i].GetString());
        next_level->push_back(neighbors[i].GetString());
    }
}


size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    std::string* str = (std::string*)userdata;
    for (size_t i = 0; i < nmemb; ++i) {
        str->push_back(ptr[i]);
    }
    return nmemb;
}

std::string req_hand(std::string url, std::string& name) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to make handle" << std::endl;
        exit(1);
    }
    CURLcode response;
    char* encoded_name = curl_easy_escape(curl, name.c_str(), name.size());
    if (encoded_name == NULL) {
        std::cerr << "Cannot encode url" << std::endl;
        exit(1);
    }
    url = url + encoded_name;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    std::string out;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &out);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    response = curl_easy_perform(curl);
    if (response) {
        std::cerr << "error: " << curl_easy_strerror(response) << std::endl;
        exit(1); 
    }
    curl_easy_cleanup(curl);
    return out;
}