#include"ThreadCache.h"
#include"CentralCache.h"


void* ThreadCache::FetchFromCentralCache(size_t i, size_t size)
{
	//获取一批对象（一批span)，慢启动
	size_t batchNum = min(SizeClass::NumMoveSize(size), _freeLists[i].MaxSize());
	//去中心缓存获取batch_num个对象
	void* start = nullptr;
	void* end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, SizeClass::RoundUp(size));
	assert(actualNum > 0);

	//多于1个，返回一个，剩下的挂到自由链表
	// 如果一次申请多个，剩下挂起来，下次申请就不需要找中心缓存
	// 减少锁竞争，提高效率
	if (actualNum > 1){
		_freeLists[i].PushRange(NextObj(start), end, actualNum - 1);
	}
	// 慢启动增长
	if (_freeLists[i].MaxSize() == batchNum){
		_freeLists[i].SetMaxSize(_freeLists[i].MaxSize() + 1);
	}
	return start;

}


void* ThreadCache::Allocate(size_t size)
{
	//这里通过需要的字节数，来判断属于threadcache对象链表的哪一个桶
	size_t i = SizeClass::Index(size);
	//如果这个桶里面有，就直接取
	if (!_freeLists[i].Empty()){
		return _freeLists[i].Pop();
	}
	else{
		//这里很复杂，要开始去和Center要，其中又分很多中情况
		return FetchFromCentralCache(i, size);
	}
	return nullptr;
}

void ThreadCache::Delallocate(void* ptr, size_t size)
{
	size_t i = SizeClass::Index(size);
	_freeLists[i].Push(ptr);//头插到threadcache的哈希表对应桶链表内

	//如果i位置的桶太长，就要往center去释放
	if (_freeLists[i].Size() >= _freeLists[i].MaxSize()){
		ListTooLong(_freeLists[i], size);
	}
}


// 释放对象时，链表过长时，回收内存回到中心缓存
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	size_t batchNum = list.MaxSize();
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, batchNum);

	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}



