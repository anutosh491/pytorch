#pragma once

#include <ATen/MapAllocator.h>
#include <c10/macros/Export.h>

#ifdef __cplusplus

C10_EXPORT void libshm_init(const char* manager_exec_path);

// Superclass to run a constructor before at::RefcountedMapAllocator
class C10_EXPORT THManagedMapAllocatorInit {
 protected:
  THManagedMapAllocatorInit(const char* manager_handle, const char* filename);
  std::string manager_handle_;
};

// Like a at::RefcountedMapAllocator, but it also makes use of an external
// shared memory manager process to ensure that shared memory regions actually
// get freed in the end (even if processes lose the memory).
class C10_EXPORT THManagedMapAllocator : private THManagedMapAllocatorInit,
                                        public at::RefcountedMapAllocator {
 public:
  THManagedMapAllocator(
      const char* manager_handle,
      const char* filename,
      int flags,
      size_t size);

  void close() override;

  ~THManagedMapAllocator() override {
    close();
  }

  static at::DataPtr makeDataPtr(
      const char* manager_handle,
      const char* filename,
      int flags,
      size_t size);
  static THManagedMapAllocator* fromDataPtr(const at::DataPtr& /*dptr*/);

  const char* manager_handle() const {
    return manager_handle_.c_str();
  }
};

#endif
