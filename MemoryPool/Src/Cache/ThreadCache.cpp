#include"include/MemoryPool/Cache/ThreadCache.h"
namespace DM
{
	inline static size_t ConvertSizeToIndex(const size_t& size) { 
		size_t ret = (size - 1) / ALIGNMENT;
		return ret>=0?ret:0; 
	}
	inline static size_t RoundUp(const size_t& size) { return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1); }
	void* ThreadCache::Allocate(size_t size)
	{
		size = size == 0 ? ALIGNMENT : size;
		if (size > MAXBLOCKBYTE)return std::malloc(size);
		size_t index = ConvertSizeToIndex(size);
		if (void* ptr = _FreeList[index])
		{
			--_FreeListSize[index];
			_FreeList[index] = *reinterpret_cast<void**>(ptr);
			return ptr;
		}
		return FetchFromCentralCache(index);
	}
	void ThreadCache::DeAllocate(void* ptr, size_t size)
	{
		if (size > MAXBLOCKBYTE) { std::free(ptr); return; }
		const size_t index = ConvertSizeToIndex(size);
		*reinterpret_cast<void**>(ptr) = _FreeList[index];
		_FreeList[index] = ptr;
		++_FreeListSize[index];
		if (ShouldReturnToCentralCache(index))
		{
			ReturnToCentralCache(_FreeList[index], size);
		}
	}
	void* ThreadCache::FetchFromCentralCache(size_t index)
	{
		size_t size = (index + 1) * ALIGNMENT;
		// 根据对象内存大小计算批量获取的数量
		size_t batchNum = GetBatchNum(size);
		// 从中心缓存批量获取内存
		void* start = CentralCache::Inst()->FetchRange(index, batchNum);
		if (!start) return nullptr;
		// 更新自由链表大小
		_FreeListSize[index] += batchNum - 1; // 增加对应大小类的自由链表大小
		// 取一个返回，其余放入线程本地自由链表
		void* result = start;
		if (batchNum > 1)
		{
			_FreeList[index] = *reinterpret_cast<void**>(start);
		}
		return result;
	}
	void ThreadCache::ReturnToCentralCache(void* start, size_t size)
	{
		const size_t index = ConvertSizeToIndex(size);
		const size_t alignedSize = RoundUp(size);
		const size_t batchNum = _FreeListSize[index];
		if (batchNum <= 1)return;
		const size_t keepNum = std::max(batchNum / 3, size_t(1));
		size_t returnNum = batchNum - keepNum;
		char* cur = static_cast<char*>(start);
		char* splitNode = cur;
		//计算实际返回数量
		for (size_t i = 0; i < keepNum - 1; ++i)
		{
			if (splitNode == nullptr)
			{
				// 如果链表提前结束，更新实际的返回数量            
				returnNum = batchNum - i;
				break;
			}
			splitNode = reinterpret_cast<char*>(*reinterpret_cast<void**>(splitNode));//移动到下一个内存块
		}
		if (splitNode != nullptr)
		{
			// 将要返回的部分和要保留的部分断开        
			void* returnNode_Start = *reinterpret_cast<void**>(splitNode);
			*reinterpret_cast<void**>(splitNode) = nullptr; // 断开连接
			// 更新ThreadCache的空闲链表        
			_FreeList[index] = start;
			// 更新自由链表大小        
			_FreeListSize[index] = batchNum-returnNum;
			// 将剩余部分返回给CentralCache        
			if (returnNum > 0 && returnNode_Start != nullptr)
			{
				CentralCache::Inst()->ReturnRange(returnNode_Start, returnNum * alignedSize, index);
			}
		}
	}
	size_t ThreadCache::GetBatchNum(size_t size)
	{
		// 基准：每次批量获取不超过4KB内存
		constexpr size_t MAX_BATCH_SIZE = 4 * 1024; // 4KB

		// 根据对象大小设置合理的基准批量数
		size_t baseNum;
		if (size <= 32)			baseNum = 64;		// 64 * 32	 = 2KB
		else if (size <= 64)	baseNum = 32;		// 32 * 64	 = 2KB
		else if (size <= 128)	baseNum = 16;		// 16 * 128	 = 2KB
		else if (size <= 256)	baseNum = 8;		// 8 *	256	 = 2KB
		else if (size <= 512)	baseNum = 4;		// 4 *	512  = 2KB
		else if (size <= 1024)	baseNum = 2;		// 2 *	1024 = 2KB
		else					baseNum = 1;		// 大于1024的对象每次只从中心缓存取1个
		size_t maxNum = std::max(size_t(1), MAX_BATCH_SIZE / size);
		return std::max(sizeof(1), std::min(maxNum, baseNum));
	}
	bool ThreadCache::ShouldReturnToCentralCache(size_t index)
	{
		return (_FreeListSize[index] > 64);
	}
}