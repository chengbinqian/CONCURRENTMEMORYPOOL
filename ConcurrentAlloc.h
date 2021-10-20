#pragma once

#include"Common.h"
#include"ThreadCache.h"
#include"PageCache.h"

static void* ConcurrentAlloc(size_t size)
{
	try{
		if (size > MAX_BYTES){
			//线程需要的内存太大了，直接去找PageCache
			size_t npage = SizeClass::RoundUp(size) >> PAGE_SHIFT;
			Span* span = PageCache::GetInstance()->NewSpan(npage);
			span->_objsize = size;
			void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
			return ptr;

		}
		else{//创建线程threadcache对象，然后让线程执行threadcache里的allocate流程
			if (tls_threadcache == nullptr){
				tls_threadcache = new ThreadCache;
			}
			return tls_threadcache->Allocate(size);
		}
	}
	catch (const std::exception& e){
		std::cout << e.what() << std::endl;
	}
	return nullptr;
}


static void ConcurrentFree(void* ptr)
{
	try{
		Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
		size_t size = span->_objsize;

		if (size > MAX_BYTES){
			//如果说，这个span的单个对象的大小，超过了最大字节，就直接释放给pagecache
			//因为这个如果超过最大字节的话，那么centralcache这层hash表就没有那个桶了..
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		}
		else{
			assert(tls_threadcache);
			tls_threadcache->Delallocate(ptr, size);
		}
	}
	catch (const std::exception& e){
		std::cout << e.what() << std::endl;
	}
}

