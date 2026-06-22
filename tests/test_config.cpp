#include "mcp/config/config_manager.h"

#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>

namespace {

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_path_ = "/tmp/solarmcp_test_config.yaml";
    }

    void TearDown() override {
        std::remove(config_path_.c_str());
    }

    void writeYaml(const std::string& content) {
        std::ofstream out(config_path_);
        out << content;
        out.close();
    }

    std::string config_path_;
};

TEST_F(ConfigTest, LoadYamlConfig) {
    writeYaml(R"(
server:
  host: "127.0.0.1"
  port: 9090
logging:
  level: "debug"
  async: true
thread_pool:
  worker_threads: 8
tools:
  read_file:
    allowed_paths:
      - "/tmp"
      - "/var/log"
)");

    auto& config = mcp::ConfigManager::getInstance();
    EXPECT_TRUE(config.loadFile(config_path_));

    EXPECT_EQ(config.getString("server.host"), "127.0.0.1");
    EXPECT_EQ(config.getInt("server.port"), 9090);
    EXPECT_EQ(config.getString("logging.level"), "debug");
    EXPECT_TRUE(config.getBool("logging.async"));
    EXPECT_EQ(config.getInt("thread_pool.worker_threads"), 8);

    auto paths = config.getStringArray("tools.read_file.allowed_paths");
    ASSERT_EQ(paths.size(), 2u);
    EXPECT_EQ(paths[0], "/tmp");
    EXPECT_EQ(paths[1], "/var/log");
}

TEST_F(ConfigTest, DefaultValues) {
    writeYaml("server:\n  host: \"0.0.0.0\"\n");

    auto& config = mcp::ConfigManager::getInstance();
    EXPECT_TRUE(config.loadFile(config_path_));

    EXPECT_EQ(config.getString("server.host"), "0.0.0.0");
    EXPECT_EQ(config.getInt("server.port", 8090), 8090); // default
    EXPECT_EQ(config.getString("nonexistent", "fallback"), "fallback");
    EXPECT_FALSE(config.getBool("nonexistent"));
}

TEST_F(ConfigTest, HasKey) {
    writeYaml("server:\n  host: \"localhost\"\n  port: 8080\n");

    auto& config = mcp::ConfigManager::getInstance();
    EXPECT_TRUE(config.loadFile(config_path_));

    EXPECT_TRUE(config.has("server.host"));
    EXPECT_TRUE(config.has("server.port"));
    EXPECT_FALSE(config.has("server.timeout"));
    EXPECT_FALSE(config.has("nonexistent.key"));
}

TEST_F(ConfigTest, FilePath) {
    writeYaml("server:\n  host: \"0.0.0.0\"\n");

    auto& config = mcp::ConfigManager::getInstance();
    EXPECT_TRUE(config.loadFile(config_path_));
    EXPECT_EQ(config.filePath(), config_path_);
}

TEST_F(ConfigTest, MissingFile) {
    auto& config = mcp::ConfigManager::getInstance();
    EXPECT_FALSE(config.loadFile("/tmp/nonexistent_config_12345.yaml"));
}

} // anonymous namespace
