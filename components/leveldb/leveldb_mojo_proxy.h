// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_LEVELDB_MOJO_PROXY_H_
#define COMPONENTS_LEVELDB_LEVELDB_MOJO_PROXY_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"

namespace leveldb {

// A proxy for thread safe access to Mojo objects from multiple threads.
//
// MojoEnv is an object passed to the LevelDB implementation which can be
// called from multiple threads. Mojo pipes are bound to a single
// thread. Because of this mismatch, we create a proxy object which will
// redirect calls to the thread which owns the Mojo pipe, sends and receives
// messages.
//
// All public methods can be accessed from any thread.
class LevelDBMojoProxy : public base::RefCountedThreadSafe<LevelDBMojoProxy> {
 public:
  explicit LevelDBMojoProxy(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // A private struct to hide the underlying file that holds the lock from our
  // callers, forcing them to go through our LockFile()/UnlockFile() interface
  // so that they don't try to use the underlying pointer from an unsafe thread.
  struct OpaqueLock;

  // A private struct to hide the underlying root directory that we're
  // operating in. LevelDBMojoProxy will want to own all the directory
  // pointers, so while opening a database, we pass the directory to the thread
  // it will be operated on.
  struct OpaqueDir;

  // Passes ownership of a |directory| to the other thread, giving a reference
  // handle back to the caller.
  OpaqueDir* RegisterDirectory(filesystem::DirectoryPtr directory);
  void UnregisterDirectory(OpaqueDir* dir);

  // Synchronously calls Directory.OpenFileHandle().
  base::File OpenFileHandle(OpaqueDir* dir,
                            const std::string& name,
                            uint32_t open_flags);

  // Synchronously syncs |directory_|.
  filesystem::FileError SyncDirectory(OpaqueDir* dir, const std::string& name);

  // Synchronously checks whether |name| exists.
  bool FileExists(OpaqueDir* dir, const std::string& name);

  // Synchronously returns the filenames of all files in |path|.
  filesystem::FileError GetChildren(OpaqueDir* dir,
                                    const std::string& path,
                                    std::vector<std::string>* result);

  // Synchronously deletes |path|.
  filesystem::FileError Delete(OpaqueDir* dir,
                               const std::string& path,
                               uint32_t delete_flags);

  // Synchronously creates |path|.
  filesystem::FileError CreateDir(OpaqueDir* dir, const std::string& path);

  // Synchronously gets the size of a file.
  filesystem::FileError GetFileSize(OpaqueDir* dir,
                                    const std::string& path,
                                    uint64_t* file_size);

  // Synchronously renames a file.
  filesystem::FileError RenameFile(OpaqueDir* dir,
                                   const std::string& old_path,
                                   const std::string& new_path);

  // Synchronously locks a file. Returns both the file return code, and if OK,
  // an opaque object to the lock to enforce going through this interface to
  // unlock the file so that unlocking happens on the correct thread.
  std::pair<filesystem::FileError, OpaqueLock*> LockFile(
      OpaqueDir* dir,
      const std::string& path);

  // Unlocks a file. LevelDBMojoProxy takes ownership of lock. (We don't make
  // this a scoped_ptr because exporting the ctor/dtor for this struct publicly
  // defeats the purpose of the struct.)
  filesystem::FileError UnlockFile(OpaqueLock* lock);

 private:
  friend class base::RefCountedThreadSafe<LevelDBMojoProxy>;
  ~LevelDBMojoProxy();

  void RunInternal(const base::Closure& task);

  void DoOnOtherThread(const base::Closure& c, base::WaitableEvent* event);

  // Implementation methods of the public interface. Depending on whether they
  // were called from the thread that |task_runner_| is, these might be run
  // on the current thread or through PostTask().
  void RegisterDirectoryImpl(
      mojo::InterfacePtrInfo<filesystem::Directory> directory_info,
      OpaqueDir** out_dir);
  void UnregisterDirectoryImpl(OpaqueDir* dir);
  void OpenFileHandleImpl(OpaqueDir* dir,
                          std::string name,
                          uint32_t open_flags,
                          base::File* out_file);
  void SyncDirectoryImpl(OpaqueDir* dir,
                         std::string name,
                         filesystem::FileError* out_error);
  void FileExistsImpl(OpaqueDir* dir,
                      std::string name,
                      bool* exists);
  void GetChildrenImpl(OpaqueDir* dir,
                       std::string name,
                       std::vector<std::string>* contents,
                       filesystem::FileError* out_error);
  void DeleteImpl(OpaqueDir* dir,
                  std::string name,
                  uint32_t delete_flags,
                  filesystem::FileError* out_error);
  void CreateDirImpl(OpaqueDir* dir,
                     std::string name,
                     filesystem::FileError* out_error);
  void GetFileSizeImpl(OpaqueDir* dir,
                       const std::string& path,
                       uint64_t* file_size,
                       filesystem::FileError* out_error);
  void RenameFileImpl(OpaqueDir* dir,
                      const std::string& old_path,
                      const std::string& new_path,
                      filesystem::FileError* out_error);
  void LockFileImpl(OpaqueDir* dir,
                    const std::string& path,
                    filesystem::FileError* out_error,
                    OpaqueLock** out_lock);
  void UnlockFileImpl(scoped_ptr<OpaqueLock> lock,
                      filesystem::FileError* out_error);

  // The task runner which represents the thread that all mojo objects are
  // bound to.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  int outstanding_opaque_dirs_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBMojoProxy);
};

}  // namespace leveldb

#endif  // COMPONENTS_LEVELDB_LEVELDB_MOJO_PROXY_H_
