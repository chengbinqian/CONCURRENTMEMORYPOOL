#pragma once
#include"Common.h"


class ThreadCache
{
public:
	//������ͷŶ���
	void* Allocate(size_t size);
	void Delallocate(void* ptr, size_t size);
	// �����Ļ����ȡ����
	void* FetchFromCentralCache(size_t index, size_t size);
	// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ķ�
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELISTS];
};


//TLS thread local storage
static __declspec(thread) ThreadCache* tls_threadcache = nullptr;