#include"PageCache.h"

PageCache PageCache::_sInst;
// 向系统申请k页内存
void* PageCache::SystemAllocPage(size_t k)
{
	return ::SystemAlloc(k);
}


Span* PageCache::NewSpan(size_t k)
{
	std::lock_guard<std::recursive_mutex> lock(_mtx);

	//针对一下子申请超过128页的操作，直接找系统要
	if (k >= NPAGES){
		void* ptr = SystemAllocPage(k);
		Span* span = new Span;
		span->_pageId = (ADDRES_INT)ptr >> PAGE_SHIFT;
		span->_n = k;
		{	
			_idSpanMap[span->_pageId] = span;
		}
		return span;
	}

	//这里是pagecache的对应映射位置有这个大小的页，直接给了
	if (!_spanList[k].Empty()){
		return _spanList[k].PopFront();
	}

	//走到这里是因为k+1大小的页没有，所以要从k+1的映射位置开始切割
	for (size_t i = k + 1; i < NPAGES; i++){
		//把大页切小,以span返回
		// 切出i-k页挂回自由链表
		if (!_spanList[i].Empty()){
			//换成尾切
			Span* span = _spanList[i].PopFront();
			Span* split = new Span;
			split->_pageId = span->_pageId + span->_n - k;
			split->_n = k;
			// 改变切出来span的页号和span的映射关系
			{	
				for (PageID i = 0; i < k; ++i){
					_idSpanMap[split->_pageId + i] = split;
				}
			}

			span->_n -= k;
			//split是需要的，span是多出来剩下的，这里把它嫁接到还剩多少页的i位置
			_spanList[span->_n].PushFront(span);
			return split;
		}
	}

	//走到这里的意思，其实一般不会走到这里
	//也就程序刚刚启动的时候，连NPAGES位置的页都没有，所以先申请一个最大的，慢慢割
	Span* bigSpan = new Span;
	void* memory = SystemAllocPage(NPAGES - 1);
	bigSpan->_pageId = (size_t)memory >> 12;
	bigSpan->_n = NPAGES - 1;
	//按页号和span映射关系的建立
	{	
		for (PageID i = 0; i < bigSpan->_n; ++i){
			PageID id = bigSpan->_pageId + i;
			_idSpanMap[id] = bigSpan;
		}
	}

	_spanList[NPAGES - 1].Insert(_spanList[NPAGES - 1].Begin(), bigSpan);
	return NewSpan(k);
}

// 获取从对象到span的映射 
Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID id = (ADDRES_INT)obj >> PAGE_SHIFT;//这里将地址转换为页号
	Span* span = _idSpanMap.get(id);//通过页号找到span
	if (span != nullptr){
		return span;
	}
	else{
		assert(false);
		return nullptr;
	}
}



void PageCache::ReleaseSpanToPageCache(Span* span)
{

	//针对超过NPAGES的大内存直接释放给系统
	if (span->_n >= NPAGES){
		{
			_idSpanMap.erase(span->_pageId);
		}
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		delete span;
		return;
	}
	std::lock_guard<std::recursive_mutex> lock(_mtx);
	//这里加锁是因为：pagecache只有一个，访问的时候，它的结构可能被某些线程改变
	//检查前后空闲span的页，进行合并,解决内存碎片问题
	//向前合并
	while (1){
		PageID preId = span->_pageId - 1;
		Span* preSpan = _idSpanMap.get(preId);
			//如果前一个页的资源没有完全被回收
		if (preSpan == nullptr){
				break;
			}
		// 如果前一个页的span还在使用中，结束向前合并
		if (preSpan->_usecount != 0)
		{
			break;
		}
			//开始合并
			//如果搜集的碎片合起来超过128页
			if (preSpan->_n + span->_n >= NPAGES){
				break;
			}
			//从对应的span链表中解除链接
			_spanList[preSpan->_n].Erase(preSpan);
			


			span->_pageId = preSpan->_pageId;
			span->_n += preSpan->_n;
			//更新页之间的链接关系
			{
				for (PageID i = 0; i < preSpan->_n; ++i){
					_idSpanMap[preSpan->_pageId + i] = span;
				}
			}

			delete preSpan;
	}

	//向后合并
	while (1){
		PageID nextId = span->_pageId + span->_n;

		Span* nextSpan = _idSpanMap.get(nextId);
		if (nextSpan == nullptr){
			break;
		}
		if (nextSpan->_usecount != 0){
			break;
		}
		//如果搜集的碎片合起来超过128页
		if (nextSpan->_n + span->_n >= NPAGES){
			break;
		}

		//开始向后合并
		_spanList[nextSpan->_n].Erase(nextSpan);
		span->_n += nextSpan->_n;

		{
			for (PageID i = 0; i < nextSpan->_n; i++){
				_idSpanMap[nextSpan->_pageId + i] = span;
			}
		}

		delete nextSpan;
	}
	//合并出的大span，插入到对应链表中
	_spanList[span->_n].PushFront(span);//这个span是以页为单位的
}

	

