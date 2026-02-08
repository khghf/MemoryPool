#pragma once
#include<array>
#include"CentralCache.h"
namespace DM
{

	class ThreadCache
	{
	public:
		static ThreadCache* Inst()
		{
			static thread_local ThreadCache instance;
			return &instance;
		}
		void*	Allocate(size_t size);
		void	DeAllocate(void* ptr, size_t size);
		void Reset()
		{
			_FreeList.fill(nullptr);
			_FreeListSize.fill(0);
		}
	private:
		ThreadCache()
		{
			Reset();
		}
		
		void*	FetchFromCentralCache(size_t index);			// 从中心缓存获取内存    
		void	ReturnToCentralCache(void* start, size_t size);	// 归还内存到中心缓存    
		size_t	GetBatchNum(size_t size);						// 计算批量获取内存块的数量    
		bool	ShouldReturnToCentralCache(size_t index);		//判断是否需要归还内存给中心缓存
	private:
		// 每个线程的自由链表数组    
		std::array<void*, FREE_LIST_SIZE> _FreeList;
		std::array<size_t, FREE_LIST_SIZE> _FreeListSize; // 自由链表元素数量统计
	};
}


