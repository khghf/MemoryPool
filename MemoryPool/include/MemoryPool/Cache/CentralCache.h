#pragma once
#include<atomic>
#include<array>
#include<chrono>
namespace DM
{
#define FREE_LIST_SIZE 64
#define ALIGNMENT 8
#define MAXBLOCKBYTE FREE_LIST_SIZE*ALIGNMENT
	class CentralCache
	{
	public:
		static CentralCache* Inst()
		{
			static CentralCache instance;
			return &instance;
		}
		void*	FetchRange(size_t index, size_t batchNum);
		void	ReturnRange(void* start, size_t size, size_t index);
		void Reset()
		{
			for (auto& ptr : _CentralFreeList)
			{
				ptr.store(nullptr, std::memory_order_relaxed);
			}
			// 初始化所有锁
			for (auto& lock : _Locks)
			{
				lock.clear();
			}
		}
	private:
		
		CentralCache()
		{
			Reset();
		}
		void*			FetchFromPageCache(size_t size);	//从页缓存获取内存    
	private:
		std::array<std::atomic<void*>, FREE_LIST_SIZE> _CentralFreeList;//中心缓存的自由链表    
		std::array<std::atomic_flag, FREE_LIST_SIZE> _Locks;			//用于同步的自旋锁    
	};
}


