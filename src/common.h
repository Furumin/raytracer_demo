#pragma once

struct App_globals {
  HWND WindowHandle;
  HINSTANCE Instance;

  inline void Set(HWND wh, HINSTANCE hi) {
    this->WindowHandle = wh;
    this->Instance = hi;
  }
};

extern App_globals gAppGlobals;

struct Mutex {
  volatile u32 Lock = 0;
};
struct Scoped_lock {
 private:
  Mutex *Lock;
 public:
  inline Scoped_lock(Mutex *lock) : Lock(lock) {
    u32 spins = 0;
    while (InterlockedCompareExchange((long volatile*)(&Lock->Lock), 1, 0) != 0) {
      if (spins++ >= 8) {
	spins = 0;
	Sleep(0);
      }
    }
    return;
  }

  inline ~Scoped_lock() {
    InterlockedExchange((long volatile*)(&Lock->Lock), 0);
  }

  inline operator=(Scoped_lock &r) = delete;
  inline Scoped_lock(Scoped_lock &&r) = delete;
  inline Scoped_lock(Scoped_lock &r) = delete;
};

struct Scratch_arena {
  void *Memory;
  u64 BytesPushed;
  u64 ByteCapacity;

  inline void Init(const u64 capacity) {
    this->ByteCapacity = capacity;
    this->Memory = Util::AllocateCommitMem(capacity);
    this->BytesPushed = 0;
  }

  inline void Free() {
    Util::FreeMem(this->Memory);
    *this = {};
  }
};

#define SCRATCH_MEMORY_ALIGNMENT (16)
struct Scratch {
  u64 BytesPushed;
  Scratch_arena *Arena;
  inline Scratch(Scratch_arena *pArena) : BytesPushed(0), Arena(pArena) {}

  template <typename T>
  inline T* Push(const u64 count) {
    u64 cur = this->Arena->BytesPushed;
    u64 siz = ALIGN(sizeof (T) * count, SCRATCH_MEMORY_ALIGNMENT);
    this->BytesPushed += siz;
    this->Arena->BytesPushed += siz;
    ASSERT(Arena->BytesPushed <= Arena->ByteCapacity, "Scratch pushed too much!");
    return (T*)((u8*)(Arena->Memory) + cur);
  }

  inline ~Scratch() {
    this->Arena->BytesPushed -= this->BytesPushed;
  }

  inline Scratch(Scratch &o) = default;
  inline Scratch(Scratch &&o) = default;
  inline Scratch& operator=(const Scratch &o) = default;
};
