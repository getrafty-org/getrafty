#include <iostream>

#include <myfs_server.hpp>
#include <sstream>



namespace getrafty::myfs {

void Server::run() {
  const char* fuse_argv[20];

  int fuse_argc = 0;
  fuse_argv[fuse_argc++] = "myfs";                // Name of the filesystem
  fuse_argv[fuse_argc++] = "-d";                  // Debug flag
  fuse_argv[fuse_argc++] = mount_point_.c_str();  // Mount point

#ifdef __APPLE__
  fuse_argv[fuse_argc++] = "-o";
  fuse_argv[fuse_argc++] = "nolocalcaches";  // no dir entry caching
  fuse_argv[fuse_argc++] = "-o";
  fuse_argv[fuse_argc++] = "daemon_timeout=86400";
#endif

  // Validate the mount point
  if (!mount_point_.empty() && mount_point_[0] != '/') {
    throw std::invalid_argument(
        "Invalid mount point: must be an absolute path");
  }

  fuse_args args = FUSE_ARGS_INIT(fuse_argc, const_cast<char**>(fuse_argv));

  fuse_cmdline_opts opts = {};
  if (fuse_parse_cmdline(&args, &opts) == -1) {
    throw std::invalid_argument("fuse_parse_cmdline failed");
  }

  fuse_lowlevel_ops ops{};

  ops.getattr = [](fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->getattr(req, ino, fi);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.lookup = [](fuse_req_t req, fuse_ino_t parent, const char* name) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->lookup(req, parent, name);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.readdir = [](fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                   struct fuse_file_info* fi) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->readdir(req, ino, size, off, fi);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.create = [](fuse_req_t req, fuse_ino_t parent, const char* name,
                  mode_t mode, struct fuse_file_info* fi) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->create(req, parent, name, mode, fi);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.unlink = [](fuse_req_t req, fuse_ino_t parent, const char* name) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->unlink(req, parent, name);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.open = [](fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->open(req, ino, fi);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.read = [](fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                struct fuse_file_info* fi) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->read(req, ino, size, off, fi);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.write = [](fuse_req_t req, fuse_ino_t ino, const char* buf, size_t size,
                 off_t off, struct fuse_file_info* fi) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->write(req, ino, buf, size, off, fi);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.statfs = [](fuse_req_t req, fuse_ino_t ino) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->statfs(req, ino);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.setattr = [](fuse_req_t req, fuse_ino_t ino, struct stat* attr,
                   int to_set, struct fuse_file_info* fi) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->setattr(req, ino, attr, to_set, fi);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.mkdir = [](fuse_req_t req, fuse_ino_t parent, const char* name,
                 mode_t mode) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->mkdir(req, parent, name, mode);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.rmdir = [](fuse_req_t req, fuse_ino_t parent, const char* name) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->rmdir(req, parent, name);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.readdir = [](fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                   fuse_file_info* fi) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->readdir(req, ino, size, off, fi);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  ops.getxattr = [](fuse_req_t req, fuse_ino_t ino, const char* name,
                    size_t size) {
    if (auto* self = static_cast<Server*>(fuse_req_userdata(req))) {
      self->getxattr(req, ino, name, size);
    } else {
      fuse_reply_err(req, EIO);
    }
  };

  auto* se = fuse_session_new(&args, &ops, sizeof(ops), this);
  if (!se) {
    throw std::invalid_argument("fuse_session_new failed");
  }

  if (fuse_session_mount(se, mount_point_.c_str()) == -1) {
    fuse_session_destroy(se);
    throw std::invalid_argument("fuse_session_mount failed");
  }

  if (fuse_session_loop(se) == -1) {
    throw std::runtime_error("fuse_session_loop failed");
  }

  fuse_session_unmount(se);
  fuse_session_destroy(se);
}

void Server::lookup(fuse_req_t req, fuse_ino_t parent, const char* name) {
  fuse_entry_param e{};

  // std::lock_guard lock(fs_mutex_);

  const auto parent_node = findInode(parent);
  if (!parent_node || !parent_node->is_directory_) {
    fuse_reply_err(req, ENOTDIR);
    return;
  }

  auto it = parent_node->children_.find(name);
  if (it == parent_node->children_.end()) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  fuse_ino_t child_ino = it->second;
  const auto child_node = findInode(child_ino);

  e.ino = child_ino;
  e.attr.st_mode =
      child_node->is_directory_ ? (S_IFDIR | 0755) : (S_IFREG | 0644);
  e.attr.st_nlink =
      child_node->is_directory_ ? 2 + child_node->children_.size() : 1;
  e.attr.st_size = child_node->data_.size();
  e.attr_timeout = 1.0;
  e.entry_timeout = 1.0;

  fuse_reply_entry(req, &e);
}

void Server::create(fuse_req_t req, fuse_ino_t parent, const char* name,
                    mode_t mode, fuse_file_info* fi) {
  // std::lock_guard lock(fs_mutex_);

  const auto parent_node = findInode(parent);
  if (!parent_node || !parent_node->is_directory_) {
    fuse_reply_err(req, ENOTDIR);
    return;
  }

  if (parent_node->children_.contains(name)) {
    fuse_reply_err(req, EEXIST);
    return;
  }

  const auto new_ino = allocateInode(parent, false, name);

  fuse_entry_param e{};
  e.ino = new_ino;
  e.attr.st_mode = S_IFREG | (mode & 0777);  // File permissions
  e.attr.st_nlink = 1;
  e.attr.st_size = 0;
  e.attr_timeout = 1.0;
  e.entry_timeout = 1.0;

  fuse_reply_create(req, &e, fi);
}

void Server::unlink(fuse_req_t req, fuse_ino_t parent, const char* name) {
  // std::lock_guard lock(fs_mutex_);

  const auto parent_node = findInode(parent);
  if (!parent_node || !parent_node->is_directory_) {
    fuse_reply_err(req, ENOTDIR);
    return;
  }

  auto it = parent_node->children_.find(name);
  if (it == parent_node->children_.end()) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  fuse_ino_t file_ino = it->second;
  inode_map_.erase(file_ino);
  parent_node->children_.erase(it);

  fuse_reply_err(req, 0);
}

void Server::open(fuse_req_t req, fuse_ino_t ino, fuse_file_info* fi) {
  // std::lock_guard lock(fs_mutex_);

  const auto node = findInode(ino);
  if (!node) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  if (node->is_directory_) {
    fuse_reply_err(req, EISDIR);
    return;
  }

  fuse_reply_open(req, fi);
}

void Server::readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                     fuse_file_info* /*fi*/) {
  // std::lock_guard lock(fs_mutex_);

  const auto node = findInode(ino);
  if (!node || !node->is_directory_) {
    fuse_reply_err(req, ENOTDIR);
    return;
  }

  std::vector<char> buf;
  buf.reserve(size);

  size_t pos = 0;

  // Add `.` entry
  if (off == 0) {
    struct stat st {};
    st.st_ino = ino;
    st.st_mode = S_IFDIR | 0755;
    pos += fuse_add_direntry(req, buf.data() + pos, size - pos, ".", &st, 1);
  }

  // Add `..` entry
  if (off <= 1) {
    struct stat st {};
    st.st_ino = node->parent_ino_;
    st.st_mode = S_IFDIR | 0755;
    pos += fuse_add_direntry(req, buf.data() + pos, size - pos, "..", &st, 2);
  }

  int current_offset = 2;  // Start after `.` and `..`
  for (const auto& [name, child_ino] : node->children_) {
    if (current_offset >= off) {
      auto child_node = findInode(child_ino);
      struct stat st {};
      st.st_ino = child_ino;
      st.st_mode = child_node->is_directory_ ? S_IFDIR : S_IFREG;
      pos += fuse_add_direntry(req, buf.data() + pos, size - pos, name.c_str(),
                               &st, current_offset + 1);

      if (pos >= size) {
        break;
      }
    }
    current_offset++;
  }

  fuse_reply_buf(req, buf.data(), pos);
}

void Server::write(fuse_req_t req, fuse_ino_t ino, const char* buf, size_t size,
                   off_t off, fuse_file_info* /*fi*/) {
  std::lock_guard lock(fs_mutex_);

  const auto node = findInode(ino);
  if (!node) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  if (node->is_directory_) {
    fuse_reply_err(req, EISDIR);
    return;
  }

  size_t write_end = static_cast<size_t>(off) + size;
  if (write_end > node->data_.size()) {
    node->data_.resize(write_end, '\0');
  }

  // Write data to the file buffer
  std::memcpy(node->data_.data() + off, buf, size);

  fuse_reply_write(req, size);
}


void Server::statfs(fuse_req_t req, fuse_ino_t /*ino*/) {

  // Calculate used space
  size_t used_space = 0;
  for (const auto& [ino, node] : inode_map_) {
    if (!node->is_directory_) {
      used_space += node->data_.size();
    }
  }

  // Free space is the remaining space
  const size_t free_space =
      TOTAL_SPACE > used_space ? TOTAL_SPACE - used_space : 0;

  struct statvfs stat {};
  stat.f_bsize = BLOCK_SIZE;                 // Filesystem block size
  stat.f_frsize = BLOCK_SIZE;                // Fragment size
  stat.f_blocks = TOTAL_SPACE / BLOCK_SIZE;  // Total blocks
  stat.f_bfree = free_space / BLOCK_SIZE;    // Free blocks
  stat.f_bavail =
      free_space / BLOCK_SIZE;       // Free blocks for unprivileged users
  stat.f_files = inode_map_.size();  // Total inodes
  stat.f_ffree = (TOTAL_SPACE / BLOCK_SIZE) - inode_map_.size();  // Free inodes
  stat.f_namemax = NAME_MAX;  // Maximum filename length

  fuse_reply_statfs(req, &stat);
}

void Server::getattr(fuse_req_t req, fuse_ino_t ino, fuse_file_info* /*fi*/) {
  // std::lock_guard lock(fs_mutex_);

  const auto node = findInode(ino);
  if (!node) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  struct stat stat {};
  stat.st_ino = ino;
  stat.st_mode = node->is_directory_ ? (S_IFDIR | 0755)
                                     : (S_IFREG | 0644);  // File or directory
  stat.st_nlink = node->is_directory_ ? 2 + node->children_.size() : 1;
  stat.st_size = node->data_.size();

  fuse_reply_attr(req, &stat, 1.0);
}

void Server::read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                  fuse_file_info* /*fi*/) {
  // std::lock_guard lock(fs_mutex_);

  const auto node = findInode(ino);
  if (!node) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  if (node->is_directory_) {
    fuse_reply_err(req, EISDIR);
    return;
  }

  // Determine how much data can be read starting from `off`
  if (off >= static_cast<off_t>(node->data_.size())) {
    fuse_reply_buf(req, nullptr, 0);  // End of file
    return;
  }

  size = std::min(size, node->data_.size() - static_cast<size_t>(off));

  // Return the requested data
  fuse_reply_buf(req, node->data_.data() + off, size);
}

void Server::mkdir(fuse_req_t req, fuse_ino_t parent, const char* name,
                   mode_t mode) {
  // std::lock_guard lock(fs_mutex_);

  const auto parent_node = findInode(parent);
  if (!parent_node || !parent_node->is_directory_) {
    fuse_reply_err(req, ENOTDIR);
    return;
  }

  if (parent_node->children_.contains(name)) {
    fuse_reply_err(req, EEXIST);
    return;
  }

  const auto new_ino = allocateInode(parent, true, name);

  fuse_entry_param e{};
  e.ino = new_ino;
  e.attr.st_mode = S_IFDIR | (mode & 0777);  // Directory permissions
  e.attr.st_nlink = 2;  // Directories have at least 2 links
  e.attr_timeout = 1.0;
  e.entry_timeout = 1.0;

  fuse_reply_entry(req, &e);
}

void Server::rmdir(fuse_req_t req, fuse_ino_t parent, const char* name) {
  // std::lock_guard lock(fs_mutex_);

  auto parent_node = findInode(parent);
  if (!parent_node || !parent_node->is_directory_) {
    fuse_reply_err(req, ENOTDIR);
    return;
  }

  auto it = parent_node->children_.find(name);
  if (it == parent_node->children_.end()) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  const fuse_ino_t child_ino = it->second;
  auto child_node = findInode(child_ino);

  if (!child_node || !child_node->is_directory_) {
    fuse_reply_err(req, ENOTDIR);
    return;
  }

  if (!child_node->children_.empty()) {
    fuse_reply_err(req, ENOTEMPTY);
    return;
  }

  forgetInode(child_ino);

  fuse_reply_err(req, 0);
}

void Server::setattr(fuse_req_t req, fuse_ino_t ino, struct stat* attr,
                     int to_set, fuse_file_info* /*fi*/) {
  // std::lock_guard lock(fs_mutex_);

  const auto node = findInode(ino);
  if (!node) {
    fuse_reply_err(req, ENOENT);  // No such file or directory
    return;
  }

  // Modify file size (truncate)
  if (to_set & FUSE_SET_ATTR_SIZE) {
    if (node->is_directory_) {
      fuse_reply_err(req, EISDIR);  // Cannot truncate a directory
      return;
    }

    if (attr->st_size < 0) {
      fuse_reply_err(req, EINVAL);  // Invalid size
      return;
    }

    node->data_.resize(attr->st_size);  // Resize the file
  }

  // Modify permissions (chmod)
  if (to_set & FUSE_SET_ATTR_MODE) {
    // Update permissions (only an example, as we're not enforcing permissions in-memory)
    attr->st_mode =
        (node->is_directory_ ? S_IFDIR : S_IFREG) | (attr->st_mode & 0777);
  }

  // Modify other attributes (timestamps, owner, group, etc.)
  // Here we ignore them because this in-memory FS does not maintain them.

  // Reply with updated attributes
  struct stat stbuf {};
  stbuf.st_ino = ino;
  stbuf.st_mode = node->is_directory_ ? (S_IFDIR | 0755) : (S_IFREG | 0644);
  stbuf.st_nlink = node->is_directory_ ? 2 + node->children_.size() : 1;
  stbuf.st_size = node->data_.size();
  fuse_reply_attr(req, &stbuf, 1.0);
}

void Server::getxattr(fuse_req_t req, fuse_ino_t /*ino*/, const char* /*name*/,
                      size_t /*size*/) {
  fuse_reply_err(req, ENOTSUP);
}

}  // namespace getrafty::myfs