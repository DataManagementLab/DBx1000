#pragma once

#include "tbb/queuing_mutex.h"
#include "tbb/queuing_rw_mutex.h"
#include "tbb/spin_mutex.h"
#include "tbb/spin_rw_mutex.h"

class Latch_pthread
{
private:
	pthread_mutex_t _latch;

public:
	Latch_pthread()
	{
		pthread_mutex_init(&_latch, NULL);
	}

	inline void lock([[maybe_unused]] const bool writer = false)
	{
		pthread_mutex_lock(&_latch);
	}

	inline bool try_lock([[maybe_unused]] const bool writer = false)
	{
		return pthread_mutex_trylock(&_latch) == 0;
	}

	inline void unlock()
	{
		pthread_mutex_unlock(&_latch);
	}
};

template <typename mutex_t>
class Latch_tbb
{
private:
	mutex_t _mutex;
	typename mutex_t::scoped_lock _lock;

public:
	inline void lock(const bool writer = true)
	{
		if constexpr (mutex_t::is_rw_mutex)
			_lock.acquire(_mutex, writer);
		else
			_lock.acquire(_mutex);
	}

	inline bool try_lock(const bool writer = true)
	{
		if constexpr (mutex_t::is_rw_mutex)
			return _lock.try_acquire(_mutex, writer);
		else
			return _lock.try_acquire(_mutex);
	}

	inline void unlock()
	{
		assert(_lock.try_acquire(_mutex) == false);
		_lock.release();
	}
};