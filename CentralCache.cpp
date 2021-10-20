#include"CentralCache.h"
#include"PageCache.h"

CentralCache CentralCache::_sInst;

Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//先在spanlist中找还有内存的span
	Span* it = list.Begin();
	while (it != list.End()){
		if (it->_list){
			return it;
		}
		it = it->_next;
	}

	//走到这里表示span都没有内存了，只能找pagecache去拿
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	//这里开始切,切好挂在list中
	char* start = (char*)((span->_pageId << PAGE_SHIFT));
	char* end = start + (span->_n << PAGE_SHIFT);
	//这里start和end是地址，然后下面的循环是切割
	while (start < end){
		char* next = start + size;
		//头插
		NextObj(start) = span->_list;
		span->_list = start;
		start = next;
	}
	span->_objsize = size;
	list.PushFront(span);
	return span;
}


size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t size)
{
	size_t i = SizeClass::Index(size);
	std::lock_guard<std::mutex> lock(_spanLists[i]._mtx);
	//这边加锁是因为：现在不是再把这个桶里的span对象给threadcache调嘛...
	Span* span = GetOneSpan(_spanLists[i], size);

	// 找到一个有对象的span，有多少给多少
	size_t j = 1;
	start = span->_list;
	void* cur = start;
	void* prev = start;
	while (j <= n && cur != nullptr){
		prev = cur;
		cur = NextObj(cur);
		++j;
		span->_usecount++;
	}
	span->_list = cur;
	end = prev;
	NextObj(prev) = nullptr;

	return j - 1;
}


void CentralCache::ReleaseListToSpans(void* start, size_t byte_size)
{
	size_t i = SizeClass::Index(byte_size);
	std::lock_guard<std::mutex> lock(_spanLists[i]._mtx);

	while (start){
		//提前保存下一个
		void* next = NextObj(start);
		//找start内存块属于哪个span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		//把对象插入到span管理的list中
		NextObj(start) = span->_list;
		span->_list = start;
		span->_usecount--;
		if (span->_usecount == 0){
			//说明这个span中切出去的内存都已经完全被还回来了
			_spanLists[i].Erase(span);
			span->_list = nullptr;
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		}

		start = next;
	}
}
