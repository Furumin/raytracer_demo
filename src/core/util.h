#pragma once

namespace Util {

  // Platform
  struct File {
    HANDLE Handle = INVALID_HANDLE_VALUE;
    HANDLE MemMapHandle = INVALID_HANDLE_VALUE;
    u64 Size = 0;
    void *MappedMemory = NULL;
  };

  // -i/o
  enum {
    IO_FILE_READ  = 0x1,
    IO_FILE_WRITE = 0x2,
    IO_FILE_SHARE_READ = 0x4,
    IO_FILE_SHARE_WRITE = 0x8,
    IO_CREATE = 0x10,
    IO_CREATE_ONLY = 0x20,
    IO_OPEN = 0x40,
    IO_OPEN_ONLY = 0x80,
    IO_FLAG_OVERLAPPED = 0x100,
    IO_FLAG_NO_BUFFERING = 0x200,
    IO_FLAG_SEQ_ACCESS = 0x400,
    IO_FLAG_RANDOM_ACCESS = 0x800,
    IO_FLAG_MEM_MAP = 0x1000
  };
  
  inline int
  OpenFile(const char *path, u32 ioFlags, File *pHandleOut, u64 memMapWriteSize) {
    DWORD access = 0;
    if (ioFlags & IO_FILE_READ) access |= GENERIC_READ;
    if (ioFlags & IO_FILE_WRITE) access |= GENERIC_WRITE;
    DWORD share = 0;
    if (ioFlags & IO_FILE_SHARE_READ) share |= FILE_SHARE_READ;
    if (ioFlags & IO_FILE_SHARE_WRITE) share |= FILE_SHARE_WRITE;
    if (!share) share = FILE_SHARE_READ;
    DWORD creationDisp = 0;
    if (ioFlags & IO_CREATE) creationDisp |= CREATE_ALWAYS;
    if (ioFlags & IO_CREATE_ONLY) creationDisp |= CREATE_NEW;
    if (ioFlags & IO_OPEN) creationDisp |= OPEN_ALWAYS;
    if (ioFlags & IO_OPEN_ONLY) creationDisp |= OPEN_EXISTING;
    if (!creationDisp) creationDisp = OPEN_EXISTING; // mandatory
    DWORD flags = 0;
    if (ioFlags & IO_FLAG_OVERLAPPED) flags |= FILE_FLAG_OVERLAPPED;
    if (ioFlags & IO_FLAG_NO_BUFFERING) flags |= FILE_FLAG_NO_BUFFERING;
    if (ioFlags & IO_FLAG_SEQ_ACCESS) flags |= FILE_FLAG_SEQUENTIAL_SCAN;
    if (ioFlags & IO_FLAG_RANDOM_ACCESS) flags |= FILE_FLAG_RANDOM_ACCESS;

    if (ioFlags & IO_FLAG_MEM_MAP) {
      share = 0;
      access |= GENERIC_READ;
      access |= GENERIC_WRITE;
    }
    *pHandleOut = {};
    HANDLE f = CreateFileA(path, access, share, NULL, creationDisp,
			   flags, NULL);
    pHandleOut->Handle = f;
    if (INVALID_HANDLE_VALUE == f) {
      return -1;
    }

    LARGE_INTEGER siz;
    GetFileSizeEx(f, &siz);
    pHandleOut->Size = siz.QuadPart;
  
    if (ioFlags & IO_FLAG_MEM_MAP) {
      LARGE_INTEGER writeSize;
      writeSize.QuadPart = memMapWriteSize; 
      DWORD sizeLow = (memMapWriteSize > 0) ? writeSize.LowPart : 0;
      DWORD sizeHigh = (memMapWriteSize > 0) ? writeSize.HighPart : 0;
      HANDLE map = CreateFileMappingA(f, NULL, PAGE_READWRITE, sizeHigh, sizeLow, NULL);
      if (INVALID_HANDLE_VALUE == map) {
	if (f) CloseHandle(f);
	*pHandleOut = {};
	return -2;
      } else {
	pHandleOut->MemMapHandle = map;
      }
      pHandleOut->MappedMemory =
	MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, 0);
      if (NULL == pHandleOut->MappedMemory) {
	CloseHandle(f);
	CloseHandle(map);
	*pHandleOut = {};
	return -3;
      }
    }
    return 0;
  }
  
  inline void
  CloseFile(File *pFile) {
    if (pFile->MappedMemory) {
      UnmapViewOfFile(pFile->MappedMemory);
    }
    CloseHandle(pFile->MemMapHandle);
    CloseHandle(pFile->Handle);
    return;
  }
  
  typedef i32 (*Launch_thread_proc)(void*);
  typedef HANDLE Thread_handle;
  inline Thread_handle
  LaunchThread(Launch_thread_proc proc, void *userData) {
    return CreateThread(0, 0, (LPTHREAD_START_ROUTINE)proc, (LPVOID)userData, 0, 0);
  }

  inline void
  ErrMsgBox(const char *msg) {
    MessageBox(NULL, msg, NULL, MB_OK);
  }

  inline void*
  AllocateCommitMem(const u64 size) {
    return VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  }

  inline void
  FreeMem(void *data) {
    VirtualFree(data, 0, MEM_DECOMMIT | MEM_RELEASE);
  }

  /*
    All allocators must be a struct with two interfaces: Alloc<Type>(const u64 count) and Free(void *pointer)
  */

  struct Platform_memory_alloc {
    template<typename T> inline T*
    Alloc(const u64 count) {
      return (T*)(AllocateCommitMem(sizeof (T) * count));
    }
    inline void
    Free(void *p) {
      FreeMem(p);
    }
  };

  struct Stdlib_memory_alloc {
    template<typename T> inline T*
    Alloc(const u64 count) {
      return (T*)(malloc(sizeof (T) * count));
    }
    inline void
    Free(void *p) {
      free(p);
    }
  };

  template<typename T>
  inline void Swap(T *a, T *b) {
    T temp = *a;
    *a = *b;
    *b = temp;
    return;
  }

#define NULL_HANDLE {0, 0}
  template<typename T>
  struct Handle {
    u16 Index = 0;
    u16 Generation = 0;

    inline bool
    IsValid() {
      return this->Generation > 0;
    }
  };
  
  template<typename T>
  struct Pool {
    T* Data = NULL;
    u16 *Generations = NULL;
    u16 *OpenSlots = NULL;
    u16 Tail = 0;
    u16 Count = 0;
    u16 Capacity = 0;
    u16 GenerationCounter = 0;
    
    template<typename Allocator> inline void
    Init(Allocator *pAllocator, const u16 count) {
      this->Data = pAllocator->template
	Alloc<T>((u64)count);
      this->Generations = pAllocator->template
	Alloc<u16>((u64)count);
      this->OpenSlots = pAllocator->template
	Alloc<u16>((u64)count);
      this->Tail = 0;
      this->Count = 0;
      this->Capacity = count;
    }
    inline Handle<T>
    Add() {
      u16 openCount = this->Tail - this->Count;
      u16 newIndex;
      if (openCount) {
	newIndex = this->OpenSlots[--openCount];
      } else {
	newIndex = this->Tail++;
      }
      this->Count++;
      ASSERT(this->Count <= this->Capacity, "Pool overflow!");
      u16 gen = MIN(this->GenerationCounter++, 1);
      this->Generations[newIndex] = gen;
      this->Data[newIndex] = {};
      return {newIndex, gen};
    }
    inline T*
    Get(Handle<T> h) const {
      ASSERT(this->Generations[h.Index] == h.Generation, "Generation mismatch!");
      return this->Data + h.Index;
    }
    inline void
    Remove(Handle<T> h) {
      ASSERT(this->Count > 0, "Can not remove from empty pool!");
      this->Generations[h.Index] = 0;
      u16 openCount = this->Tail - this->Count;
      this->OpenSlots[openCount] = h.Index;
      this->Count--;
    }
    template<typename Allocator> inline void
    Free(Allocator *pAllocator) {
      pAllocator->Free(V_(this->Data));
      pAllocator->Free(V_(this->Generations));
      pAllocator->Free(V_(this->OpenSlots));
      *this = {};
    }
  };
  
  template<typename T, typename Tm>
  struct Pool2 {
    T* Data = NULL;
    Tm* Meta = NULL;
    u16 *Generations = NULL;
    u16 *OpenSlots = NULL;
    u16 Tail = 0;
    u16 Count = 0;
    u16 Capacity = 0;
    u16 GenerationCounter = 0;
    
    template<typename Allocator> inline void
    Init(Allocator *pAllocator, const u16 count) {
      this->Data = pAllocator->template
	Alloc<T>((u64)count);
      this->Meta = pAllocator->template
	Alloc<Tm>((u64)count);
      this->Generations = pAllocator->template
	Alloc<u16>((u64)count);
      this->OpenSlots = pAllocator->template
	Alloc<u16>((u64)count);
      this->Tail = 0;
      this->Count = 0;
      this->Capacity = count;
    }
    inline Handle<T>
    Add() {
      u16 openCount = this->Tail - this->Count;
      u16 newIndex;
      if (openCount) {
	newIndex = this->OpenSlots[--openCount];
      } else {
	newIndex = this->Tail++;
      }
      this->Count++;
      ASSERT(this->Count <= this->Capacity, "Pool overflow!");
      u16 gen = MIN(this->GenerationCounter++, 1);
      this->Generations[newIndex] = gen;
      // Just to be safe.
      this->Data[newIndex] = {};
      this->Meta[newIndex] = {};
      return {newIndex, gen};
    }
    inline T*
    Get(Handle<T> h) const {
      ASSERT(this->Generations[h.Index] == h.Generation, "Generation mismatch!");
      return this->Data + h.Index;
    }
    inline Tm*
    GetMeta(Handle<T> h) const {
      ASSERT(this->Generations[h.Index] == h.Generation, "Generation mismatch!");
      return this->Meta + h.Index;
    }
    inline void
    Remove(Handle<T> h) {
      ASSERT(this->Count > 0, "Can not remove from empty pool!");
      this->Generations[h.Index] = 0;
      u16 openCount = this->Tail - this->Count;
      this->OpenSlots[openCount] = h.Index;
      this->Count--;
    }
    template<typename Allocator> inline void
    Free(Allocator *pAllocator) {
      pAllocator->Free(V_(this->Data));
      pAllocator->Free(V_(this->Meta));
      pAllocator->Free(V_(this->Generations));
      pAllocator->Free(V_(this->OpenSlots));
      *this = {};
    }
  };

  typedef Handle<void*> Any;
  struct VirtualPool {
    u16 *Generations = NULL;
    u16 *OpenSlots = NULL;
    u16 Tail = 0;
    u16 Count = 0;
    u16 Capacity = 0;
    u16 GenerationCounter = 0;
    
    template<typename Allocator> inline void
    Init(Allocator *pAllocator, const u16 count) {
        this->Generations = pAllocator->template
	Alloc<u16>((u64)count);
      this->OpenSlots = pAllocator->template
	Alloc<u16>((u64)count);
      this->Tail = 0;
      this->Count = 0;
      this->Capacity = count;
    }
    inline Handle<Any>
    Add() {
      u16 openCount = this->Tail - this->Count;
      u16 newIndex;
      if (openCount) {
	newIndex = this->OpenSlots[--openCount];
      } else {
	newIndex = this->Tail++;
      }
      this->Count++;
      ASSERT(this->Count <= this->Capacity, "Pool overflow!");
      u16 gen = MIN(this->GenerationCounter++, 1);
      this->Generations[newIndex] = gen;
      return {newIndex, gen};
    }
    inline void
    Remove(Handle<Any> h) {
      ASSERT(this->Count > 0, "Can not remove from empty pool!");
      this->Generations[h.Index] = 0;
      u16 openCount = this->Tail - this->Count;
      this->OpenSlots[openCount] = h.Index;
      this->Count--;
    }
    template<typename Allocator> inline void
    Free(Allocator *pAllocator) {
      pAllocator->Free(V_(this->Generations));
      pAllocator->Free(V_(this->OpenSlots));
      *this = {};
    }
  };

  template<typename T>
  struct Stack {
    T* Data = NULL;
    u32 Count = 0, Capacity = 0;
    template<typename Allocator> inline void
    Init(Allocator *pAllocator, const u16 count) {
      this->Count = 0;
      this->Capacity = count;
      this->Data = pAllocator->template
	Alloc<T>((u64)count);
    }

    template<typename Allocator> inline void
    Free(Allocator *pAllocator) {
      // pAllocator->template
      //Free(V_(this->Data));
      pAllocator->Free(V_(this->Data));
      *this = Stack{};
    }

    inline T*
    Pop() {
      ASSERT(this->Count, "Cannot pop an empty stack!");
      return this->Data + --this->Count;
    }

    inline u32
    Push(T* d) {
      ASSERT(this->Count < this->Capacity, "Stack overflow!");
      u32 i = this->Count++;
      T* t = this->Data + i;
      *t = *d;
      return i;
    }
  };

  // Static array with unstable indices
  template<typename T, int N>
  struct VolatileArray {
    T Data[N];
    u16 Count = 0;
    u16 Capacity = N;

    inline u16
    Add(T* d) {
      ASSERT(this->Count < this->Capacity, "Out of space in array!");
      u16 i = this->Count++;
      this->Data[i] = *d;
      return i;
    }

    inline void
    Remove(u16 i) {
      ASSERT(this->Count, "No elements to remove!");
      Swap(this->Data + i, this->Data + this->Count - 1);
      this->Count--;
    }

    inline T&
    operator[](const u32 i) {
      ASSERT(i < this->Count, "Access out of bounds!");
      return this->Data[i];
    }
  };
}

// Algorithms
