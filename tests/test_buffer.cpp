#include "mcp/network/buffer.h"

#include <gtest/gtest.h>
#include <string>

namespace {

TEST(BufferTest, InitialState) {
    mcp::Buffer buf;
    EXPECT_EQ(buf.readableBytes(), 0u);
    EXPECT_GE(buf.writableBytes(), 0u);
    EXPECT_EQ(buf.prependableBytes(), mcp::Buffer::kPrependSize);
}

TEST(BufferTest, AppendAndRetrieve) {
    mcp::Buffer buf;
    std::string data = "Hello, World!";

    buf.append(data);
    EXPECT_EQ(buf.readableBytes(), data.size());
    EXPECT_EQ(buf.retrieveAsString(data.size()), data);
    EXPECT_EQ(buf.readableBytes(), 0u);
}

TEST(BufferTest, PartialRetrieve) {
    mcp::Buffer buf;
    buf.append("ABCDEFGH");

    EXPECT_EQ(buf.retrieveAsString(3), "ABC");
    EXPECT_EQ(buf.readableBytes(), 5u);
    EXPECT_EQ(buf.retrieveAllAsString(), "DEFGH");
}

TEST(BufferTest, FindCRLF) {
    mcp::Buffer buf;
    buf.append("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");

    ssize_t pos = buf.findCRLF();
    ASSERT_GE(pos, 0);
    EXPECT_EQ(buf.retrieveAsString(static_cast<size_t>(pos)), "GET / HTTP/1.1");
    buf.retrieve(2); // 消费 CRLF

    pos = buf.findCRLF();
    ASSERT_GE(pos, 0);
    EXPECT_EQ(buf.retrieveAsString(static_cast<size_t>(pos)), "Host: localhost");
}

TEST(BufferTest, FindCRLFNotFound) {
    mcp::Buffer buf;
    buf.append("No line ending here");
    EXPECT_EQ(buf.findCRLF(), -1);
}

TEST(BufferTest, Prepending) {
    mcp::Buffer buf;
    buf.append("World");

    // 在 "World" 前前置 "Hello, "
    buf.prepend("Hello, ", 7);
    EXPECT_EQ(buf.readableBytes(), 12u);
    EXPECT_EQ(buf.retrieveAllAsString(), "Hello, World");
}

TEST(BufferTest, LargeData) {
    mcp::Buffer buf;
    std::string large(10000, 'X');
    buf.append(large);

    EXPECT_EQ(buf.readableBytes(), 10000u);
    EXPECT_EQ(buf.retrieveAllAsString(), large);
}

TEST(BufferTest, RetrieveUntil) {
    mcp::Buffer buf;
    buf.append("abc:def:ghi");

    const char* colon = static_cast<const char*>(std::memchr(buf.peek(), ':', 8));
    ASSERT_NE(colon, nullptr);
    buf.retrieveUntil(colon);
    EXPECT_EQ(buf.readableBytes(), 8u); // 剩余 ":def:ghi"
    EXPECT_EQ(buf.retrieveAsString(1), ":");
}

TEST(BufferTest, Clear) {
    mcp::Buffer buf;
    buf.append("some data");
    buf.clear();
    EXPECT_EQ(buf.readableBytes(), 0u);
    EXPECT_EQ(buf.prependableBytes(), mcp::Buffer::kPrependSize);
}

TEST(BufferTest, RepeatedAppend) {
    mcp::Buffer buf;
    for (int i = 0; i < 1000; ++i) {
        buf.append("a");
    }
    EXPECT_EQ(buf.readableBytes(), 1000u);
    buf.retrieveAll();
    EXPECT_EQ(buf.readableBytes(), 0u);

    // retrieve 后仍应可写
    for (int i = 0; i < 500; ++i) {
        buf.append("b");
    }
    EXPECT_EQ(buf.readableBytes(), 500u);
}

} // anonymous namespace
