#pragma once

#include"Common.h"
#include"ThreadCache.h"
#include"PageCache.h"

static void* ConcurrentAlloc(size_t size)
{
	try{
		if (size > MAX_BYTES){
			//�߳���Ҫ���ڴ�̫���ˣ�ֱ��ȥ��PageCache
			size_t npage = SizeClass::RoundUp(size) >> PAGE_SHIFT;
			Span* span = PageCache::GetInstance()->NewSpan(npage);
			span->_objsize = size;
			void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
			return ptr;

		}
		else{//�����߳�threadcache����Ȼ�����߳�ִ��threadcache���allocate����
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
			//���˵�����span�ĵ�������Ĵ�С������������ֽڣ���ֱ���ͷŸ�pagecache
			//��Ϊ��������������ֽڵĻ�����ôcentralcache���hash���û���Ǹ�Ͱ��..
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

