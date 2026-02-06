#include "test_fixture_loader.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>
#include <stdexcept>

namespace af::test {

TestFixtureLoader::TestFixtureLoader(const std::string& fixtures_base_path)
    : fixtures_base_path_(fixtures_base_path) {
    
    if (!std::filesystem::exists(fixtures_base_path_)) {
        throw std::runtime_error("Fixtures path does not exist: " + fixtures_base_path_);
    }
}

nlohmann::json TestFixtureLoader::LoadFixture(const std::string& relative_path) {
    std::string full_path = fixtures_base_path_ + "/" + relative_path;
    
    std::ifstream file(full_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open fixture file: " + full_path);
    }
    
    nlohmann::json json_data;
    try {
        file >> json_data;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Failed to parse JSON from fixture: " + 
                                full_path + " - " + e.what());
    }
    
    return json_data;
}

std::string TestFixtureLoader::ApplyTemplateVariables(
    const std::string& content,
    const std::map<std::string, std::string>& variables) {
    
    std::string result = content;
    
    // Replace {{variable}} patterns
    for (const auto& [key, value] : variables) {
        std::string pattern = "{{" + key + "}}";
        size_t pos = 0;
        while ((pos = result.find(pattern, pos)) != std::string::npos) {
            result.replace(pos, pattern.length(), value);
            pos += value.length();
        }
    }
    
    return result;
}

nlohmann::json TestFixtureLoader::LoadFixtureWithVars(
    const std::string& relative_path,
    const std::map<std::string, std::string>& variables) {
    
    std::string content = LoadFixtureAsString(relative_path);
    std::string processed = ApplyTemplateVariables(content, variables);
    
    return nlohmann::json::parse(processed);
}

std::map<std::string, nlohmann::json> TestFixtureLoader::LoadFixturesFromDirectory(
    const std::string& directory) {
    
    std::string full_path = fixtures_base_path_ + "/" + directory;
    std::map<std::string, nlohmann::json> fixtures;
    
    for (const auto& entry : std::filesystem::directory_iterator(full_path)) {
        if (entry.path().extension() == ".json") {
            std::string filename = entry.path().filename().string();
            fixtures[filename] = LoadFixture(directory + "/" + filename);
        }
    }
    
    return fixtures;
}

std::string TestFixtureLoader::LoadFixtureAsString(const std::string& relative_path) {
    std::string full_path = fixtures_base_path_ + "/" + relative_path;
    
    std::ifstream file(full_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open fixture file: " + full_path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace af::test