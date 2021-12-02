#ifndef _HAWK_ALLOC_H_
#define _HAWK_ALLOC_H_

class alignas(128) HawkAllocator
{
#if defined(HAWK_ALLOC_CAPACITY)
	static const uint64_t capacity = HAWK_ALLOC_CAPACITY;
#else
	static const uint64_t capacity = 0;
#endif
	static const uint64_t alignment = 64;
	void* memoryBegin;
	void* memoryEnd;

public:

	HawkAllocator()
	{
		auto envCapacity = std::getenv("HAWK_ALLOC_CAPACITY");
		auto useCapacity = capacity;
		if(envCapacity) {
			useCapacity = std::stoul(envCapacity);
		}

		memoryBegin = mmap(NULL, useCapacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_HUGETLB, -1, 0);
		if (memoryBegin == MAP_FAILED)
		{
			memoryBegin = mmap(NULL, useCapacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
			if (memoryBegin == MAP_FAILED)
				exit(EXIT_FAILURE);
			else
				std::cerr << "HawkAllocator not using HUGETLB\n";
		}

		memoryEnd = (char*)memoryBegin + useCapacity;
	}

	inline void* malloc(const uint64_t size) noexcept
	{
		void* res = memoryBegin;
		void* memoryNextBegin = reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(memoryBegin) + size + alignment - 1) & ~(alignment - 1));
		if (__builtin_expect((memoryNextBegin < memoryEnd), 1))
		{
			memoryBegin = memoryNextBegin;
		}
		else
		{
			std::cerr << "Out of hawked memory!" << std::endl;
			std::abort();
		}

		return res;
	}
};

inline void* malloc_hawk(const uint64_t size) noexcept
{
#if MALLOC_TYPE == HAWK_ALLOC || THREAD_ALLOC_HAWK_INSERT
	thread_local HawkAllocator alloc;

	return alloc.malloc(size);
#else
	std::abort();

	return nullptr;
#endif  // THREAD_ALLOC_HAWK
}
#endif