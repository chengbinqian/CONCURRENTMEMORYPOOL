#include"PageCache.h"

PageCache PageCache::_sInst;
// ��ϵͳ����kҳ�ڴ�
void* PageCache::SystemAllocPage(size_t k)
{
	return ::SystemAlloc(k);
}


Span* PageCache::NewSpan(size_t k)
{
	std::lock_guard<std::recursive_mutex> lock(_mtx);

	//���һ�������볬��128ҳ�Ĳ�����ֱ����ϵͳҪ
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

	//������pagecache�Ķ�Ӧӳ��λ���������С��ҳ��ֱ�Ӹ���
	if (!_spanList[k].Empty()){
		return _spanList[k].PopFront();
	}

	//�ߵ���������Ϊk+1��С��ҳû�У�����Ҫ��k+1��ӳ��λ�ÿ�ʼ�и�
	for (size_t i = k + 1; i < NPAGES; i++){
		//�Ѵ�ҳ��С,��span����
		// �г�i-kҳ�һ���������
		if (!_spanList[i].Empty()){
			//����β��
			Span* span = _spanList[i].PopFront();
			Span* split = new Span;
			split->_pageId = span->_pageId + span->_n - k;
			split->_n = k;
			// �ı��г���span��ҳ�ź�span��ӳ���ϵ
			{	
				for (PageID i = 0; i < k; ++i){
					_idSpanMap[split->_pageId + i] = split;
				}
			}

			span->_n -= k;
			//split����Ҫ�ģ�span�Ƕ����ʣ�µģ���������޽ӵ���ʣ����ҳ��iλ��
			_spanList[span->_n].PushFront(span);
			return split;
		}
	}

	//�ߵ��������˼����ʵһ�㲻���ߵ�����
	//Ҳ�ͳ���ո�������ʱ����NPAGESλ�õ�ҳ��û�У�����������һ�����ģ�������
	Span* bigSpan = new Span;
	void* memory = SystemAllocPage(NPAGES - 1);
	bigSpan->_pageId = (size_t)memory >> 12;
	bigSpan->_n = NPAGES - 1;
	//��ҳ�ź�spanӳ���ϵ�Ľ���
	{	
		for (PageID i = 0; i < bigSpan->_n; ++i){
			PageID id = bigSpan->_pageId + i;
			_idSpanMap[id] = bigSpan;
		}
	}

	_spanList[NPAGES - 1].Insert(_spanList[NPAGES - 1].Begin(), bigSpan);
	return NewSpan(k);
}

// ��ȡ�Ӷ���span��ӳ�� 
Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID id = (ADDRES_INT)obj >> PAGE_SHIFT;//���ｫ��ַת��Ϊҳ��
	Span* span = _idSpanMap.get(id);//ͨ��ҳ���ҵ�span
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

	//��Գ���NPAGES�Ĵ��ڴ�ֱ���ͷŸ�ϵͳ
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
	//�����������Ϊ��pagecacheֻ��һ�������ʵ�ʱ�����Ľṹ���ܱ�ĳЩ�̸߳ı�
	//���ǰ�����span��ҳ�����кϲ�,����ڴ���Ƭ����
	//��ǰ�ϲ�
	while (1){
		PageID preId = span->_pageId - 1;
		Span* preSpan = _idSpanMap.get(preId);
			//���ǰһ��ҳ����Դû����ȫ������
		if (preSpan == nullptr){
				break;
			}
		// ���ǰһ��ҳ��span����ʹ���У�������ǰ�ϲ�
		if (preSpan->_usecount != 0)
		{
			break;
		}
			//��ʼ�ϲ�
			//����Ѽ�����Ƭ����������128ҳ
			if (preSpan->_n + span->_n >= NPAGES){
				break;
			}
			//�Ӷ�Ӧ��span�����н������
			_spanList[preSpan->_n].Erase(preSpan);
			


			span->_pageId = preSpan->_pageId;
			span->_n += preSpan->_n;
			//����ҳ֮������ӹ�ϵ
			{
				for (PageID i = 0; i < preSpan->_n; ++i){
					_idSpanMap[preSpan->_pageId + i] = span;
				}
			}

			delete preSpan;
	}

	//���ϲ�
	while (1){
		PageID nextId = span->_pageId + span->_n;

		Span* nextSpan = _idSpanMap.get(nextId);
		if (nextSpan == nullptr){
			break;
		}
		if (nextSpan->_usecount != 0){
			break;
		}
		//����Ѽ�����Ƭ����������128ҳ
		if (nextSpan->_n + span->_n >= NPAGES){
			break;
		}

		//��ʼ���ϲ�
		_spanList[nextSpan->_n].Erase(nextSpan);
		span->_n += nextSpan->_n;

		{
			for (PageID i = 0; i < nextSpan->_n; i++){
				_idSpanMap[nextSpan->_pageId + i] = span;
			}
		}

		delete nextSpan;
	}
	//�ϲ����Ĵ�span�����뵽��Ӧ������
	_spanList[span->_n].PushFront(span);//���span����ҳΪ��λ��
}

	

