#pragma once

#include <cstdint>
#include <string>

#define FUSE_USE_VERSION FUSE_MAKE_VERSION(3, 14)
#include <fuse3/fuse_lowlevel.h>

#include <atomic>
#include <cassert>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace getrafty::myfs {

constexpr size_t BLOCK_SIZE = 4096;               // Block size in bytes
constexpr size_t TOTAL_SPACE = 8 * 1024 * 1024; // ~8MB

class Server {
 public:
  explicit Server(const std::string& port, const std::string& mount_point_)
      : port_(port), mount_point_(mount_point_) {
    auto root = std::make_shared<FileNode>(true, 1);
    inode_map_[1] = std::move(root);
  }

  void run();

 private:
  const std::string& port_;
  const std::string& mount_point_;

  struct FileNode {
    explicit FileNode(const bool is_directory, fuse_ino_t parent_ino)
        : is_directory_(is_directory), parent_ino_(parent_ino){};

    const bool is_directory_;
    const fuse_ino_t parent_ino_;
    std::vector<char> data_{};
    std::unordered_map<std::string, fuse_ino_t> children_{};
  };

  std::mutex fs_mutex_;
  std::unordered_map<fuse_ino_t, std::shared_ptr<FileNode>> inode_map_;

  fuse_ino_t allocateInode(fuse_ino_t parent, bool is_directory,
                           const char* name) {
    static std::atomic<fuse_ino_t> next_file_inode_{2};
    static std::atomic<fuse_ino_t> next_dir_inode_{3};

    fuse_ino_t ino;
    if (is_directory) {
      ino = next_dir_inode_.fetch_add(2);
    } else {
      ino = next_file_inode_.fetch_add(2);
    }

    assert(inode_map_[parent]->is_directory_);

    inode_map_[ino] = std::make_shared<FileNode>(is_directory, parent);
    inode_map_[parent]->children_[name] = ino;
    return ino;
  }

  std::shared_ptr<FileNode> findInode(const fuse_ino_t ino) {
    auto it = inode_map_.find(ino);
    return (it != inode_map_.end()) ? it->second : nullptr;
  }

  void forgetInode(const fuse_ino_t ino) {
    auto it = inode_map_.find(ino);
    if (it == inode_map_.end()) {
      return;
    }

    // If the node has a valid parent, remove it from the parent's children
    if (auto node = it->second; node->parent_ino_ != 0) {
      auto parent_it = inode_map_.find(node->parent_ino_);
      if (parent_it != inode_map_.end()) {
        auto& children = parent_it->second->children_;
        for (auto child_it = children.begin(); child_it != children.end(); ++child_it) {
          if (child_it->second == ino) {
            children.erase(child_it);
            break;
          }
        }
      }
    }

    inode_map_.erase(it);
  }



  // FUSE API:

  void getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi);

  void getxattr(fuse_req_t req, fuse_ino_t ino, const char* name,
                      size_t size);

  void statfs(fuse_req_t req, fuse_ino_t ino);

  void lookup(fuse_req_t req, fuse_ino_t parent, const char* name);

  void create(fuse_req_t req, fuse_ino_t parent, const char* name, mode_t mode,
              struct fuse_file_info* fi);

  void mknod(fuse_req_t req, fuse_ino_t parent, const char* name, mode_t mode,
             dev_t rdev);

  void open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi);

  void read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
            struct fuse_file_info* fi);

  void write(fuse_req_t req, fuse_ino_t ino, const char* buf, size_t size,
             off_t off, struct fuse_file_info* fi);

  void setattr(fuse_req_t req, fuse_ino_t ino, struct stat* attr, int to_set,
               struct fuse_file_info* fi);

  void unlink(fuse_req_t req, fuse_ino_t parent, const char* name);

  void mkdir(fuse_req_t req, fuse_ino_t parent, const char* name, mode_t mode);

  void rmdir(fuse_req_t req, fuse_ino_t parent, const char* name);

  void readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info* fi);
};

}  // namespace getrafty::myfs