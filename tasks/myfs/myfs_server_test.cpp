
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
  std::string mount_point = "/var/tmp/myfs-test";
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

TEST_F(ServerTest, CreateFileInRoot) {
  const std::string file_path = mount_point + "/test_file.txt";
  std::ofstream ofs(file_path);
  ASSERT_TRUE(ofs.is_open());
  ofs << "Hello, FUSE!";
  ofs.close();

  ASSERT_TRUE(fs::exists(file_path));
}

TEST_F(ServerTest, CreateDirectory) {
  std::string dir_path = mount_point + "/test_dir";
  ASSERT_TRUE(fs::create_directory(dir_path));

  ASSERT_TRUE(fs::is_directory(dir_path));
}

TEST_F(ServerTest, CreateFileInDirectory) {
  std::string dir_path = mount_point + "/test_dir";
  ASSERT_TRUE(fs::create_directory(dir_path));

  std::string file_path = dir_path + "/test_file.txt";
  std::ofstream ofs(file_path);
  ASSERT_TRUE(ofs.is_open());
  ofs << "Nested file content!";
  ofs.close();

  ASSERT_TRUE(fs::exists(file_path));
}


TEST_F(ServerTest, WriteAndReadFile) {
  std::string file_path = mount_point + "/test_file.txt";
  std::ofstream ofs(file_path);
  ASSERT_TRUE(ofs.is_open());
  ofs << "Sample content";
  ofs.close();

  std::ifstream ifs(file_path);
  ASSERT_TRUE(ifs.is_open());
  std::string content;
  std::getline(ifs, content);

  ASSERT_EQ(content, "Sample content");
}

TEST_F(ServerTest, Append) {
  std::string file_path = mount_point + "/test_file.txt";
  std::ofstream ofs(file_path);
  ASSERT_TRUE(ofs.is_open());
  ofs << "Short";
  ofs.close();

  std::ofstream ofs_append(file_path, std::ios::app);
  ASSERT_TRUE(ofs_append.is_open());
  ofs_append << " and extended!";
  ofs_append.close();

  std::ifstream ifs(file_path);
  ASSERT_TRUE(ifs.is_open());
  std::string content;
  std::getline(ifs, content);

  ASSERT_EQ(content, "Short and extended!");
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
  const std::string non_existent_file = mount_point + "/does_not_exist.txt";
  std::ifstream ifs(non_existent_file);
  ASSERT_FALSE(ifs.is_open());
}

TEST_F(ServerTest, AccessNonExistentFile) {
  const std::string file_path = mount_point + "/does_not_exist.txt";
  ASSERT_FALSE(fs::exists(file_path));

  std::ifstream ifs(file_path);
  ASSERT_FALSE(ifs.is_open());
}

TEST_F(ServerTest, CreateFileWithLongName) {
  const std::string long_name(std::string::size_type(255), 'a'); // Maximum filename length
  const std::string file_path = mount_point + "/" + long_name;
  std::ofstream ofs(file_path);
  ASSERT_TRUE(ofs.is_open());
  ofs.close();

  ASSERT_TRUE(fs::exists(file_path));
}

TEST_F(ServerTest, WriteToDirectory) {
  std::string dir_path = mount_point + "/test_dir";
  ASSERT_TRUE(fs::create_directory(dir_path));

  std::ofstream ofs(dir_path);
  ASSERT_FALSE(ofs.is_open()); // Writing to a directory should fail
}

TEST_F(ServerTest, RemoveNonEmptyDirectory) {
  std::string dir_path = mount_point + "/test_dir";
  ASSERT_TRUE(fs::create_directory(dir_path));

  std::string file_path = dir_path + "/test_file.txt";
  std::ofstream ofs(file_path);
  ASSERT_TRUE(ofs.is_open());
  ofs.close();

  ASSERT_ANY_THROW(fs::remove(dir_path));
  ASSERT_TRUE(fs::exists(dir_path));
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

TEST_F(ServerTest, CreateAndWriteLargeFile) {
  const size_t large_size = 10 * 1024 * 1024; // 10 MB
  std::string file_path = mount_point + "/large_file.txt";

  std::ofstream ofs(file_path, std::ios::binary);
  ASSERT_TRUE(ofs.is_open());

  std::vector<char> large_data(large_size, 'A');
  ofs.write(large_data.data(), large_size);
  ofs.close();

  ASSERT_TRUE(fs::exists(file_path));
  ASSERT_EQ(fs::file_size(file_path), large_size);
}

TEST_F(ServerTest, ReadLargeFile) {
  const size_t large_size = 10 * 1024 * 1024; // 10 MB
  std::string file_path = mount_point + "/large_file.txt";

  // Create a large file
  std::ofstream ofs(file_path, std::ios::binary);
  ASSERT_TRUE(ofs.is_open());
  std::vector<char> large_data(large_size, 'A');
  ofs.write(large_data.data(), large_size);
  ofs.close();

  // Read the file back
  std::ifstream ifs(file_path, std::ios::binary);
  ASSERT_TRUE(ifs.is_open());
  std::vector<char> read_data(large_size);
  ifs.read(read_data.data(), large_size);

  ASSERT_EQ(read_data, large_data);
}

TEST_F(ServerTest, DeeplyNestedDirectories) {
  const int depth = 100;
  std::string path = mount_point;

  for (int i = 0; i < depth; ++i) {
    path += "/dir_" + std::to_string(i);
    ASSERT_TRUE(fs::create_directory(path));
    ASSERT_TRUE(fs::is_directory(path));
  }

  // Verify the deepest directory
  ASSERT_TRUE(fs::exists(path));
  ASSERT_TRUE(fs::is_directory(path));
}

TEST_F(ServerTest, RemoveDeeplyNestedDirectories) {
  const int depth = 100;
  std::string path = mount_point;

  // Create nested directories
  for (int i = 0; i < depth; ++i) {
    path += "/dir_" + std::to_string(i);
    ASSERT_TRUE(fs::create_directory(path));
  }

  // Remove all directories from the deepest to the root
  for (int i = depth - 1; i >= 0; --i) {
    path = mount_point;
    for (int j = 0; j < i; ++j) {
      path += "/dir_" + std::to_string(j);
    }
    std::string dir_to_remove = path + "/dir_" + std::to_string(i);
    ASSERT_TRUE(fs::remove(dir_to_remove));
    ASSERT_FALSE(fs::exists(dir_to_remove));
  }
}

TEST_F(ServerTest, MaxFiles) {
  const int max_files = 10000;
  std::vector<std::string> file_paths;

  for (int i = 0; i < max_files; ++i) {
    std::string file_path = mount_point + "/file_" + std::to_string(i) + ".txt";
    file_paths.push_back(file_path);
    std::ofstream ofs(file_path);
    ASSERT_TRUE(ofs.is_open());
    ofs.close();
  }

  for (const auto& path : file_paths) {
    ASSERT_TRUE(fs::exists(path));
  }
}

TEST_F(ServerTest, DISABLED_ExceedFilesystemSpace) {
  constexpr size_t num_blocks = (TOTAL_SPACE / BLOCK_SIZE) + 10; // Exceed by one block
  const std::string file_path = mount_point + "/large_file.txt";
  std::ofstream ofs(file_path, std::ios::binary);
  ASSERT_TRUE(ofs.is_open());

  const std::vector<char> block(std::vector<char>::size_type(BLOCK_SIZE), 'A');
  size_t written_blocks = 0;

  for (size_t i = 0; i < num_blocks; ++i) {
    ofs.write(block.data(), BLOCK_SIZE);
    if (ofs.fail()) break;
    ++written_blocks;
  }

  ofs.close();

  // Verify we couldn't exceed the total space
  ASSERT_EQ(written_blocks, TOTAL_SPACE / BLOCK_SIZE + 1);
}

TEST_F(ServerTest, LseekAndWriteMidFile) {
  std::string file_path = mount_point + "/test_lseek_mid.txt";

  std::ofstream ofs(file_path);
  ASSERT_TRUE(ofs.is_open());
  ofs << std::string(std::string::size_type(20), 'A'); // Write 20 'A's
  ofs.close();

  ASSERT_EQ(fs::file_size(file_path), 20);

  std::fstream fs(file_path, std::ios::in | std::ios::out);
  ASSERT_TRUE(fs.is_open());

  fs.seekp(10, std::ios::beg);
  ASSERT_EQ(fs.tellp(), 10);
  fs.write("XYZ", 3);

  fs.close();

  // Verify file size remains the same
  ASSERT_EQ(fs::file_size(file_path), 20);

  // Verify file content
  std::ifstream ifs(file_path);
  ASSERT_TRUE(ifs.is_open());
  std::string content;
  std::getline(ifs, content);
  ASSERT_EQ(content, "AAAAAAAAAAXYZAAAAAAA");
}

TEST_F(ServerTest, TruncateOperation) {
  std::string file_path = mount_point + "/test_truncate.txt";

  std::ofstream ofs(file_path);
  ASSERT_TRUE(ofs.is_open());
  ofs << std::string(std::string::size_type(20), 'A'); // Write 20 'A's
  ofs.close();

  ASSERT_EQ(fs::file_size(file_path), 20);

  std::filesystem::resize_file(file_path, 10);

  ASSERT_EQ(fs::file_size(file_path), 10);

  std::ifstream ifs(file_path);
  ASSERT_TRUE(ifs.is_open());
  std::string content;
  std::getline(ifs, content);
  ASSERT_EQ(content, "AAAAAAAAAA"); // First 10 'A's should remain
}

TEST_F(ServerTest, FileAttributes) {
  std::string file_path = mount_point + "/test_file.txt";

  std::ofstream ofs(file_path);
  ASSERT_TRUE(ofs.is_open());
  ofs.close();

  struct stat st {};
  ASSERT_EQ(stat(file_path.c_str(), &st), 0);
  ASSERT_TRUE(S_ISREG(st.st_mode));
  ASSERT_EQ(st.st_size, 0);
}

TEST_F(ServerTest, DirectoryAttributes) {
  std::string dir_path = mount_point + "/test_dir";

  // Create the directory
  ASSERT_TRUE(fs::create_directory(dir_path));

  // Check attributes
  struct stat st {};
  ASSERT_EQ(stat(dir_path.c_str(), &st), 0);
  ASSERT_TRUE(S_ISDIR(st.st_mode));
  ASSERT_EQ(st.st_nlink, 2); // "." and parent ".."
}
