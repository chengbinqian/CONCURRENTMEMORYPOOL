#include"ThreadCache.h"
#include"CentralCache.h"


void* ThreadCache::FetchFromCentralCache(size_t i, size_t size)
{
	//��ȡһ������һ��span)��������
	size_t batchNum = min(SizeClass::NumMoveSize(size), _freeLists[i].MaxSize());
	//ȥ���Ļ����ȡbatch_num������
	void* start = nullptr;
	void* end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, SizeClass::RoundUp(size));
	assert(actualNum > 0);

	//����1��������һ����ʣ�µĹҵ���������
	// ���һ����������ʣ�¹��������´�����Ͳ���Ҫ�����Ļ���
	// ���������������Ч��
	if (actualNum > 1){
		_freeLists[i].PushRange(NextObj(start), end, actualNum - 1);
	}
	// ����������
	if (_freeLists[i].MaxSize() == batchNum){
		_freeLists[i].SetMaxSize(_freeLists[i].MaxSize() + 1);
	}
	return start;

}


void* ThreadCache::Allocate(size_t size)
{
	//����ͨ����Ҫ���ֽ��������ж�����threadcache�����������һ��Ͱ
	size_t i = SizeClass::Index(size);
	//������Ͱ�����У���ֱ��ȡ
	if (!_freeLists[i].Empty()){
		return _freeLists[i].Pop();
	}
	else{
		//����ܸ��ӣ�Ҫ��ʼȥ��CenterҪ�������ַֺܶ������
		return FetchFromCentralCache(i, size);
	}
	return nullptr;
}

void ThreadCache::Delallocate(void* ptr, size_t size)
{
	size_t i = SizeClass::Index(size);
	_freeLists[i].Push(ptr);//ͷ�嵽threadcache�Ĺ�ϣ���ӦͰ������

	//���iλ�õ�Ͱ̫������Ҫ��centerȥ�ͷ�
	if (_freeLists[i].Size() >= _freeLists[i].MaxSize()){
		ListTooLong(_freeLists[i], size);
	}
}


// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ļ���
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	size_t batchNum = list.MaxSize();
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, batchNum);

	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}



