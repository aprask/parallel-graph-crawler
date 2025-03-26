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
#include <mutex>
#include <functional>
#include "rapidjson/document.h"

#define ARGS 4

std::string base_url = "http://hollywood-graph-crawler.bridgesuncc.org/neighbors/";
std::mutex visited_mtx;
 
size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
std::string req_hand(std::string url, std::string& name);
void parse_json(const std::string& res_bod, std::unordered_set<std::string>* visited, std::vector<std::string>* next_level);
void crawl(std::string* initial_name, size_t& depth);
void crawl_parallel(std::string* initial_name, size_t& depth, const size_t& thread_cap);
void node_routine(std::unordered_set<std::string>* visited, std::vector<std::string> thread_nodes, std::string url, std::vector<std::string>* next_level);
void parse_json_parallel(const std::string& res_bod, std::unordered_set<std::string>* visited, std::vector<std::string>* next_level);

int main (int argc, char** argv) {
    if (argc != ARGS) {
        std::cerr << "Entered <" << argc << ">. Expected <" << ARGS << ">" << std::endl;
        return 1;
    }
    std::string name = argv[1];
    std::replace(name.begin(), name.end(), '_', ' ');
    size_t depth = std::stol(argv[2]);
    const size_t max_threads = std::stol(argv[3]);
    if (max_threads == 0) {
        crawl(&name, depth);
    } else {
        crawl_parallel(&name, depth, max_threads);
    }
    return 0;
}

void crawl_parallel(std::string* initial_name, size_t& depth, const size_t& thread_cap) {
    std::unordered_set<std::string> visited;
    std::vector<std::thread> threads;
    std::vector<std::vector<std::string>> levels;
    std::vector<std::string> thread_nodes;
    size_t thread_dist;
    size_t thread_c;
    size_t node_c;
    std::ofstream output_file("output.txt", std::ios::trunc);

    visited.insert(*initial_name);
    levels.push_back({*initial_name});
    auto global_time = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < depth; ++i) {
        output_file << "Level " << i << std::endl;
        levels.push_back({});
        node_c = levels[i].size();
        thread_c = thread_cap;
        for (std::string node : levels[i]) {
            thread_dist = node_c / thread_c + node_c % thread_c;
            output_file << node << std::endl;
            thread_nodes.push_back(node);
            if (thread_nodes.size() == thread_dist) {
                node_c = node_c - thread_dist;
                threads.push_back(std::thread(node_routine, &visited, thread_nodes, base_url, &levels[i+1]));
                thread_c--;
                thread_nodes.clear();
            }
        }
        for (int j = 0; j < threads.size(); j++) if (threads[j].joinable()) threads[j].join();
        threads.clear();
    }
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - global_time).count();
    output_file << "Time: " << duration << std::endl;
    output_file.close();
}

void node_routine(std::unordered_set<std::string>* visited, std::vector<std::string> thread_nodes, std::string url, std::vector<std::string>* next_level) {
    std::string out;
    for (std::string thread_node : thread_nodes) {
        out = req_hand(base_url, thread_node);
        parse_json_parallel(out, visited, next_level);
    }
}

void parse_json_parallel(const std::string& res_bod, std::unordered_set<std::string>* visited, std::vector<std::string>* next_level) {
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
    visited_mtx.lock();
    visited->insert(doc["node"].GetString());
    const rapidjson::Value& neighbors = doc["neighbors"];
    for (size_t i = 0; i < neighbors.Size(); ++i) {
        if (visited->find(neighbors[i].GetString()) != visited->end()) continue;
        visited->insert(neighbors[i].GetString());
        next_level->push_back(neighbors[i].GetString());
    }
    visited_mtx.unlock();
}

void crawl(std::string* initial_name, size_t& depth) {
    std::unordered_set<std::string> visited;
    std::vector<std::vector<std::string>> levels;
    std::ofstream output_file("output.txt", std::ios::trunc);
    visited.insert(*initial_name);
    levels.push_back({*initial_name});
    auto global_time = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < depth; ++i) {
        output_file << "Level " << i << std::endl;
        levels.push_back({});
        for (std::string node : levels[i]) {
            std::string out = req_hand(base_url, node);
            output_file << node << std::endl;
            parse_json(out, &visited, &levels[i+1]);
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
    std::string out;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &out);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: C++-Client/1.0");
    if (!headers) {
        curl_slist_free_all(headers);
        std::cerr << "error: failed to create headers" << std::endl;
        exit(1); 
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    response = curl_easy_perform(curl);
    if (response != CURLE_OK) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        std::cerr << "error: " << curl_easy_strerror(response) << std::endl;
        exit(1); 
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return out;
}