#pragma once

#include <cstdint>
#include <string>

#define FUSE_USE_VERSION FUSE_MAKE_VERSION(3, 14)
#include <fuse3/fuse_lowlevel.h>

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace getrafty::myfs {

class Server {
 public:
  explicit Server(const std::string& port, const std::string& mount_point_)
      : port_(port), mount_point_(mount_point_) {
    auto root = std::make_shared<FileNode>(true);
    inode_map_[1] = std::move(root);
  }

  void run();

 private:
  const std::string& port_;
  const std::string& mount_point_;

  struct FileNode {
    explicit FileNode(const bool is_directory) : is_directory_(is_directory){};

    const bool is_directory_;
    std::vector<char> data_{};
    std::unordered_map<std::string, fuse_ino_t> children_{};
  };

  std::mutex fs_mutex_;
  std::unordered_map<fuse_ino_t, std::shared_ptr<FileNode>> inode_map_;

  fuse_ino_t allocate_inode(std::shared_ptr<FileNode> node) {
    static std::atomic<fuse_ino_t> next_file_inode_{2};
    static std::atomic<fuse_ino_t> next_dir_inode_{3};

    fuse_ino_t ino;
    if (node->is_directory_) {
      ino = next_dir_inode_.fetch_add(2);
    } else {
      ino = next_file_inode_.fetch_add(2);
    }
    inode_map_[ino] = node;
    return ino;
  }

  std::shared_ptr<FileNode> get_node(fuse_ino_t ino) {
    auto it = inode_map_.find(ino);
    return (it != inode_map_.end()) ? it->second : nullptr;
  }

  // FUSE API:

  void getattr(fuse_req_t req, fuse_ino_t ino,  // NOLINT
               struct fuse_file_info* fi);

  void statfs(fuse_req_t req, fuse_ino_t ino);  // NOLINT

  void readdir(fuse_req_t req, fuse_ino_t ino, size_t size,  // NOLINT
               off_t off, struct fuse_file_info* fi);

  void lookup(fuse_req_t req, fuse_ino_t parent,  // NOLINT
              const char* name);

  void create(fuse_req_t req, fuse_ino_t parent,  // NOLINT
              const char* name, mode_t mode, struct fuse_file_info* fi);

  void mknod(fuse_req_t req, fuse_ino_t parent,  // NOLINT
             const char* name, mode_t mode, dev_t rdev);

  void open(fuse_req_t req, fuse_ino_t ino,  // NOLINT
            struct fuse_file_info* fi);

  void read(fuse_req_t req, fuse_ino_t ino, size_t size,  // NOLINT
            off_t off, struct fuse_file_info* fi);

  void write(fuse_req_t req, fuse_ino_t ino,  // NOLINT
             const char* buf, size_t size, off_t off,
             struct fuse_file_info* fi);

  void setattr(fuse_req_t req, fuse_ino_t ino,  // NOLINT
               struct stat* attr, int to_set, struct fuse_file_info* fi);

  void unlink(fuse_req_t req, fuse_ino_t parent,  // NOLINT
              const char* name);

  void mkdir(fuse_req_t req, fuse_ino_t parent,  // NOLINT
             const char* name, mode_t mode);
};

}  // namespace getrafty::myfs