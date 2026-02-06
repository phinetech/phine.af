#pragma once

#include <string>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

namespace af::test {

/**
 * Manages loading and templating of test fixtures
 */
class TestFixtureLoader {
public:
    explicit TestFixtureLoader(const std::string& fixtures_base_path);

    /**
     * Load a JSON fixture file
     */
    nlohmann::json LoadFixture(const std::string& relative_path);

    /**
     * Load a fixture and apply template variables
     * Example: LoadFixtureWithVars("qod/create_session.json", {{"ueId", "12345"}})
     */
    nlohmann::json LoadFixtureWithVars(
        const std::string& relative_path,
        const std::map<std::string, std::string>& variables
    );

    /**
     * Load all fixtures from a directory
     */
    std::map<std::string, nlohmann::json> LoadFixturesFromDirectory(
        const std::string& directory
    );

    /**
     * Get the raw JSON string from a fixture
     */
    std::string LoadFixtureAsString(const std::string& relative_path);

private:
    std::string fixtures_base_path_;
    
    std::string ApplyTemplateVariables(
        const std::string& content,
        const std::map<std::string, std::string>& variables
    );
};

} // namespace af::test