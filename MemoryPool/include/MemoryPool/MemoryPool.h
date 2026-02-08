#pragma once
#include "Cache/ThreadCache.h"
namespace DM
{
	class MemoryPool
	{
	public:
		static void* Allocate(size_t size)
		{
			return ThreadCache::Inst()->Allocate(size);
		}

		static void DeAllocate(void* ptr, size_t size)
		{
			ThreadCache::Inst()->DeAllocate(ptr, size);
		}
	};
}