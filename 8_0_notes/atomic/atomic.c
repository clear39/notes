原子操作和多线程库[多工内存模型]

我对这部分没太在意，直到看到了一篇文章 http://blog.csdn.net/pongba/article/details/1659952 才意识到，C++的多线程操作也是个麻烦的问题。
简而言之，C++编译器在进行编译优化的时候，认为当前是单进程的，并且遵循可观察行为（Observable Behavior）不变的原则。就是说在可观察行为不变的情况下，操作是可以被改变顺序的，而单进程可观察行为不变，不代表在多进程的情况下仍然不变。还是上大牛的例子：
例子一：

x = y = 0;
线程1						线程2
if(x == 1)					if(y == 1)
    ++y;					   ++x;

    
完全可以优化成

x = y = 0;
线程1	线程2
++y;
if(x != 1)
    –y;	++x;
if(y != 1)
    –x;
分别对于两个进程而言，可观察行为确实没有变化。而这种优化在某些时候确实会有比较明显的效果。但是很显然，语义变化了。在原来的结果里不可能发生 x和y都为0的情况，而优化过后，有可能出现。
再来个例子：

for (...) {
    ...
    if (mt)
        pthread_mutex_lock(...);
    x = ... x ...
    if (mt)
        pthread_mutex_unlock(...);
}
// 当它被Register Promotion华丽丽地优化成
r = x;
for (...) {
    ...
    if (mt) {
        x = r;
        pthread_mutex_lock(...);
        r = x;
    }
    r = ... r ...
    if (mt) {
        x = r;
        pthread_mutex_unlock(...);
        r = x;
    }
}
x = r;
做何感想？所以说，现在的多线程库多少都是有缺陷的，要解决这一问题，只能从语言内存模型上动手脚了。




这里主要介绍两个库，原子操作和线程库
原子操作（Atomic）
头文件 #include <atomic>
原子操作只支持C++类型
基本类型 std::atomic<T>
扩展实现 std::atomic_char, std::atomic_int, std::atomic_uint 等是stl中的默认实现。
这个类型用于对数据进行原子操作，在操作的过程中可以指定内存规则。

主要的函数如下：

atomic_store
保存非原子数据到原子数据结构

atomic_load
读取原子结构中的数据

atomic_exchange
保存非原子数据到原子数据结构，返回原来保存的数据

atomic_fetch_add
对原子结构中的数据做加操作

atomic_fetch_sub
atomic_fetch_sub_explicit
对原子结构中的数据做减操作

atomic_fetch_and
对原子结构中的数据逻辑与

atomic_fetch_or
对原子结构中的数据逻辑或

atomic_fetch_xor
对原子结构中的数据逻辑异或

刚才提到了在原子操作时候的内存操作规则，内存操作规则主要是 std::memory_order，这是个枚举类型，里面包含着N多规则

值	定义规则
memory_order_relaxed	不保证顺序
memory_order_consume	类比生产者-消费者模型中的消费者读取动作（仅是读取，无计数器），保证该操作先于依赖于当前读取的数据（比如后面用到了这次读取的数据）不会被提前，
			但不保证其他读取操作的顺序。仅对大多编译环境的多线程程序的编译优化过程有影响。
memory_order_acquire	类比生产者-消费者模型中的消费者读取动作（仅是读取，无计数器），保证在这个操作之后的所有操作不会被提前，同样对大多编译环境的多线程程序的编译优化过程有影响。
memory_order_release	类比生产者-消费者模型中的生产者创建动作（仅操作一个数据），保证这之前的操作不会被延后。
memory_order_acq_rel	同时包含memory_order_acquire和memory_order_release标记
memory_order_seq_cst	全部存取都按顺序执行，在多核系统上容易成为性能瓶颈



在前面的原子操作的函数中，默认规则都是std::memory_order_seq_cst
此外，atomic还有一些标记类型和测试操作，比较类似操作系统里的原子操作
std::atomic_flag
标记类型
 
atomic_flag_test_and_set
尝试设置为占用（原子操作）
 
atomic_flag_clear
释放（原子操作）

std::atomic_flag lock = ATOMIC_FLAG_INIT;
 
void f(int n) {
    for(int cnt = 0; cnt < 100; ++cnt) {
        while(std::atomic_flag_test_and_set_explicit(&lock, std::memory_order_acquire));
        std::cout << "线程 " << n << std::endl;
        std::atomic_flag_clear_explicit(&lock, std::memory_order_release);
    }
}
 
int main() {
    std::atomic_int a;
 
    a.store(100);
    a.fetch_add(105);
 
    int i = a.load(std::memory_order_consume);
 
    printf("i => %d\n", i);
 
    // 原子标记
    std::vector<std::thread> v;
    for (int n = 0; n < 10; ++n) {
        v.emplace_back(f, n);
    }
    for (auto t = v.begin(); t != v.end(); ++ t) {
        t->join();
    }
 
    return 0;
}

