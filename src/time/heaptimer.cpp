#include "heaptimer.h"


void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i < heap.size() && j < heap.size());
    std::swap(heap[i], heap[j]);
    ref[heap[i].id] = i; // 更新 ref
    ref[heap[j].id] = j;
}

// 向上调整，确保每个父节点都小于子节点，添加的时候用
void HeapTimer::siftup_(size_t i) {
    assert(i < heap.size());
    size_t j = (i - 1) / 2;  // j 是 i 的父节点
    while (j >= 0) {
        if (heap[j] < heap[i]) {
            break;
        }
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

// 向下调整，确保每个父节点都小于子节点，删除的时候用
bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap.size());
    assert(n >= 0 && n <= heap.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while (j < n) {
        if (j + 1 < n && heap[j + 1] < heap[j]) {
            ++j;
        }
        if (heap[i] < heap[j]) {
            break;
        }
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}
// 删除指定下标的节点
void HeapTimer::del_(size_t i) {
    assert(i < heap.size());
    size_t n = heap.size() - 1;
    if (i < n) {
        SwapNode_(i, n);
        if (!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    ref.erase(heap.back().id);
    heap.pop_back();
}