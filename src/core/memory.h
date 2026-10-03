#ifndef MEMORY_H
#define MEMORY_H

#define ZeroMemVariable(a) memset(&(a), 0x00, sizeof (a))
#define ZeroMemStaticArray(a) memset((a), 0x00, sizeof (a))

/*==================Two-level Segregate Fit==================*/
/*https://github.com/sebbbi/OffsetAllocator*/
/*MIT License

  Copyright (c) 2023 Sebastian Aaltonen

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without ion, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.*/

#if defined(__cplusplus)
extern "C" {
#endif

  const u32	TLSF_MANTISSA_BITS = 3;
  const u32	TLSF_EXP_BITS	   = 5;
  const u32	TLSF_MANTISSA_MAX  = 7; //0b111
#define IS_TLSF_BLOCK_FREE(blk) (blk[0].Size & 2) //0b10)
#define IS_TLSF_BLOCK_LAST(blk) (blk[0].Size & 1) //0b01)
#define TLSF_BLOCK_SIZE(blk)	(blk[0].Size & (~3)) //~(0b11))

#ifdef G_MEMORY_TRACE
#define GMEMTRACE(a, b) GTRACE_ZONE(a, b)
#define GMEMSCOPEB(a, b, c) GTRACE_SCOPE_BEGIN(a, b, c)
#define GMEMSCOPEE(a) GTRACE_SCOPE_END(a)
#else
#define GMEMTRACE(a, b)
#define GMEMSCOPEB(a, b, c) 
#define GMEMSCOPEE(a) 
#endif

#ifdef G_MEMORY_DEBUG_BYTES
#define GMEM_SETMEM memset
#else
#define GMEM_SETMEM
#endif

  struct Cpu_alloc {
    u8		SL[32];
#ifdef G_MEMORY_DEBUG_NAMES
    char Name[32];
#endif
    u32	FL;
  
    //value of 0xffffffff means unused. these are offsets that lead to
    //THE HEADER  
    u32	 FreeListHeads[32*8];
    u8		*Data;
  };

  typedef u32 GpuNodeIndex;

  struct _Gpu_alloc_node {
    u32	Size;
    GpuNodeIndex	PreviousPhysicalBlock;
    GpuNodeIndex	NextPhysicalBlock;
    GpuNodeIndex	NextFree;	//either next free block in list, or next free allocation
    GpuNodeIndex	PreviousFree;
    u32	Offset;		//0xffffffff means this node is free to be taken
  };

  struct Gpu_alloc {
    u8			 SL[32];
#ifdef G_MEMORY_DEBUG_NAMES
    char Name[32];
#endif  
    u32		 FL;
    //value of 0xffffffff means unused. these are offsets that lead to
    //THE HEADER  
    u32		 FreeListHeads[32*8];
    //  u8 *Data;
    _Gpu_alloc_node	*Nodes;
    GpuNodeIndex		 NodeCount;
    GpuNodeIndex		 NodeCapacity;
    GpuNodeIndex		 FirstFree;
  };

  // From https://github.com/yuikns/intrin/blob/master/intrin_x86.h
  static __inline__ __attribute__((always_inline)) unsigned char _BitScanReverse(unsigned long *const Index, const unsigned long Mask) {
    __asm__("bsrl %[Mask], %[Index]" : [Index] "=r"(*Index) : [Mask] "mr"(Mask));
    return Mask ? 1 : 0;
  }
  
  static inline u32
  Lzcntu32(u32 value) {
    unsigned long ret;
    if (value) {
      _BitScanReverse(&ret, (unsigned long)value);
      ret = 31 - ret;
    } else {
      ret = 32;
    }
    return (u32)ret;
  }

  static inline u32
  FloatToUi32(u32 u) {
    //bits [3..7] are exp
    u32	exp	 = u >> TLSF_MANTISSA_BITS;
    u32	mantissa = u & (0xffffffff >> (32 - TLSF_MANTISSA_BITS));
    i32		sh	 = exp - TLSF_MANTISSA_BITS;
    if (sh > 0){
      return ((1 << TLSF_MANTISSA_BITS) | (mantissa)) << sh;
    } else {
      return ((1 << TLSF_MANTISSA_BITS) | (mantissa)) >> -sh;      
    }
  }

  static inline u32
  Ui32ToFloatRoundUp(u32 u) {
    //check if num is 0
    u32	z      = Lzcntu32(u);
    if (z == 32)
      return 0;
    u32	first1 = 31 - z;
    //no bias, so exponent is simply (first1 - 1)
    u32	exp    = first1;
    //get the first 3 bits after first 1
    u32	mantissa = (u >> (exp - TLSF_MANTISSA_BITS)) & TLSF_MANTISSA_MAX;
    //make sure to round up!
    //make mask of all but four most significant bits
    u32	mask   = 0xffffffff >> (z + TLSF_MANTISSA_BITS);
    if (u & mask) {
      mantissa++;
    }
    return (exp << TLSF_MANTISSA_BITS) + mantissa;
  }

  static inline u32
  Ui32ToFloatRoundDown(u32 u) {
    //check if num is 0
    u32	z = Lzcntu32(u);
    if (z		       == 32)
      return 0;
    u32	first1	= 31 - z;
    //no bias, so exponent is simply number of digits - 1 (implicit msb)
    u32	exp	= first1;
    //get the first 3 bits after first 1
    u32	mantissa = (u >> (exp - TLSF_MANTISSA_BITS)) & TLSF_MANTISSA_MAX;

    return (exp << TLSF_MANTISSA_BITS) | mantissa;
  }

  /*Takes ui32 input, returns ui32 rounded up by converting to float!*/
  static inline u32
  Ui32FloatRoundUp(u32 u) {
    return FloatToUi32(Ui32ToFloatRoundUp(u));
  }

  static inline u32
  Ui32FloatRoundDown(u32 u) {
    return FloatToUi32(Ui32ToFloatRoundDown(u));
  }

  struct Free_block_header {
    //last two bits marks free/used and last/not last respectively
    u32	Size;
    u32	PreviousPhysicalBlock;
    u32	NextFree, PreviousFree;
  };

  struct Used_block_header {
    //last two bits marks free/used and last/not last respectively
    u32	Size;
    u32	PreviousPhysicalBlock;
  };

  static inline void
  _SetBlockSize(u32* const blockdata, const u32 size) {
    u32	meta  = blockdata[0] & 3; //0b11
    blockdata[0]	      = size;
    blockdata[0]	     |= meta;
  }

  static inline void
  _SetBlockLast(u32* const blockdata, const u32 isLast) {
    ASSERT(isLast == 0 || isLast == 1, "Invalid value for isLast!");
    blockdata[0]	&= 0xfffffffe;
    blockdata[0]	|= isLast;
  }

  static inline void
  _SetBlockFree(u32* const blockdata) {
    blockdata[0] |= 2; //0b10
  }

  static inline void
  _SetBlockClaimed(u32* const blockdata) {
    blockdata[0] &= (0xfffffffd);
  }

  static inline void
  _MappingSearch(const u32 size, u32* const fl, u32* const sl) {
    fl[0] = size >> TLSF_MANTISSA_BITS;
    sl[0] = size &	TLSF_MANTISSA_MAX;
  }

  static inline bool
  _AllocAssertSameBucket(const u32 size1, const u32 size2) {
    u32	fl1, sl1, fl2, sl2;
    u32	size11 = Ui32ToFloatRoundDown(size1);
    u32	size22 = Ui32ToFloatRoundDown(size2);
    _MappingSearch(size11, &fl1, &sl1);
    _MappingSearch(size22, &fl2, &sl2);
    bool	ret    = fl1 == fl2 && sl1 == sl2;
    return ret;
  }


  static inline void
  _CpuAllocMarkSegListFree(Cpu_alloc* const a,
			   u32		 fl,
			   u32 sl) {
    a[0].SL[fl] |= (1 << (7 - sl));      
    a[0].FL     |= (1 << fl);
  }

  static inline void
  _CpuAllocMarkSegListClaimed(Cpu_alloc* const a,
			      u32	 fl,
			      u32 sl) {
    a[0].SL[fl] &= ~(1 << (7 - sl));
    if (!a[0].SL[fl])
      {
	a[0].FL &= ~(1 << fl);
      }
  }

  static inline void
  _GpuAllocMarkSegListFree(Gpu_alloc* const a,
			   u32		 fl,
			   u32 sl) {
    a[0].SL[fl] |= (1 << (7 - sl));      
    a[0].FL     |= (1 << fl);  
  }

  static inline void
  _GpuAllocMarkSegListClaimed(Gpu_alloc* const a,
			      u32	 fl,
			      u32 sl) {
    a[0].SL[fl] &= ~(1 << (7 - sl));
    if (!a[0].SL[fl])
      {
	a[0].FL &= ~(1 << fl);  
      }
  }

  //check if a bucket has any free blocks in it
#define CPUALLOC_CHECK_LIST_BIT(fl, sl) (a[0].SL[fl] & (1 << (7 - sl)))
#define GPUALLOC_CHECK_LIST_BIT(fl, sl) (a[0].SL[fl] & (1 << (7 - sl)))

  static inline u32 CpuAllocInit(Cpu_alloc *a, u32 capacity, u8* data) {
    ASSERT((u64)data % 8 == 0, "Misaligned data!");
    ASSERT(data, "Cannot make a cpu allocator from null data!");
    capacity		   = capacity & ~(3); //0b11
    u32	capacityAsFloat = Ui32ToFloatRoundDown(capacity);
    u32	size = FloatToUi32(capacityAsFloat);

    //memset all bins
    memset(a[0].FreeListHeads, 0xff, sizeof(a[0].FreeListHeads));
    ZeroMemVariable(a[0].FL);
    ZeroMemStaticArray(a[0].SL);

    u32	fl, sl;
    _MappingSearch(capacityAsFloat, &fl, &sl);

    //set bits
    a[0].SL[fl] |= (1 << (7 - sl));
    a[0].FL |= (1 << (fl));
  
    a[0].FreeListHeads[fl*8+sl]	  = 0;
    u32		 actualSize = size - sizeof(Free_block_header); 
    Free_block_header	*header	  = (Free_block_header*)data;
    _SetBlockLast((u32*)header, 1);
    _SetBlockFree((u32*)header);
    _SetBlockSize((u32*)header, actualSize);
    header[0].PreviousPhysicalBlock = 
      header[0].NextFree		  = 
      header[0].PreviousFree	  = 
      0xffffffff;

    a[0].Data = data;
  
    return actualSize;
  }

  /*Inserts a free block. size excludes header size*/
  static inline void _CpuAllocMakeBlockFree(Cpu_alloc *a, u32 blockstart) {
    //find used block header, treat it as free
    Free_block_header	*header = (Free_block_header*)(a[0].Data + blockstart);

    _SetBlockFree((u32*)header);
    header[0].PreviousFree = 0xffffffff;
    header[0].NextFree	 = 0xffffffff;

    u32	fl, sl;
    u32	fsiz = Ui32ToFloatRoundDown(TLSF_BLOCK_SIZE(header));
    //  u32 fsiz			 = Ui32ToFloatRoundUp(TLSF_BLOCK_SIZE(header));
    _MappingSearch(fsiz,
		   &fl, &sl);
    u32	binidx			 = fsiz;
    u32	firstFreeInBucketOffset	 = a[0].FreeListHeads[binidx];
    Free_block_header *firstFreeInBucket =
      (Free_block_header*)(a[0].Data + firstFreeInBucketOffset);
    a[0].FreeListHeads[binidx]		 = blockstart;
    if (firstFreeInBucketOffset		!= 0xffffffff	&&
	firstFreeInBucketOffset != blockstart) {
      ASSERT(_AllocAssertSameBucket(TLSF_BLOCK_SIZE(header),
				    TLSF_BLOCK_SIZE(firstFreeInBucket)),
	     "An allocation was in the wrong bucket!");
      header[0].NextFree		 = firstFreeInBucketOffset;
      firstFreeInBucket[0].PreviousFree	 = blockstart;
    }
    _CpuAllocMarkSegListFree(a, fl, sl);
  
  }

  static inline void
  _CpuAllocCoalesce(Cpu_alloc* const a ,
		    Free_block_header* first,
		    Free_block_header* second) {
    GMEMSCOPEB(coalesce_blocks, "CpuAlloc::Coalesce", 0xcc0000);
    ASSERT(IS_TLSF_BLOCK_FREE(first)	&&
	   IS_TLSF_BLOCK_FREE(second), "blocks must be free!");
    ASSERT((u8*)first + (sizeof(Free_block_header) -
			 sizeof(Used_block_header)) + TLSF_BLOCK_SIZE(first) ==
	   (u8*)second, "blocks must be next to each other!");
    ASSERT(!IS_TLSF_BLOCK_LAST(first), "cannot coalesce when first block is last");
  
    //remove block and next from free, insert coalesced block
    u32 offsetFirst, offsetSecond, siz1, siz2, fp, fn, sp, sn;
    GMEMTRACE(get_firsts_and_seconds, 0xcc0000) {
      GMEMTRACE(get_first_half, 0xcc0000) {
	offsetFirst  = (u32)((u64)first - (u64)a[0].Data);
	siz1                     = TLSF_BLOCK_SIZE(first);
	offsetSecond = (u32)((u64)second - (u64)a[0].Data);
	siz2		     = TLSF_BLOCK_SIZE(second);
      }
      GMEMTRACE(deref_first_second, 0xcc0000) {
	fp	                     = first[0].PreviousFree;
	fn			     = first[0].NextFree;
	sp			     = second[0].PreviousFree;
	sn			     = second[0].NextFree;
      }
    }
    //todo:: get nexts and previous that are neither offsetFirst nor offsetSecond!
    while (fp == offsetFirst || fp == offsetSecond) {
      fp		     = ( (Free_block_header*)(a[0].Data + fp))[0].PreviousFree;
    }
    while (fn == offsetFirst || fn == offsetSecond) {
      fn = ((Free_block_header*)(a[0].Data + fn))[0].NextFree;
    }
    while (sp == offsetFirst || sp == offsetSecond) {
      sp = ((Free_block_header*)(a[0].Data + sp))[0].PreviousFree;
    }
    while (sn == offsetFirst || sn == offsetSecond) {
      sn     = ((Free_block_header*)(a[0].Data + sn))[0].NextFree;
    }    

    u32	fl1, sl1, fl2, sl2;
    //get slots of the two free blocks, confirm that theyre free
    u32	fsiz1 = Ui32ToFloatRoundDown(siz1),
      fsiz2 = Ui32ToFloatRoundDown(siz2);  
    _MappingSearch(fsiz1, &fl1, &sl1);
    _MappingSearch(fsiz2, &fl2, &sl2);

    ASSERT(fn != offsetFirst && sn != offsetSecond,
	   "sanity check!");
  
    //  u32	ss1 = CPUALLOC_CHECK_LIST_BIT(fl1, sl1);
    //  u32	ss2 = CPUALLOC_CHECK_LIST_BIT(fl2, sl2);
    //  ASSERT(ss1 && ss2, "free chunks have their buckets registered as non-free!");
    u32	binidx1 = fl1*8+sl1,
      binidx2		= fl2*8+sl2;

    //check the head of each list,
    //pop the blocks from them
    GMEMTRACE(pop_blocks_mark_claimed, 0xcc0000) {
      u32	off1  = a[0].FreeListHeads[binidx1];
      u32	off2 = a[0].FreeListHeads[binidx2];

      if (off1	     == offsetFirst) {
	ASSERT(fp  == 0xffffffff, "something went horrible wrong!\n");
	a[0].FreeListHeads[binidx1] = fn;
      }
      if (off2 == offsetSecond) {
	ASSERT(sp == 0xffffffff, "something went horrible wrong!\n");
	a[0].FreeListHeads[binidx2] = sn;
      }

      if (a[0].FreeListHeads[binidx1] == 0xffffffff) {
	_CpuAllocMarkSegListClaimed(a, fl1, sl1);
      }
      if (a[0].FreeListHeads[binidx2] == 0xffffffff) {
	_CpuAllocMarkSegListClaimed(a, fl2, sl2);
      }
    }

    //remove both first and second from their linked lists
    Free_block_header	*firstPrevious	= 0,
      *firstNext = 0,
      *secondPrevious = 0,
      *secondNext				= 0;
    GMEMTRACE(remove_from_linked_lists, 0xcc0000) {
      if (fp			       != 0xffffffff) {
	firstPrevious			= (Free_block_header*)(a[0].Data + fp);
	u32		 t		= firstPrevious[0].NextFree;
	firstPrevious[0].NextFree		= fn;
	if (t			       != 0xffffffff) {
	  Free_block_header* temp	= (Free_block_header*)(a[0].Data + t);
	  temp[0].PreviousFree		= fp;
	}
      }
      if (fn != 0xffffffff) {
	firstNext = (Free_block_header*)(a[0].Data + fn);
	u32	t = firstNext[0].PreviousFree;
	firstNext[0].PreviousFree = fp;
	if (t != 0xffffffff) {
	  Free_block_header* temp = (Free_block_header*)(a[0].Data + t);
	  temp[0].NextFree	= fn;
	}      
      }

      if (sp			 != 0xffffffff) {
	secondPrevious = (Free_block_header*)(a[0].Data + sp);
	u32	t = secondPrevious[0].NextFree;
	secondPrevious[0].NextFree  = sn;
	if (t			 != 0xffffffff) {
	  Free_block_header* temp = (Free_block_header*)(a[0].Data + t);
	  temp[0].PreviousFree	  = sp;
	}            
      }
      if (sn			  != 0xffffffff) {
	secondNext = (Free_block_header*)(a[0].Data + sn);
	u32	t = secondNext[0].PreviousFree;
	secondNext[0].PreviousFree   = sp;
	if (t			  != 0xffffffff) {
	  Free_block_header* temp  = (Free_block_header*)(a[0].Data + t);
	  temp[0].NextFree	   = sn;
	}            
      }
    }
    u32 totsize = siz1 + siz2 +
      sizeof(Used_block_header);
    _SetBlockSize((u32*)first, totsize);

    GMEMTRACE(fix_buckets_mark_free, 0xff0000) {
      u32	flt, slt;
      u32	binidxt	 = Ui32ToFloatRoundDown(totsize);
      _MappingSearch(binidxt, &flt, &slt);
      u32	curhead	       = a[0].FreeListHeads[binidxt];
      a[0].FreeListHeads[binidxt]  = offsetFirst;
      first[0].NextFree = 0xffffffff;
      first[0].PreviousFree	       = 0xffffffff;  
      if (curhead != 0xffffffff	&&
	  curhead		      != offsetFirst) {
	Free_block_header *cur = (Free_block_header*)
	  (a[0].Data + curhead);
	ASSERT(_AllocAssertSameBucket(TLSF_BLOCK_SIZE(first),
				      TLSF_BLOCK_SIZE(cur)),
	       "An allocation was in the wrong bucket!");      
	cur[0].PreviousFree      = offsetFirst;
	first[0].NextFree	       = curhead;
      }
      _CpuAllocMarkSegListFree(a, flt, slt);
    }
  
    if (IS_TLSF_BLOCK_LAST(second)) {
      //second block header isnt valid data anymore
      _SetBlockLast((u32*)first, 1);
    } else {
      Used_block_header *b	 = 
	(Used_block_header*)(a[0].Data + offsetSecond + siz2 +
			     (sizeof(Free_block_header) - sizeof(Used_block_header)));
      b[0].PreviousPhysicalBlock = offsetFirst;
    }
  
    //set trap
    second[0].Size		  = 0;
    second[0].PreviousPhysicalBlock = second[0].PreviousFree = second[0].NextFree = 0xffffffff;
  
    //change physical block
    ASSERT(first[0].NextFree != offsetFirst && second[0].NextFree != offsetSecond,
	   "sanity check!");
    GMEMSCOPEE(coalesce_blocks);
  }

  static inline void
  CpuAllocFree(Cpu_alloc* const a, u8* const p) {
    ASSERT((u64)p > (u64)a[0].Data, "invalid pointer!");
    Used_block_header	*block	     = (Used_block_header*)(p - sizeof(Used_block_header));
    ASSERT(block[0].Size != 0, "Hit a trap!");
    Free_block_header	*blockAsFree = (Free_block_header*)block;
    Free_block_header	*next	     = (Free_block_header*)(p + TLSF_BLOCK_SIZE(block));
    Free_block_header	*prev	     = (Free_block_header*)(a[0].Data + block[0].PreviousPhysicalBlock);
    _CpuAllocMakeBlockFree(a, (u32)((u8*)blockAsFree - a[0].Data));
    GMEMTRACE(coalesce_cur_last, 0x0000cc) {
      if (!(IS_TLSF_BLOCK_LAST(block))	&&
	  IS_TLSF_BLOCK_FREE(next)) {
	_CpuAllocCoalesce(a, blockAsFree, next);
      }
    }
    GMEMTRACE(coalesce_cur_next, 0x0000cc) {
      if (blockAsFree[0].PreviousPhysicalBlock != 0xffffffff	&&
	  IS_TLSF_BLOCK_FREE(prev)) {
	_CpuAllocCoalesce(a, prev, blockAsFree);
      }
    }

    // write empty bytes
  }

  static inline u8*
  CpuAllocAllocate(Cpu_alloc*  const a, u32 requestedSize) {
    if (requestedSize < 16)
      requestedSize    = 16;
    requestedSize = ALIGN(requestedSize, 8);
    u32	sizeAsFloat = Ui32ToFloatRoundUp(requestedSize);
    u32	size = FloatToUi32(sizeAsFloat);
  
    //mapping search
    u32	fl, sl;
    _MappingSearch(sizeAsFloat, &fl, &sl);

    u32	tmp = ((~(u32)0xff << sl)) & ((u32)a[0].SL[fl]);
    if (tmp) {
      sl = Lzcntu32(tmp) - 24;
      //recalc size
      size	    = FloatToUi32((fl << TLSF_MANTISSA_BITS) | sl);
    } else {
      //go for next seg list
      tmp = a[0].FL & ((u32)0xffffffff << (fl));
      if (!tmp)	{
	ASSERT(0, "Ran out of memory!");
	return 0;
      }
      fl = 31 - Lzcntu32(tmp);
      sl = (u32)(Lzcntu32(a[0].SL[fl]) - 24);
    }

    u32	headbinx  = sl+(fl*8);
    u32	head	  = a[0].FreeListHeads[headbinx];
    ASSERT(head	 != 0xffffffff,
	   "Expected a free bucket where there was none!");
  
    //new alloced chunk, 
    Free_block_header	*header	  = (Free_block_header*)(a[0].Data + head);
    u32		 freeSize = TLSF_BLOCK_SIZE(header);
    u32		 diff	  = freeSize - size;
    ASSERT(IS_TLSF_BLOCK_FREE(header), "block to claim wasnt free!");

    //remove claimed (former free) block from free blocks
    a[0].FreeListHeads[headbinx]	    = header[0].NextFree;
    if (header[0].NextFree != 0xffffffff) {
      Free_block_header *nextHeader = 
	(Free_block_header*)(a[0].Data + header[0].NextFree);
      ASSERT(_AllocAssertSameBucket(TLSF_BLOCK_SIZE(header),
				    TLSF_BLOCK_SIZE(nextHeader)),
	     "An allocation was in the wrong bucket!");      
      nextHeader[0].PreviousFree = 0xffffffff;
    } else {
      //mark as occupied in seg list
      _CpuAllocMarkSegListClaimed(a, fl, sl);
    }
    header[0].NextFree	 = 0xffffffff;
    header[0].PreviousFree = 0xffffffff;
    _SetBlockClaimed((u32*)header);
    if (diff > 32) {
      //split block

      //calculate offset of new free block
      u32 newFreeOff = 
	head + sizeof(Used_block_header) + TLSF_BLOCK_SIZE(header) - diff;
      
      //get bin info for new free block
      u32	fl2, sl2;
      u32	difffloat = Ui32ToFloatRoundDown(diff);
      //      u32 difffloat = Ui32ToFloatRoundUp(diff);
      _MappingSearch(difffloat, &fl2, &sl2);

      //get bin index
      u32	binidx = fl2*8+sl2;

      //adjust the size of the claimed block
      _SetBlockSize((u32*)header, newFreeOff - head - sizeof(Used_block_header));


      //write block header data for the new free block
      Free_block_header *newFreeBlock = (Free_block_header*)(a[0].Data + newFreeOff);
      _SetBlockSize((u32*)newFreeBlock, diff - sizeof(Used_block_header));
      _SetBlockFree((u32*)newFreeBlock);

      newFreeBlock[0].PreviousPhysicalBlock = head;
      newFreeBlock[0].NextFree		    = 0xffffffff;	  
      newFreeBlock[0].PreviousFree	    = 0xffffffff;

      u32	curhead = a[0].FreeListHeads[binidx];

      //if the bucket already has a free block,
      //put the new one BEFORE next one
      a[0].FreeListHeads[binidx] = newFreeOff;
      if (curhead			  != 0xffffffff) {
	Free_block_header *existingheader = 
	  (Free_block_header*)(a[0].Data + curhead);
	ASSERT(_AllocAssertSameBucket(TLSF_BLOCK_SIZE(newFreeBlock),
				      TLSF_BLOCK_SIZE(existingheader)),
	       "An allocation was in the wrong bucket!"); 
	existingheader[0].PreviousFree	  = newFreeOff;
	newFreeBlock[0].NextFree	  = curhead;
      }

      //mark as free in seg list
      _CpuAllocMarkSegListFree(a, fl2, sl2);

      if (IS_TLSF_BLOCK_LAST(header)) {
	_SetBlockLast((u32*)header, 0);
	_SetBlockLast((u32*)newFreeBlock, 1);
      }
    }

    return a[0].Data + head + (sizeof(Used_block_header));
  }

  static inline void
  CpuAllocReport(Cpu_alloc *a) {
    printf("============CPU ALLOC REPORT=================\n");
    u32	siz = (u32)ARRAYCOUNT(a[0].FreeListHeads);
    for (u32 i = 0; i < siz; i++)
      {
	u32	off = a[0].FreeListHeads[i];
	if (off == 0xffffffff)
	  continue;

	Free_block_header*	h = (Free_block_header*)(a[0].Data + off);
	printf("At offset %u: %u bytes free\n",
	       (u32)(off + sizeof(Free_block_header)),
	       h[0].Size);
	while (h[0].NextFree != 0xffffffff) {
	  h = (Free_block_header*)(a[0].Data + h[0].NextFree);
	  printf("At offset %u: %u bytes free\n",
		 (u32)(off + sizeof(Free_block_header)),
		 h[0].Size);
	}
      }
    printf("=============================================\n");
  }

  static inline void
  CpuAllocInvalidate(Cpu_alloc*  const a) {
    a[0].FL = 0;
    ZeroMemStaticArray(a[0].SL);
    memset(a[0].FreeListHeads, 0xff, sizeof(a[0].FreeListHeads));
  }

  //================GPU

  static inline u32
  GetGpuAllocRequiredSize(u32 maxAllocations) {
    u32 requiredStructSize = 
      sizeof(Gpu_alloc) + ALIGN(sizeof(_Gpu_alloc_node), 8) *	maxAllocations;
    return requiredStructSize;
  }

  //gpu alloc struct will LIVE in residencedata
  static inline Gpu_alloc*
  GpuAllocInit(u32	maxAllocations,
	       u32	capacity,
	       u8*  const residenceData,
	       u32	residenceDataSize,
	       u32*  const actualCapacity) {
    ASSERT((u64)residenceData % 8 == 0, "Misaligned data!");
  
    u32	requiredStructSize = GetGpuAllocRequiredSize(maxAllocations);
    if (requiredStructSize > residenceDataSize) {
      return 0;
    }
  
    u32	capacityAsFloat = Ui32ToFloatRoundDown(capacity);
    u32	size = FloatToUi32(capacityAsFloat);

    //memset all bins
    Gpu_alloc	*a = (Gpu_alloc*)residenceData;
    memset(a[0].FreeListHeads, 0xff, sizeof(a[0].FreeListHeads));
    ZeroMemVariable(a[0].FL);
    ZeroMemStaticArray(a[0].SL);

    u32	fl, sl;
    _MappingSearch(capacityAsFloat, &fl, &sl);

    //set bits
    a[0].SL[fl] |= (1 << (7 - sl));
    a[0].FL     |= (1 << (fl));

    //add entire data block as one free block
    a[0].FreeListHeads[fl*8+sl]	      = 0;
    u32	actualSize	      = size; 
    a[0].Nodes			      = (_Gpu_alloc_node*)(residenceData + ALIGN(sizeof(Gpu_alloc), 8));
    _SetBlockLast((u32*)a[0].Nodes, 1);
    _SetBlockFree((u32*)a[0].Nodes);
    _SetBlockSize((u32*)a[0].Nodes, actualSize);
    a[0].Nodes[0].PreviousPhysicalBlock =	
      a[0].Nodes[0].NextFree	      =	
      a[0].Nodes[0].PreviousFree	      =	
      0xffffffff;
    a[0].Nodes[0].NextPhysicalBlock = 0xffffffff;
    a[0].Nodes[0].Offset		      = 0;
  
    a[0].FirstFree = 0xffffffff;	//no free alloc nodes
    a[0].NodeCapacity = maxAllocations;
    a[0].NodeCount    = 1;

    if (actualCapacity)
      actualCapacity[0] = actualSize;
    return a;
  }

  /*changes the specified block to free. does not tamper with its size.*/
  static inline void
  _GpuAllocMakeBlockFree(Gpu_alloc*  const a, GpuNodeIndex nodeIndex) {
    _Gpu_alloc_node	*node = a[0].Nodes + nodeIndex;
    _SetBlockFree((u32*)node);

    node[0].PreviousFree = node[0].NextFree = 0xffffffff;
    u32	fl, sl;
    u32	fsiz			  = Ui32ToFloatRoundDown(TLSF_BLOCK_SIZE(node));
    _MappingSearch(fsiz, &fl, &sl);

    u32	binidx			 = fsiz;
    u32	firstFreeInBucketOffset	 = a[0].FreeListHeads[binidx];
    _Gpu_alloc_node *firstFreeInBucket	 = 
      (_Gpu_alloc_node*)(a[0].Nodes + firstFreeInBucketOffset);
    //set newly added free block as front of list
    a[0].FreeListHeads[binidx]		 = nodeIndex;
    _GpuAllocMarkSegListFree(a, fl, sl);
    if (firstFreeInBucketOffset != 0xffffffff && firstFreeInBucketOffset != nodeIndex) {
      node[0].NextFree			 = firstFreeInBucketOffset;
      firstFreeInBucket[0].PreviousFree	 = nodeIndex;
    } else {
    }
    return;
  }

  static inline void
  _GpuAllocCoalesce(Gpu_alloc		*a,
		    _Gpu_alloc_node*  const first,
		    _Gpu_alloc_node*  const second) {
    u32 indexFirst  = (u32)(first - a[0].Nodes);
    u32 indexSecond = (u32)(second - a[0].Nodes);

    //assert that the blocks are both free and consecutive
    ASSERT(IS_TLSF_BLOCK_FREE(first) &&	IS_TLSF_BLOCK_FREE(second), "Blocks were not free!");
    ASSERT(second[0].PreviousPhysicalBlock == indexFirst, "PreviousPhysical and indexFirst do not match!");
    ASSERT(!IS_TLSF_BLOCK_LAST(first), "Last block cannot be coalesced!");
  
    //remove block and next from free, insert coalesced block
    u32	siz1 = TLSF_BLOCK_SIZE(first),
      siz2		= TLSF_BLOCK_SIZE(second);
    u32	totsize = siz1 + siz2;
    u32	fl1, sl1, fl2, sl2;
    //get slots of the two free blocks, confirm that theyre free
    u32	fsiz1	= Ui32ToFloatRoundDown(siz1),
      fsiz2		= Ui32ToFloatRoundDown(siz2);
    _MappingSearch(fsiz1, &fl1, &sl1);
    _MappingSearch(fsiz2, &fl2, &sl2);
    //  u32	ss1 = GPUALLOC_CHECK_LIST_BIT(fl1, sl1);
    //  u32	ss2	= GPUALLOC_CHECK_LIST_BIT(fl2, sl2);
    //  ASSERT(ss1 && ss2);
    u32	binidx1 = fl1*8+sl1,
      binidx2 = fl2*8+sl2;
  
    //remove both first and second from their linked lists
    _Gpu_alloc_node	*firstPrevious,
      *firstNext,
      *secondPrevious,
      *secondNext;
    if (first[0].PreviousFree	!= 0xffffffff) {
      firstPrevious		 = a[0].Nodes + first[0].PreviousFree;
      firstPrevious[0].NextFree	 = first[0].NextFree;
    }
    if (first[0].NextFree		!= 0xffffffff) {
      firstNext			 = a[0].Nodes + first[0].NextFree;
      firstNext[0].PreviousFree	 = first[0].PreviousFree;
    }
  
    if (second[0].PreviousFree != 0xffffffff) {
      secondPrevious	      = a[0].Nodes + second[0].PreviousFree;
      secondPrevious[0].NextFree = second[0].NextFree;
    }
    if (second[0].NextFree	 != 0xffffffff) {
      secondNext = a[0].Nodes + second[0].NextFree;
      secondNext[0].PreviousFree  = second[0].PreviousFree;
    }
  
    //check the head of each list,
    //pop the blocks from them
    u32	off1  = a[0].FreeListHeads[binidx1];
    if (off1	     == indexFirst) {
      a[0].FreeListHeads[binidx1] = first[0].NextFree;
    }
    u32	off2 = a[0].FreeListHeads[binidx2];
    if (off2			  == indexSecond) {
      a[0].FreeListHeads[binidx2]  = second[0].NextFree;
    }

    if (a[0].FreeListHeads[binidx1] == 0xffffffff) {
      _GpuAllocMarkSegListClaimed(a, fl1, sl1);
    }
    if (a[0].FreeListHeads[binidx2] == 0xffffffff) {
      _GpuAllocMarkSegListClaimed(a, fl2, sl2);
    }
      
    _SetBlockSize((u32*)first, totsize);
    first[0].NextPhysicalBlock = second[0].NextPhysicalBlock;
    //set phys block pointed by second.next, set its previous to first

    _Gpu_alloc_node *secondNextPhysicalBlock		= 
      a[0].Nodes + second[0].NextPhysicalBlock;
    if (second[0].NextPhysicalBlock		       != 0xffffffff) {
      secondNextPhysicalBlock[0].PreviousPhysicalBlock	= 
	indexFirst;      
    }

    _GpuAllocMakeBlockFree(a, indexFirst);

    if (IS_TLSF_BLOCK_LAST(second)) {
      _SetBlockLast((u32*)first, 1);
      _SetBlockLast((u32*)second, 0);
    }
  
    second[0].NextPhysicalBlock = 0xffffffff;
    second[0].PreviousPhysicalBlock = 0xffffffff;
    //recycle the node in second
    second[0].Size = 0xffffffff;
    second[0].Offset = 0xffffffff;
    second[0].NextFree = a[0].FirstFree;
    a[0].FirstFree     = indexSecond;
  }

  struct OffsetAllocation {
    u32	Offset;
    u32	Node;
    //  u32 Size;
  };

  static inline void
  GpuAllocFree(Gpu_alloc*  const a, const OffsetAllocation alloc) {
    _Gpu_alloc_node	*toFree	  = a[0].Nodes + alloc.Node;
    //  ASSERT(alloc.Size, "hit a trap!");
    ASSERT(TLSF_BLOCK_SIZE(toFree), "hit a trap!");
    //previous PHYSICAL
    _Gpu_alloc_node	*prevPhys = a[0].Nodes + toFree[0].PreviousPhysicalBlock;
    //next PHYSICAL
    _Gpu_alloc_node	*nextPhys = a[0].Nodes + toFree[0].NextPhysicalBlock;

    _SetBlockFree((u32*)toFree);
    _GpuAllocMakeBlockFree(a, alloc.Node);
    if (toFree[0].NextPhysicalBlock != 0xffffffff &&
	!(IS_TLSF_BLOCK_LAST(toFree)) && IS_TLSF_BLOCK_FREE(nextPhys)) {
      _GpuAllocCoalesce(a, toFree, nextPhys);
    }

    if (toFree[0].PreviousPhysicalBlock != 0xffffffff	&&
	IS_TLSF_BLOCK_FREE(prevPhys)) {
      _GpuAllocCoalesce(a, prevPhys, toFree);
    }
  }

  static inline OffsetAllocation
  GpuAllocAllocate(Gpu_alloc*  const a, u32 requestedSize) {
    if (requestedSize < 16)
      requestedSize	    = 16;
    requestedSize		    = ALIGN(requestedSize, 8);
    u32	sizeAsFloat = Ui32ToFloatRoundUp(requestedSize);
    u32	size	    = FloatToUi32(sizeAsFloat);

    //size is guaranteed to be 8-byte aligned.
    //the start of our pointers should thus also be guaranteed to be 8-byte aligned

    //mapping search
    u32	fl, sl;
    _MappingSearch(sizeAsFloat, &fl, &sl);

    //  u32 tmp    = ((u32)0xffffffff << sl) & (u32)a[0].SL[fl];
    u32	tmp = ((~(u32)0xff << sl)) & ((u32)a[0].SL[fl]);
    u32	lzn;
    u32	tou32;
    u32	aa;
    if (tmp) {
      sl	    = Lzcntu32(tmp) - 24;
      //recalc size
      size	    = FloatToUi32((fl << TLSF_MANTISSA_BITS) | sl);
    } else {
      //go for next seg list
      tmp	    = a[0].FL & ((u32)0xffffffff << (fl));
      if (!tmp) {
	//ran out of memory
	return (OffsetAllocation){0xffffffff, 0xffffffff/*, 0*/};
      }
      aa = Lzcntu32(tmp);
      fl    = 31 - aa;
      tou32 = (u32)a[0].SL[fl];
      lzn = Lzcntu32(tou32);
      sl    = (u32)(lzn - 24);
    }

    u32 headbinx    = sl+(fl*8);
    u32	head = a[0].FreeListHeads[headbinx];

    ASSERT(head != 0xffffffff,
	   "Expected a free bucket where there was none!");
    //new alloced chunk, 

    _Gpu_alloc_node *header	    = (_Gpu_alloc_node*)(a[0].Nodes + head);
    u32	freeSize	    = TLSF_BLOCK_SIZE(header);
    u32	diff		    = freeSize - size;
    _SetBlockClaimed((u32*)header);
    //remove claimed block from free blocks
    a[0].FreeListHeads[headbinx]	    = header[0].NextFree;
    header[0].NextFree		    = 0xffffffff;
    if (a[0].FreeListHeads[headbinx] == 0xffffffff) {
      //mark as occupied in seg list
      _GpuAllocMarkSegListClaimed(a, fl, sl);
    }
    if (diff > 16) {
      //16 due to alignment
      //split block

      //get bin info for new free block
      u32	fl2, sl2;
      u32	difffloat = Ui32ToFloatRoundDown(diff);
      _MappingSearch(difffloat, &fl2, &sl2);

      //get bin index for new free block
      u32	binidx = fl2*8+sl2;

      //write block header data for the new free block
      _Gpu_alloc_node	*newFree;
      GpuNodeIndex	 idx  = a[0].NodeCount;
      if (a[0].FirstFree     == 0xffffffff)	{
	newFree = a[0].Nodes + a[0].NodeCount;
	a[0].NodeCount++;
	if (a[0].NodeCount >= a[0].NodeCapacity) {
	  ASSERT(0,
		 "Hit max allocation limit!");
	}
      } else	{
	idx				  = a[0].FirstFree;
	_Gpu_alloc_node	*freeNode = a[0].Nodes + a[0].FirstFree;
	ASSERT(freeNode[0].Offset  == 0xffffffff, "Node must be free.");
	newFree			  = freeNode;
	a[0].FirstFree		  = newFree[0].NextFree;
      }
      
      newFree[0].Offset = header[0].Offset + size;
      _SetBlockSize((u32*)newFree, diff);
      _SetBlockFree((u32*)newFree);
      newFree[0].PreviousPhysicalBlock	  = head;
      newFree[0].NextPhysicalBlock	  = header[0].NextPhysicalBlock;
      if (header[0].NextPhysicalBlock != 0xffffffff) {
	_Gpu_alloc_node *headerNextP =
	  a->Nodes + header->NextPhysicalBlock;
	headerNextP->PreviousPhysicalBlock = idx;
      }
      header[0].NextPhysicalBlock	  = idx;
      newFree[0].PreviousFree		  = 
	newFree[0].NextFree		  = 0xffffffff;
      u32	curhead			  = a[0].FreeListHeads[binidx];
      //check if there already are free blocks. if there are, put the new free block
      //in front of them
      a[0].FreeListHeads[binidx]	  = idx;      
      if (curhead != 0xffffffff) {
	_Gpu_alloc_node *existingheader = 
	  (_Gpu_alloc_node*)(a[0].Nodes + curhead);
	ASSERT(existingheader->Offset != 0xffffffff, "Head of free list was not free!");
	existingheader[0].PreviousFree  = idx;
	newFree[0].NextFree = curhead;
      } 

      //adjust the size of the claimed block
      _SetBlockSize((u32*)header, size);

      //mark as free in seg list
      _GpuAllocMarkSegListFree(a, fl2, sl2);

      if (IS_TLSF_BLOCK_LAST(header)) {
	_SetBlockLast((u32*)header, 0);
	_SetBlockLast((u32*)newFree, 1);
      }      
    }
    ASSERT(header[0].Offset % 8 == 0,
	   "::gpualloc error:: offset generated from alloc was not 8-byte aligned!");
    return (OffsetAllocation){header[0].Offset, head/*, size*/};
  }

  static inline void
  GpuAllocReport(Gpu_alloc *a) {
    printf("============GPU ALLOC REPORT=================\n");
    u32	siz = (u32)ARRAYCOUNT(a[0].FreeListHeads);
    u32 totalBytesFree = 0;
    for (u32 i	    = 0; i < siz; i++)
      {
	u32	off = a[0].FreeListHeads[i];
	if (off == 0xffffffff)
	  continue;

	_Gpu_alloc_node*	h = a[0].Nodes + off;
	printf("At offset %u: %u bytes free\n",
	       h[0].Offset,
	       TLSF_BLOCK_SIZE(h));
	totalBytesFree += TLSF_BLOCK_SIZE(h);
	while (h[0].NextFree != 0xffffffff)
	  {
	    h = a[0].Nodes + h[0].NextFree;
	    printf("At offset %u: %u bytes free\n",
		   h[0].Offset,
		   TLSF_BLOCK_SIZE(h));
	    totalBytesFree += TLSF_BLOCK_SIZE(h);	  
	  }
      }
    printf("Total bytes free: %u\n", totalBytesFree);
    printf("=============================================\n");
  }

  static inline void GpuAllocInvalidate(Gpu_alloc *a) {
    a[0].FL = 0;
    ZeroMemStaticArray(a[0].SL);
    memset(a[0].FreeListHeads, 0xff, sizeof(a[0].FreeListHeads));
  }


  // utils for users
  static inline u32
  CpuAllocGetPointerOffset(const Cpu_alloc*  const c, const u8*  const p) {
    return (u32)(p - c[0].Data);
  }

  static inline u32
  CpuAllocGetAllocationSize(u8* const pAllocatedChunkStart) {
    Used_block_header *h =
      (Used_block_header*)(pAllocatedChunkStart - sizeof(Used_block_header));
    return TLSF_BLOCK_SIZE(h);
  }

  static inline u32
  GpuAllocGetAllocationSize(Gpu_alloc *a, u32 node) {
    _Gpu_alloc_node *n = a->Nodes + node;
    return TLSF_BLOCK_SIZE(n);
  }


#ifdef G_MEMORY_DEBUG_NAMES
  /*
    Take null-terminated string as name
  */
  static inline void
  CpuAllocAssignDebugName(Cpu_alloc *pAlloc, const char *pName) {
    char *p = (char*)pName;
    u64 maxNameLen = sizeof(pAlloc->Name);
    u64 currentChar = 0;
    while (*p && currentChar < maxNameLen - 1) {
      pAlloc->Name[currentChar++] = *p++;
    }
    pAlloc->Name[currentChar] = 0;
    return;
  }

  static inline void
  GpuAllocAssignDebugName(Gpu_alloc *pAlloc, const char *pName) {
    char *p = (char*)pName;
    u64 maxNameLen = sizeof(pAlloc->Name);
    u64 currentChar = 0;
    while (*p && currentChar < maxNameLen - 1) {
      pAlloc->Name[currentChar++] = *p++;
    }
    pAlloc->Name[currentChar] = 0;
    return;
  }
#else
  // null op
  static inline void
  CpuAllocAssignDebugName(Cpu_alloc *pAlloc, const char *pName) {
    return;
  }

  static inline void
  GpuAllocAssignDebugName(Gpu_alloc *pAlloc, const char *pName) {
    return;
  }
#endif

#if defined(__cplusplus)
}

namespace Memory {
  struct Virtual_allocator : public Gpu_alloc {
    template <typename Allocator> static inline Virtual_allocator*
      Create(Allocator *pAllocator, const u32 allocationCapacity, const u32 allocatorCapacity) {
      const u32 reqSize = GetGpuAllocRequiredSize(allocationCapacity);
      u8 *residenceData = pAllocator->template
	Alloc<u8>(reqSize);
      u32 c;
      Gpu_alloc *a = GpuAllocInit(allocationCapacity, allocatorCapacity, residenceData, reqSize, &c);
      return (Virtual_allocator*)a;
    }

    inline void 
      Invalidate() {
      GpuAllocInvalidate(this);
    }
      
    template <typename T> inline OffsetAllocation
      Alloc(const u64 count) {
      u32 s = sizeof (T) * count;
      return GpuAllocAllocate(this, s);
    }

    inline void
      Free(OffsetAllocation p) {
      GpuAllocFree(this, p);
    }
  };
}

#endif

#undef ZeroMemVariable
#undef ZeroMemStaticArray
// end implementation
#endif
