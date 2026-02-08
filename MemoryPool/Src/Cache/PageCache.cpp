#include"include/MemoryPool/Cache/PageCache.h"
#include"include/MemoryPool/Cache/CentralCache.h"
#include"include/MemoryPool/Cache/ThreadCache.h"
namespace DM
{
	size_t PageCache::PAGE_SIZE = 4096;

	PageCache::~PageCache()
	{
		ReturnMemoryToSystem();
	}
	void* DM::PageCache::AllocateSpan(size_t pageNum)
	{
		std::lock_guard<std::mutex>lock(_Mutex);
		//先从空闲列表中查找页数>=pageNum的span
		auto it = _FreeSpans.lower_bound(pageNum);
		if (it != _FreeSpans.end())
		{
			Span*span = it->second;
			// 将取出的span从原有的空闲链表_FreeSpans[it->first]中移除        
			if (span->Next)_FreeSpans[it->first] = span->Next;
			else _FreeSpans.erase(it);
			//如果找到的span过大，进行分割
			if (span->NumPages > pageNum)
			{
				// 分割出需要的页数，剩余超出的部分放回空闲列表      
				Span* newSpan = new Span;//超出部分
				newSpan->PageAddr = static_cast<char*>(span->PageAddr) +pageNum * PAGE_SIZE;
				newSpan->NumPages = span->NumPages - pageNum;
				newSpan->Next = nullptr;
				// 将超出部分放回空闲Span*列表头部            
				auto& list = _FreeSpans[newSpan->NumPages];
				newSpan->Next = list;
				list = newSpan;
				span->NumPages = pageNum;
				_SpanMap[newSpan->PageAddr] = newSpan;
			}
			//记录span信息用于回收        
			_SpanMap[span->PageAddr] = span;
			return span->PageAddr;
		}
		//没有合适的空闲内存，向系统申请  
		void* memory = SystemAlloc(pageNum);
		if (!memory) return nullptr;
		// 创建新的span 
		Span* span = new Span;
		span->PageAddr = memory;
		span->NumPages = pageNum;
		span->Next = nullptr;
		// 记录span信息用于回收    
		_SpanMap[memory] = span;
		return memory;
	}
	void PageCache::DeallocateSpan(void* ptr, size_t pageNum)
	{
		std::lock_guard<std::mutex> lock(_Mutex);
		//查找对应的span，没找到代表不是PageCache分配的内存，直接返回    
		auto it = _SpanMap.find(ptr);
		if (it == _SpanMap.end()) return;
		Span* span = it->second;
		// 尝试合并相邻的span    
		void* nextAddr = static_cast<char*>(ptr) + pageNum * PAGE_SIZE;
		auto nextIt = _SpanMap.find(nextAddr);
		if (nextIt != _SpanMap.end())
		{
			Span* nextSpan = nextIt->second;
			//从空闲列表中移除下一个span        
			auto& nextList = _FreeSpans[nextSpan->NumPages];//注意这里是引用 
			bool bFound = false;
			if (nextList == nextSpan) //如果相邻Span(nextSpan)是空闲链表的头节点        
			{   //将nextSpan从链表_FreeSpans[nextSpan->pageNum]中移除            
				nextList = nextSpan->Next;
				bFound = true;
			}
			else //如果相邻Span(nextSpan)不是空闲链表的头节点        
			{
				Span* prev = nextList;
				while (prev->Next)
				{
					if (prev->Next == nextSpan)
					{
						prev->Next = nextSpan->Next;
						bFound = true;
						break;
					}
					prev = prev->Next;
				}
			}
			//合并span     
			if (bFound)
			{
				span->NumPages += nextSpan->NumPages;
				_SpanMap.erase(nextAddr);
				delete nextSpan;
			}
		}
		//将合并后的span插入空闲列表    
		auto& list = _FreeSpans[span->NumPages];
		span->Next = list;
		list = span;
	}

	void PageCache::ReturnMemoryToSystem()
	{
		std::lock_guard<std::mutex>lock(_Mutex);
		//置空线程缓存和中心缓存的链表,因为ThreadCache和CentralCache的内存是PageCache分配的多线程情况下可能访问到已释放的内存
		ThreadCache::Inst()->Reset();
		CentralCache::Inst()->Reset();
		
		for (const auto& it : _SystemAllocatedMemory)
		{
			SystemDeAlloc(it.first, it.second);
		}
		//释放new出来的span
		for (const auto& span : _SpanMap)
		{
			delete span.second;
		}
		_SpanMap.clear();
		_SystemAllocatedMemory.clear();
	}

	PageCache::PageCache()
	{
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		PAGE_SIZE = sysInfo.dwPageSize; // 获取系统页面大小
	}

	void* PageCache::SystemAlloc(size_t pageNum)
	{
		if (pageNum == 0) {
			return nullptr;
		}
		size_t allocSize = pageNum * PAGE_SIZE;
		void* ptr = VirtualAlloc(
			nullptr,					// 让操作系统选择内存起始地址（推荐）
			allocSize,					// 申请的内存大小（字节，自动向上对齐到页面大小）
			MEM_COMMIT | MEM_RESERVE,	// 同时保留虚拟地址空间并提交物理内存
			PAGE_READWRITE				// 内存权限：可读、可写
		);
		if (ptr == nullptr) {
			DWORD errorCode = GetLastError();
			printf("VirtualAlloc alloc failed, error code: %lu\n", errorCode);
			return nullptr;
		}
		_SystemAllocatedMemory.emplace_back(ptr, pageNum);
		return ptr;
	}	

	void PageCache::SystemDeAlloc(void* ptr, size_t pageNum)
	{
		if (ptr == nullptr || pageNum == 0) {
			return;
		}
		size_t freeSize = pageNum * PAGE_SIZE;
		BOOL ret = VirtualFree(
			ptr,                    // 要释放的内存起始地址
			0,                      // 释放整个区域时填0
			MEM_RELEASE             // 释放虚拟地址空间并回收物理内存
		);
		if (!ret) {
			DWORD errorCode = GetLastError();
			printf("VirtualFree free failed, error code: %lu\n", errorCode);
		}
	}

}