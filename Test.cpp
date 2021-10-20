#pragma once
#include"ThreadCache.h"
#include"ConcurrentAlloc.h"

void fun1()
{
	for (size_t i = 0; i < 10; i++){
		ConcurrentAlloc(17);
	}
}

void fun2()
{
	for (size_t i = 0; i < 20; i++){
		ConcurrentAlloc(5);
	}
}

void TestThreads()
{
	std::thread t1(fun1);
	std::thread t2(fun2);

	t1.join();
	t2.join();
}

//int main()
//{
//	TestThreads();
//	return 0;
//}