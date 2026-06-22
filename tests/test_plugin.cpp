#include "mcp/plugin/plugin_manager.h"
#include "mcp/tool/tool_manager.h"

#include <gtest/gtest.h>

namespace {

TEST(PluginManagerTest, LoadFromEmptyDirectory) {
    mcp::PluginManager pm;
    mcp::ToolManager tm;

    int count = pm.loadFromDirectory("/tmp", tm);
    EXPECT_GE(count, 0); // May be 0 if no .so files

    pm.unloadAll();
    EXPECT_EQ(pm.loadedCount(), 0u);
}

TEST(PluginManagerTest, LoadFromNonexistentDirectory) {
    mcp::PluginManager pm;
    mcp::ToolManager tm;

    int count = pm.loadFromDirectory("/tmp/nonexistent_dir_abc123", tm);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(pm.loadedCount(), 0u);
}

TEST(PluginManagerTest, UnloadAllOnEmptyManager) {
    mcp::PluginManager pm;
    EXPECT_EQ(pm.loadedCount(), 0u);
    pm.unloadAll(); // Should not crash
    EXPECT_EQ(pm.loadedCount(), 0u);
}

TEST(PluginManagerTest, LoadedCountInitial) {
    mcp::PluginManager pm;
    EXPECT_EQ(pm.loadedCount(), 0u);
}

} // anonymous namespace
