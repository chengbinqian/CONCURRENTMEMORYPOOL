#pragma once
#include"Common.h"


class ThreadCache
{
public:
	//申请和释放对象
	void* Allocate(size_t size);
	void Delallocate(void* ptr, size_t size);
	// 从中心缓存获取对象
	void* FetchFromCentralCache(size_t index, size_t size);
	// 释放对象时，链表过长时，回收内存回到中心堆
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELISTS];
};


//TLS thread local storage
static __declspec(thread) ThreadCache* tls_threadcache = nullptr;