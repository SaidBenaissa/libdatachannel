/**
 * Copyright (c) 2019 Paul-Louis Ageneau
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef RTC_QUEUE_H
#define RTC_QUEUE_H

#include "include.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace rtc {

template <typename T> class Queue {
public:
	Queue(std::size_t limit = 0);
	~Queue();

	void stop();
	bool empty() const;
	size_t size() const;
	void push(const T &element);
	void push(T &&element);
	std::optional<T> pop();
	std::optional<T> tryPop();
	void wait();
	void wait(const std::chrono::milliseconds &duration);

private:
	const size_t mLimit;
	std::queue<T> mQueue;
	std::condition_variable mPopCondition, mPushCondition;
	bool mStopping = false;

	mutable std::mutex mMutex;
};

template <typename T> Queue<T>::Queue(size_t limit) : mLimit(limit) {}

template <typename T> Queue<T>::~Queue() { stop(); }

template <typename T> void Queue<T>::stop() {
	std::lock_guard<std::mutex> lock(mMutex);
	mStopping = true;
	mPopCondition.notify_all();
	mPushCondition.notify_all();
}

template <typename T> bool Queue<T>::empty() const {
	std::lock_guard<std::mutex> lock(mMutex);
	return mQueue.empty();
}

template <typename T> size_t Queue<T>::size() const {
	std::lock_guard<std::mutex> lock(mMutex);
	return mQueue.size();
}

template <typename T> void Queue<T>::push(const T &element) { push(T{element}); }

template <typename T> void Queue<T>::push(T &&element) {
	std::unique_lock<std::mutex> lock(mMutex);
	mPushCondition.wait(lock, [this]() { return !mLimit || mQueue.size() < mLimit || mStopping; });
	if (!mStopping) {
		mQueue.emplace(std::move(element));
		mPopCondition.notify_one();
	}
}

template <typename T> std::optional<T> Queue<T>::pop() {
	std::unique_lock<std::mutex> lock(mMutex);
	mPopCondition.wait(lock, [this]() { return !mQueue.empty() || mStopping; });
	if (!mQueue.empty()) {
		std::optional<T> element(std::move(mQueue.front()));
		mQueue.pop();
		return element;
	} else {
		return nullopt;
	}
}

template <typename T> std::optional<T> Queue<T>::tryPop() {
	std::unique_lock<std::mutex> lock(mMutex);
	if (!mQueue.empty()) {
		std::optional<T> element(std::move(mQueue.front()));
		mQueue.pop();
		return element;
	} else {
		return nullopt;
	}
}

template <typename T> void Queue<T>::wait() {
	std::unique_lock<std::mutex> lock(mMutex);
	mPopCondition.wait(lock, [this]() { return !mQueue.empty() || mStopping; });
}

template <typename T> void Queue<T>::wait(const std::chrono::milliseconds &duration) {
	std::unique_lock<std::mutex> lock(mMutex);
	mPopCondition.wait_for(lock, duration, [this]() { return !mQueue.empty() || mStopping; });
}

} // namespace rtc

#endif

