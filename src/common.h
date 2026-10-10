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

/*
  KEY STATE FOR ONE KEY:
  KS0 | KS1
  KS: 4 bits:
  first 2 bits are states (KP), latter 2 bits (S) describe whether bit x-2 was read or not
  when S[1:0] = 3, we stop reading input from I/O
  when RESET is called, S[1:0] is set to 0


  check if key down, given cur. frame state
  if S[1:0] = 0, return false
  if S[1:0] = 1, return KS[0]
  if S[1:0] = 2, (NOT POSSIBLE) return 0
  if S[1:0] = 3, return KS[1]. set PRESS to !KS[0] && KS[1]. set RELEASE to KS[0] && !KS[1]

  check if key up, given cur. frame state
  return !(key down)

  check if key pressed, given cur & prev frame states
  a = cur.key down
  if (PRESS) return true;
  else return prev.key up && a

  note that in 1 byte, we store the key state for the previous frame and current frame
  we do this on the receiver side, though. the sender only cares about the current frame
)

 */
namespace _App_input_details {
  inline i32
  GetAppInputIndex(u32 virtualKeyCode) {
    return (i32)virtualKeyCode;
  }
  // k[1:0] dictates states 1 and 0.
  // the first write each loop is to state 0 (k[2]), which asserts k[0] (write flag)
  // all subsequent writes are to state 1 stored in k[3], which asserts k[1] (write flag)
  // bits k[7:4] are unused
  inline void
  SetKey(u8 *k, u32 state) {
    ASSERT(state < 2, "Invalid state bit!");
    u32 nk = (u32)*k; // copy key
    u32 nks = nk & 0b0011; // get state bits
    u32 ns = (nks << 1) | 1; // if nks == 0, ns = 1; if nks == 1, ns = 3;
    // if ns > 3 (state has been latches twice) but new state doesnt match state 1, overwrite state 1
    ns = MIN(ns, 0b0011); // clamp
    u32 shf = (nks & 0b0001); // decide which bit to set. nks is 0 => set first state, otherwise set second state
    u32 st = ((state << shf) & 0b0011) << (2); // shift 1 bit if S0 was already recorded
    st |= ns; // add updated status bits
    // reset state 0 if first write (nks is 0), otherwise only reset state 1
    u32 nullmask = nks ? 0b0100 : 0b0000;
    nk &= nullmask; // overwrite bits
    nk |= st; // write new value/state
    *k = (u8)nk;  
  }

#define GET_BIT(a, b) ((a >> b) & 1)
#define SET_BIT(a, b, value) (((a) & (~(1 << (b)))) | (((value) & 1) << (b)))

  inline void
  ResetKeyStates(u8 *k) {
    u32 a = (u32)*k;
    if (a & 0b0010) {
      // a >>= 1; // set KS[0] to KS[1] if KS[1] was recorded
      a = SET_BIT(a, 2, GET_BIT(a, 3));
    }
    else if (a & 0b0001) {
      // copy bit
      a = SET_BIT(a, 3, GET_BIT(a, 2));
    }
    a &= (0b1100);
    *k = (u8)a;
  }
#undef GET_BIT
#undef SET_BIT
}

#define APP_KEY_COUNT (256)
struct App_input {
  u8 KeyStates[APP_KEY_COUNT];
  u8 MouseStates[2];
  iv2 MousePosition;

  inline void
  Reset() volatile {
    for (i32 i = 0; i < APP_KEY_COUNT; i++) {
      u8 v = (u8)this->KeyStates[i];
      _App_input_details::ResetKeyStates(&v);
      this->KeyStates[i] = v;
    }
    for (u32 i = 0; i < ARRAYCOUNT(this->MouseStates); i++) {
      u8 v = (u8)this->MouseStates[i];
      _App_input_details::ResetKeyStates(&v);
      this->MouseStates[i] = v;
    }
  }
};

struct Input_stream {
  App_input States[2];

  enum Time {
    PREVIOUS = 0,
    CURRENT = 1
  };

  inline bool
  _Down(const u8 *buf, const u32 vk, bool* PRESS = NULL, bool *RELEASE = NULL) const {
    u8 v = buf[vk];
    ASSERT((v & 3) != 2, "Failed key state invariant; ks1 was set but not ks0!");
    bool ks0 = (bool)(v & 4);
    bool ks1 = (bool)(v & 8);
    bool res[4] = { ks0, ks0, 0, ks1 };
    bool ret = res[v & 3];
    bool s0s1 = (v & 3) == 3;
    if (PRESS) *PRESS = s0s1 && (!ks0 && ks1);
    if (RELEASE) *RELEASE = s0s1 && (ks0 && !ks1);
    return ret;    
  }

  // vk: virtual key
  inline bool
  Down(const u32 vk, const u32 stateIndex = CURRENT, bool* PRESS = NULL, bool *RELEASE = NULL) const {
    return _Down(States[stateIndex].KeyStates, vk, PRESS, RELEASE);
  }

  inline bool
  Up(const u32 vk, const u32 stateIndex = CURRENT, bool *PRESS = NULL, bool *RELEASE = NULL) const {
    return !Down(vk, stateIndex, PRESS, RELEASE);
  }

  inline bool
  Pressed(u32 vk) const {
    bool p, r;
    bool p0, r0;
    bool a = Down(CURRENT, vk, &p, &r);
    return p || (Up(PREVIOUS, vk, &p0, &r0) && a);
  }

  inline bool
  Released(u32 vk) const {
    bool p, r;
    bool p0, r0;
    bool a = Up(CURRENT, vk, &p, &r);
    return r || (Down(PREVIOUS, vk, &p0, &r0) && a);
  }

  inline bool
  MouseDown(const u32 mouseIndex, const u32 stateIndex = CURRENT, bool *PRESS = NULL, bool *RELEASE = NULL) const {
    return _Down(States[stateIndex].MouseStates, mouseIndex, PRESS, RELEASE);    
  }
  inline bool
  MouseUp(const u32 mouseIndex, const u32 stateIndex = CURRENT, bool *PRESS = NULL, bool *RELEASE = NULL) const {
    return !MouseDown(mouseIndex, stateIndex, PRESS, RELEASE);    
  }

  inline iv2
  MousePos(const u32 stateIndex = CURRENT) const {
    return States[stateIndex].MousePosition;
  }
};

extern volatile App_input gAppInput;

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
