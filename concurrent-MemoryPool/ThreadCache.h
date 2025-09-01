#pragma once
#include "common.h"
//�ڴ�ص�һ�� ThreadCache
class ThreadCache
{
public:
	// ��Thread ������������ͷ��ڴ��������������
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);
	// �����Ļ����ȡ����
	void* FetchFromCenterCache(size_t index, size_t size);
	// ���̫���˾͵����������
	void ListTooLong(FreeList& list, size_t size);
private:
	//ʹ�ù�ϣͰ�ķ�ʽ����ڴ�
	FreeList _freeLists[NFREELIST];
	
};
// Ϊ�˱�֤���ٵ��߳��໥������ʹ�� TLS 
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
