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
    size_t n = heap.size() - 1;  // 这个传给 siftdown_ 用，不能是 heap.size()
    if (i < n) {
        SwapNode_(i, n);
        // if (!siftdown_(i, n)) {
        //     siftup_(i);
        // }
        siftdown_(i, n); // 优化，直接向下调整就ok
    }
    ref.erase(heap.back().id);
    heap.pop_back();
}

//调整指定 id 的节点
void HeapTimer::adjust(int id, int newExpires) {
    assert(!heap.empty() && ref.count(id) > 0);
    heap[ref[id]].expires = Clock::now() + MS(newExpires);
    siftdown_(ref[id], heap.size()); // 因为时间变大了，所以向下调整
}

// 添加一个定时器
void HeapTimer::add(int id, int timeOut, const TimeoutCallBack& cb){
    assert(id >= 0);
    if(ref.count(id)) {
        int tmp = ref[id];
        heap[tmp].expires = Clock::now() + MS(timeOut);
        heap[tmp].cb = cb;
        if(!siftdown_(tmp, heap.size())) {
            siftup_(tmp);
        }
    }
    else{
        size_t i = heap.size();
        ref[id] = i;
        TimeStamp expires = Clock::now() + MS(timeOut);
        heap.push_back({id, expires, cb});
        siftup_(i); // 向上调整即可  
    }    
}

// 删除指定 id ,并调用回调函数
void HeapTimer::doWork(int id){
    if(heap.empty() || ref.count(id) == 0) {
        return;
    }
    size_t i = ref[id];
    TimerNode node = heap[i];
    node.cb();
    del_(i);
}

void HeapTimer::tick(){
    // 清除超时节点
    if(heap.empty()) {
        return;
    }
    while(!heap.empty()) {
        TimerNode node = heap.front();
        // 还没过期
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {
    if(heap.empty()) {
        return;
    }
    del_(0); // 删除堆顶元素
}

void HeapTimer::clear() {
    ref.clear();
    heap.clear();
}

// 离下一个超时事件还有多久
int HeapTimer::getNextTick() {
    tick();
    size_t res = -1;
    if(!heap.empty()) {
        res = std::chrono::duration_cast<MS>(heap.front().expires - Clock::now()).count();
        if(res < 0) {
            res = 0;
        }
    }
    return res;
}