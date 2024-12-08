
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include <gtest/gtest.h>
#include <myfs_server.hpp>

using namespace getrafty::myfs;
namespace fs = std::filesystem;

class ServerTest : public ::testing::Test {
 protected:
  std::unique_ptr<Server> server;
  std::unique_ptr<std::thread> server_thread;
  std::string mount_point = "/var/tmp/fuse_test";
  std::string port = "localhost:8080";

  void SetUp() override {
    // Ensure the mount point exists
    if (!fs::exists(mount_point)) {
      fs::create_directory(mount_point);
    }

    // Start the server in a separate thread
    server = std::make_unique<Server>(port, mount_point);
    server_thread = std::make_unique<std::thread>([&]() { EXPECT_NO_THROW(server->run());});

    // Allow some time for the server to initialize
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  void TearDown() override {
    // Unmount and stop the server
    std::system(("fusermount -u " + mount_point).c_str());
    if (server_thread->joinable()) {
      server_thread->join();
    }

    // Clean up the mount point
    if (fs::exists(mount_point)) {
      fs::remove_all(mount_point);
    }
  }
};

TEST_F(ServerTest, CreateAndWriteFile) {
  // File creation and write test
  std::string test_file = mount_point + "/test_file.txt";
  std::ofstream ofs(test_file);
  ASSERT_TRUE(ofs.is_open());
  ofs << "Hello, FUSE!";
  ofs.close();

  // Verify the file exists and contains the expected data
  ASSERT_TRUE(fs::exists(test_file));
  std::ifstream ifs(test_file);
  std::string content;
  std::getline(ifs, content);
  ASSERT_EQ(content, "Hello, FUSE!");
}

TEST_F(ServerTest, CreateAndRemoveDirectory) {
  // Directory creation test
  std::string test_dir = mount_point + "/test_dir";
  ASSERT_TRUE(fs::create_directory(test_dir));
  ASSERT_TRUE(fs::exists(test_dir));

  // Directory removal test
  fs::remove(test_dir);
  ASSERT_FALSE(fs::exists(test_dir));
}

TEST_F(ServerTest, ReadNonExistentFile) {
  // Attempt to read a non-existent file
  std::string non_existent_file = mount_point + "/does_not_exist.txt";
  std::ifstream ifs(non_existent_file);
  ASSERT_FALSE(ifs.is_open());
}

TEST_F(ServerTest, ListDirectoryContents) {
  // Create files and directories
  std::string test_dir = mount_point + "/test_dir";
  fs::create_directory(test_dir);
  std::string test_file = test_dir + "/test_file.txt";
  std::ofstream ofs(test_file);
  ofs << "File inside directory";
  ofs.close();

  // List directory contents
  std::vector<std::string> entries;
  for (const auto& entry : fs::directory_iterator(test_dir)) {
    entries.push_back(entry.path().filename().string());
  }

  ASSERT_EQ(entries.size(), 1);
  ASSERT_EQ(entries[0], "test_file.txt");
}
