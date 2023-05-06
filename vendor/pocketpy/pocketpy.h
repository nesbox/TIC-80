/*
 *  Copyright (c) 2023 blueloveTH
 *  Distributed Under The MIT License
 *  https://github.com/blueloveTH/pocketpy
 */

#ifndef POCKETPY_H
#define POCKETPY_H


#ifdef _MSC_VER
#pragma warning (disable:4267)
#pragma warning (disable:4101)
#pragma warning (disable:4244)
#define _CRT_NONSTDC_NO_DEPRECATE
#define strdup _strdup
#endif

#include <cmath>
#include <cstring>

#include <sstream>
#include <regex>
#include <stdexcept>
#include <vector>
#include <string>
#include <chrono>
#include <string_view>
#include <iomanip>
#include <memory>
#include <iostream>
#include <map>
#include <set>
#include <algorithm>
#include <random>
#include <initializer_list>
#include <variant>
#include <type_traits>

#define PK_VERSION				"0.9.9"

// debug macros
#define DEBUG_NO_BUILTIN_MODULES	0
#define DEBUG_EXTRA_CHECK			0
#define DEBUG_DIS_EXEC				0
#define DEBUG_CEVAL_STEP			0
#define DEBUG_CEVAL_STEP_MIN		0
#define DEBUG_FULL_EXCEPTION		0
#define DEBUG_MEMORY_POOL			0
#define DEBUG_NO_MEMORY_POOL		0
#define DEBUG_NO_AUTO_GC			0
#define DEBUG_GC_STATS				0

#if (defined(__ANDROID__) && __ANDROID_API__ <= 22)
#define PK_ENABLE_OS 			0
#else
#define PK_ENABLE_OS 			0
#endif

// This is the maximum number of arguments in a function declaration
// including positional arguments, keyword-only arguments, and varargs
#define PK_MAX_CO_VARNAMES			255

#if _MSC_VER
#define PK_ENABLE_COMPUTED_GOTO		0
#define UNREACHABLE()				__assume(0)
#else
#define PK_ENABLE_COMPUTED_GOTO		1
#define UNREACHABLE()				__builtin_unreachable()

#if DEBUG_CEVAL_STEP || DEBUG_CEVAL_STEP_MIN
#undef PK_ENABLE_COMPUTED_GOTO
#endif

#endif

namespace pkpy{

namespace std = ::std;

template <size_t T>
struct NumberTraits;

template <>
struct NumberTraits<4> {
	using int_t = int32_t;
	using float_t = float;

	template<typename... Args>
	static int_t stoi(Args&&... args) { return std::stoi(std::forward<Args>(args)...); }
	template<typename... Args>
	static float_t stof(Args&&... args) { return std::stof(std::forward<Args>(args)...); }
};

template <>
struct NumberTraits<8> {
	using int_t = int64_t;
	using float_t = double;

	template<typename... Args>
	static int_t stoi(Args&&... args) { return std::stoll(std::forward<Args>(args)...); }
	template<typename... Args>
	static float_t stof(Args&&... args) { return std::stod(std::forward<Args>(args)...); }
};

using Number = NumberTraits<sizeof(void*)>;
using i64 = Number::int_t;
using f64 = Number::float_t;

static_assert(sizeof(i64) == sizeof(void*));
static_assert(sizeof(f64) == sizeof(void*));
static_assert(std::numeric_limits<f64>::is_iec559);

struct Dummy { };
struct DummyInstance { };
struct DummyModule { };
struct Discarded { };

struct Type {
	int index;
	Type(): index(-1) {}
	Type(int index): index(index) {}
	bool operator==(Type other) const noexcept { return this->index == other.index; }
	bool operator!=(Type other) const noexcept { return this->index != other.index; }
	operator int() const noexcept { return this->index; }
};

#define THREAD_LOCAL	// thread_local
#define CPP_LAMBDA(x) ([](VM* vm, ArgsView args) { return x; })
#define CPP_NOT_IMPLEMENTED() ([](VM* vm, ArgsView args) { vm->NotImplementedError(); return vm->None; })

#ifdef POCKETPY_H
#define FATAL_ERROR() throw std::runtime_error( "L" + std::to_string(__LINE__) + " FATAL_ERROR()!");
#else
#define FATAL_ERROR() throw std::runtime_error( __FILE__ + std::string(":") + std::to_string(__LINE__) + " FATAL_ERROR()!");
#endif

inline const float kInstAttrLoadFactor = 0.67f;
inline const float kTypeAttrLoadFactor = 0.5f;

struct PyObject;
#define BITS(p) (reinterpret_cast<i64>(p))
inline bool is_tagged(PyObject* p) noexcept { return (BITS(p) & 0b11) != 0b00; }
inline bool is_int(PyObject* p) noexcept { return (BITS(p) & 0b11) == 0b01; }
inline bool is_float(PyObject* p) noexcept { return (BITS(p) & 0b11) == 0b10; }
inline bool is_special(PyObject* p) noexcept { return (BITS(p) & 0b11) == 0b11; }

inline bool is_both_int_or_float(PyObject* a, PyObject* b) noexcept {
    return is_tagged(a) && is_tagged(b);
}

inline bool is_both_int(PyObject* a, PyObject* b) noexcept {
    return is_int(a) && is_int(b);
}

// special singals, is_tagged() for them is true
inline PyObject* const PY_NULL = (PyObject*)0b000011;
inline PyObject* const PY_BEGIN_CALL = (PyObject*)0b010011;
inline PyObject* const PY_OP_CALL = (PyObject*)0b100011;
inline PyObject* const PY_OP_YIELD = (PyObject*)0b110011;

struct Expr;
typedef std::unique_ptr<Expr> Expr_;

} // namespace pkpy


namespace pkpy{

struct LinkedListNode{
    LinkedListNode* prev;
    LinkedListNode* next;
};

template<typename T>
struct DoubleLinkedList{
    static_assert(std::is_base_of_v<LinkedListNode, T>);
    int _size;
    LinkedListNode head;
    LinkedListNode tail;
    
    DoubleLinkedList(): _size(0){
        head.prev = nullptr;
        head.next = &tail;
        tail.prev = &head;
        tail.next = nullptr;
    }

    void push_back(T* node){
        node->prev = tail.prev;
        node->next = &tail;
        tail.prev->next = node;
        tail.prev = node;
        _size++;
    }

    void push_front(T* node){
        node->prev = &head;
        node->next = head.next;
        head.next->prev = node;
        head.next = node;
        _size++;
    }

    void pop_back(){
#if DEBUG_MEMORY_POOL
        if(empty()) throw std::runtime_error("DoubleLinkedList::pop_back() called on empty list");
#endif
        tail.prev->prev->next = &tail;
        tail.prev = tail.prev->prev;
        _size--;
    }

    void pop_front(){
#if DEBUG_MEMORY_POOL
        if(empty()) throw std::runtime_error("DoubleLinkedList::pop_front() called on empty list");
#endif
        head.next->next->prev = &head;
        head.next = head.next->next;
        _size--;
    }

    T* back() const {
#if DEBUG_MEMORY_POOL
        if(empty()) throw std::runtime_error("DoubleLinkedList::back() called on empty list");
#endif
        return static_cast<T*>(tail.prev);
    }

    T* front() const {
#if DEBUG_MEMORY_POOL
        if(empty()) throw std::runtime_error("DoubleLinkedList::front() called on empty list");
#endif
        return static_cast<T*>(head.next);
    }

    void erase(T* node){
#if DEBUG_MEMORY_POOL
        if(empty()) throw std::runtime_error("DoubleLinkedList::erase() called on empty list");
        LinkedListNode* n = head.next;
        while(n != &tail){
            if(n == node) break;
            n = n->next;
        }
        if(n != node) throw std::runtime_error("DoubleLinkedList::erase() called on node not in the list");
#endif
        node->prev->next = node->next;
        node->next->prev = node->prev;
        _size--;
    }

    void move_all_back(DoubleLinkedList<T>& other){
        if(other.empty()) return;
        other.tail.prev->next = &tail;
        tail.prev->next = other.head.next;
        other.head.next->prev = tail.prev;
        tail.prev = other.tail.prev;
        _size += other._size;
        other.head.next = &other.tail;
        other.tail.prev = &other.head;
        other._size = 0;
    }

    bool empty() const {
#if DEBUG_MEMORY_POOL
        if(size() == 0){
            if(head.next != &tail || tail.prev != &head){
                throw std::runtime_error("DoubleLinkedList::size() returned 0 but the list is not empty");
            }
            return true;
        }
#endif
        return _size == 0;
    }

    int size() const { return _size; }

    template<typename Func>
    void apply(Func func){
        LinkedListNode* p = head.next;
        while(p != &tail){
            LinkedListNode* next = p->next;
            func(static_cast<T*>(p));
            p = next;
        }
    }
};

template<int __BlockSize=128>
struct MemoryPool{
    static const size_t __MaxBlocks = 256*1024 / __BlockSize;
    struct Block{
        void* arena;
        char data[__BlockSize];
    };

    struct Arena: LinkedListNode{
        Block _blocks[__MaxBlocks];
        Block* _free_list[__MaxBlocks];
        int _free_list_size;
        bool dirty;
        
        Arena(): _free_list_size(__MaxBlocks), dirty(false){
            for(int i=0; i<__MaxBlocks; i++){
                _blocks[i].arena = this;
                _free_list[i] = &_blocks[i];
            }
        }

        bool empty() const { return _free_list_size == 0; }
        bool full() const { return _free_list_size == __MaxBlocks; }

        size_t allocated_size() const{
            return __BlockSize * (__MaxBlocks - _free_list_size);
        }

        Block* alloc(){
#if DEBUG_MEMORY_POOL
            if(empty()) throw std::runtime_error("Arena::alloc() called on empty arena");
#endif
            _free_list_size--;
            return _free_list[_free_list_size];
        }

        void dealloc(Block* block){
#if DEBUG_MEMORY_POOL
            if(full()) throw std::runtime_error("Arena::dealloc() called on full arena");
#endif
            _free_list[_free_list_size] = block;
            _free_list_size++;
        }
    };

    DoubleLinkedList<Arena> _arenas;
    DoubleLinkedList<Arena> _empty_arenas;

    template<typename __T>
    void* alloc() { return alloc(sizeof(__T)); }

    void* alloc(size_t size){
#if DEBUG_NO_MEMORY_POOL
        return malloc(size);
#endif
        if(size > __BlockSize){
            void* p = malloc(sizeof(void*) + size);
            memset(p, 0, sizeof(void*));
            return (char*)p + sizeof(void*);
        }

        if(_arenas.empty()){
            // std::cout << _arenas.size() << ',' << _empty_arenas.size() << ',' << _full_arenas.size() << std::endl;
            _arenas.push_back(new Arena());
        }
        Arena* arena = _arenas.back();
        void* p = arena->alloc()->data;
        if(arena->empty()){
            _arenas.pop_back();
            arena->dirty = true;
            _empty_arenas.push_back(arena);
        }
        return p;
    }

    void dealloc(void* p){
#if DEBUG_NO_MEMORY_POOL
        free(p);
        return;
#endif
#if DEBUG_MEMORY_POOL
        if(p == nullptr) throw std::runtime_error("MemoryPool::dealloc() called on nullptr");
#endif
        Block* block = (Block*)((char*)p - sizeof(void*));
        if(block->arena == nullptr){
            free(block);
        }else{
            Arena* arena = (Arena*)block->arena;
            if(arena->empty()){
                _empty_arenas.erase(arena);
                _arenas.push_front(arena);
                arena->dealloc(block);
            }else{
                arena->dealloc(block);
                if(arena->full() && arena->dirty){
                    _arenas.erase(arena);
                    delete arena;
                }
            }
        }
    }

    size_t allocated_size() {
        size_t n = 0;
        _arenas.apply([&n](Arena* arena){ n += arena->allocated_size(); });
        _empty_arenas.apply([&n](Arena* arena){ n += arena->allocated_size(); });
        return n;
    }

    ~MemoryPool(){
        _arenas.apply([](Arena* arena){ delete arena; });
        _empty_arenas.apply([](Arena* arena){ delete arena; });
    }
};

inline MemoryPool<64> pool64;
inline MemoryPool<128> pool128;

// get the total memory usage of pkpy (across all VMs)
inline size_t memory_usage(){
    return pool64.allocated_size() + pool128.allocated_size();
}

#define SP_MALLOC(size) pool128.alloc(size)
#define SP_FREE(p) pool128.dealloc(p)

template <typename T>
struct shared_ptr {
    int* counter;

    T* _t() const noexcept { return (T*)(counter + 1); }
    void _inc_counter() { if(counter) ++(*counter); }
    void _dec_counter() { if(counter && --(*counter) == 0) {((T*)(counter + 1))->~T(); SP_FREE(counter);} }

public:
    shared_ptr() : counter(nullptr) {}
    shared_ptr(int* counter) : counter(counter) {}
    shared_ptr(const shared_ptr& other) : counter(other.counter) {
        _inc_counter();
    }
    shared_ptr(shared_ptr&& other) noexcept : counter(other.counter) {
        other.counter = nullptr;
    }
    ~shared_ptr() { _dec_counter(); }

    bool operator==(const shared_ptr& other) const { return counter == other.counter; }
    bool operator!=(const shared_ptr& other) const { return counter != other.counter; }
    bool operator<(const shared_ptr& other) const { return counter < other.counter; }
    bool operator>(const shared_ptr& other) const { return counter > other.counter; }
    bool operator<=(const shared_ptr& other) const { return counter <= other.counter; }
    bool operator>=(const shared_ptr& other) const { return counter >= other.counter; }
    bool operator==(std::nullptr_t) const { return counter == nullptr; }
    bool operator!=(std::nullptr_t) const { return counter != nullptr; }

    shared_ptr& operator=(const shared_ptr& other) {
        _dec_counter();
        counter = other.counter;
        _inc_counter();
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& other) noexcept {
        _dec_counter();
        counter = other.counter;
        other.counter = nullptr;
        return *this;
    }

    T& operator*() const { return *_t(); }
    T* operator->() const { return _t(); }
    T* get() const { return _t(); }

    int use_count() const { 
        return counter ? *counter : 0;
    }

    void reset(){
        _dec_counter();
        counter = nullptr;
    }
};

template <typename T, typename... Args>
shared_ptr<T> make_sp(Args&&... args) {
    int* p = (int*)SP_MALLOC(sizeof(int) + sizeof(T));
    *p = 1;
    new(p+1) T(std::forward<Args>(args)...);
    return shared_ptr<T>(p);
}

};  // namespace pkpy



namespace pkpy{

template<typename T>
struct pod_vector{
    static_assert(128 % sizeof(T) == 0);
    static_assert(std::is_pod_v<T>);
    static constexpr int N = 128 / sizeof(T);
    static_assert(N > 4);
    int _size;
    int _capacity;
    T* _data;

    pod_vector(): _size(0), _capacity(N) {
        _data = (T*)pool128.alloc(_capacity * sizeof(T));
    }

    pod_vector(int size): _size(size), _capacity(std::max(N, size)) {
        _data = (T*)pool128.alloc(_capacity * sizeof(T));
    }

    pod_vector(const pod_vector& other): _size(other._size), _capacity(other._capacity) {
        _data = (T*)pool128.alloc(_capacity * sizeof(T));
        memcpy(_data, other._data, sizeof(T) * _size);
    }

    pod_vector(pod_vector&& other) noexcept {
        _size = other._size;
        _capacity = other._capacity;
        _data = other._data;
        other._data = nullptr;
    }

    pod_vector& operator=(pod_vector&& other) noexcept {
        if(_data!=nullptr) pool128.dealloc(_data);
        _size = other._size;
        _capacity = other._capacity;
        _data = other._data;
        other._data = nullptr;
        return *this;
    }

    // remove copy assignment
    pod_vector& operator=(const pod_vector& other) = delete;

    template<typename __ValueT>
    void push_back(__ValueT&& t) {
        if (_size == _capacity) reserve(_capacity*2);
        _data[_size++] = std::forward<__ValueT>(t);
    }

    template<typename... Args>
    void emplace_back(Args&&... args) {
        if (_size == _capacity) reserve(_capacity*2);
        new (&_data[_size++]) T(std::forward<Args>(args)...);
    }

    void reserve(int cap){
        if(cap <= _capacity) return;
        _capacity = cap;
        T* old_data = _data;
        _data = (T*)pool128.alloc(_capacity * sizeof(T));
        if(old_data!=nullptr){
            memcpy(_data, old_data, sizeof(T) * _size);
            pool128.dealloc(old_data);
        }
    }

    void pop_back() { _size--; }
    T popx_back() { T t = std::move(_data[_size-1]); _size--; return t; }
    void extend(const pod_vector& other){
        for(int i=0; i<other.size(); i++) push_back(other[i]);
    }

    T& operator[](int index) { return _data[index]; }
    const T& operator[](int index) const { return _data[index]; }

    T* begin() { return _data; }
    T* end() { return _data + _size; }
    const T* begin() const { return _data; }
    const T* end() const { return _data + _size; }
    T& back() { return _data[_size - 1]; }
    const T& back() const { return _data[_size - 1]; }

    bool empty() const { return _size == 0; }
    int size() const { return _size; }
    T* data() { return _data; }
    const T* data() const { return _data; }
    void clear() { _size=0; }

    template<typename __ValueT>
    void insert(int i, __ValueT&& val){
        if (_size == _capacity) reserve(_capacity*2);
        for(int j=_size; j>i; j--) _data[j] = _data[j-1];
        _data[i] = std::forward<__ValueT>(val);
        _size++;
    }

    void erase(int i){
        for(int j=i; j<_size-1; j++) _data[j] = _data[j+1];
        _size--;
    }

    void resize(int size){
        if(size > _capacity) reserve(size);
        _size = size;
    }

    ~pod_vector() {
        if(_data!=nullptr) pool128.dealloc(_data);
    }
};


template <typename T, typename Container=std::vector<T>>
class stack{
	Container vec;
public:
	void push(const T& t){ vec.push_back(t); }
	void push(T&& t){ vec.push_back(std::move(t)); }
    template<typename... Args>
    void emplace(Args&&... args){
        vec.emplace_back(std::forward<Args>(args)...);
    }
	void pop(){ vec.pop_back(); }
	void clear(){ vec.clear(); }
	bool empty() const { return vec.empty(); }
	size_t size() const { return vec.size(); }
	T& top(){ return vec.back(); }
	const T& top() const { return vec.back(); }
	T popx(){ T t = std::move(vec.back()); vec.pop_back(); return t; }
    void reserve(int n){ vec.reserve(n); }
	Container& data() { return vec; }
};

template <typename T>
using pod_stack = stack<T, pod_vector<T>>;
} // namespace pkpy


namespace pkpy {

inline int utf8len(unsigned char c, bool suppress=false){
    if((c & 0b10000000) == 0) return 1;
    if((c & 0b11100000) == 0b11000000) return 2;
    if((c & 0b11110000) == 0b11100000) return 3;
    if((c & 0b11111000) == 0b11110000) return 4;
    if((c & 0b11111100) == 0b11111000) return 5;
    if((c & 0b11111110) == 0b11111100) return 6;
    if(!suppress) throw std::runtime_error("invalid utf8 char: " + std::to_string(c));
    return 0;
}

struct Str{
    int size;
    bool is_ascii;
    char* data;

    Str(): size(0), is_ascii(true), data(nullptr) {}

    Str(int size, bool is_ascii): size(size), is_ascii(is_ascii) {
        data = (char*)pool64.alloc(size);
    }

#define STR_INIT()                                  \
        data = (char*)pool64.alloc(size);           \
        for(int i=0; i<size; i++){                  \
            data[i] = s[i];                         \
            if(!isascii(s[i])) is_ascii = false;    \
        }

    Str(const std::string& s): size(s.size()), is_ascii(true) {
        STR_INIT()
    }

    Str(std::string_view s): size(s.size()), is_ascii(true) {
        STR_INIT()
    }

    Str(const char* s): size(strlen(s)), is_ascii(true) {
        STR_INIT()
    }

    Str(const char* s, int len): size(len), is_ascii(true) {
        STR_INIT()
    }

#undef STR_INIT

    Str(const Str& other): size(other.size), is_ascii(other.is_ascii) {
        data = (char*)pool64.alloc(size);
        memcpy(data, other.data, size);
    }

    Str(Str&& other): size(other.size), is_ascii(other.is_ascii), data(other.data) {
        other.data = nullptr;
        other.size = 0;
    }

    const char* begin() const { return data; }
    const char* end() const { return data + size; }
    char operator[](int idx) const { return data[idx]; }
    int length() const { return size; }
    bool empty() const { return size == 0; }
    size_t hash() const{ return std::hash<std::string_view>()(sv()); }

    Str& operator=(const Str& other){
        if(data!=nullptr) pool64.dealloc(data);
        size = other.size;
        is_ascii = other.is_ascii;
        data = (char*)pool64.alloc(size);
        memcpy(data, other.data, size);
        return *this;
    }

    Str& operator=(Str&& other) noexcept{
        if(data!=nullptr) pool64.dealloc(data);
        size = other.size;
        is_ascii = other.is_ascii;
        data = other.data;
        other.data = nullptr;
        return *this;
    }

    ~Str(){
        if(data!=nullptr) pool64.dealloc(data);
    }

    Str operator+(const Str& other) const {
        Str ret(size + other.size, is_ascii && other.is_ascii);
        memcpy(ret.data, data, size);
        memcpy(ret.data + size, other.data, other.size);
        return ret;
    }

    Str operator+(const char* p) const {
        Str other(p);
        return *this + other;
    }

    friend Str operator+(const char* p, const Str& str){
        Str other(p);
        return other + str;
    }

    friend std::ostream& operator<<(std::ostream& os, const Str& str){
        if(str.data!=nullptr) os.write(str.data, str.size);
        return os;
    }

    bool operator==(const Str& other) const {
        if(size != other.size) return false;
        return memcmp(data, other.data, size) == 0;
    }

    bool operator!=(const Str& other) const {
        if(size != other.size) return true;
        return memcmp(data, other.data, size) != 0;
    }

    bool operator<(const Str& other) const {
        int ret = strncmp(data, other.data, std::min(size, other.size));
        if(ret != 0) return ret < 0;
        return size < other.size;
    }

    bool operator<(const std::string_view& other) const {
        int ret = strncmp(data, other.data(), std::min(size, (int)other.size()));
        if(ret != 0) return ret < 0;
        return size < (int)other.size();
    }

    friend bool operator<(const std::string_view& other, const Str& str){
        return str > other;
    }

    bool operator>(const Str& other) const {
        int ret = strncmp(data, other.data, std::min(size, other.size));
        if(ret != 0) return ret > 0;
        return size > other.size;
    }

    bool operator<=(const Str& other) const {
        int ret = strncmp(data, other.data, std::min(size, other.size));
        if(ret != 0) return ret < 0;
        return size <= other.size;
    }

    bool operator>=(const Str& other) const {
        int ret = strncmp(data, other.data, std::min(size, other.size));
        if(ret != 0) return ret > 0;
        return size >= other.size;
    }

    Str substr(int start, int len) const {
        Str ret(len, is_ascii);
        memcpy(ret.data, data + start, len);
        return ret;
    }

    Str substr(int start) const {
        return substr(start, size - start);
    }

    char* c_str_dup() const {
        char* p = (char*)malloc(size + 1);
        memcpy(p, data, size);
        p[size] = 0;
        return p;
    }

    std::string_view sv() const {
        return std::string_view(data, size);
    }

    std::string str() const {
        return std::string(data, size);
    }

    Str lstrip() const {
        std::string copy(data, size);
        copy.erase(copy.begin(), std::find_if(copy.begin(), copy.end(), [](char c) {
            // std::isspace(c) does not working on windows (Debug)
            return c != ' ' && c != '\t' && c != '\r' && c != '\n';
        }));
        return Str(copy);
    }

    Str escape(bool single_quote=true) const {
        std::stringstream ss;
        ss << (single_quote ? '\'' : '"');
        for (int i=0; i<length(); i++) {
            char c = this->operator[](i);
            switch (c) {
                case '"':
                    if(!single_quote) ss << '\\';
                    ss << '"';
                    break;
                case '\'':
                    if(single_quote) ss << '\\';
                    ss << '\'';
                    break;
                case '\\': ss << '\\' << '\\'; break;
                case '\n': ss << "\\n"; break;
                case '\r': ss << "\\r"; break;
                case '\t': ss << "\\t"; break;
                default:
                    if ('\x00' <= c && c <= '\x1f') {
                        ss << "\\x" << std::hex << std::setw(2) << std::setfill('0') << (int)c;
                    } else {
                        ss << c;
                    }
            }
        }
        ss << (single_quote ? '\'' : '"');
        return ss.str();
    }

    int index(const Str& sub, int start=0) const {
        auto p = std::search(data + start, data + size, sub.data, sub.data + sub.size);
        if(p == data + size) return -1;
        return p - data;
    }

    Str replace(const Str& old, const Str& new_, int count=-1) const {
        std::stringstream ss;
        int start = 0;
        while(true){
            int i = index(old, start);
            if(i == -1) break;
            ss << substr(start, i - start);
            ss << new_;
            start = i + old.size;
            if(count != -1 && --count == 0) break;
        }
        ss << substr(start, size - start);
        return ss.str();
    }

    /*************unicode*************/

    int _unicode_index_to_byte(int i) const{
        if(is_ascii) return i;
        int j = 0;
        while(i > 0){
            j += utf8len(data[j]);
            i--;
        }
        return j;
    }

    int _byte_index_to_unicode(int n) const{
        if(is_ascii) return n;
        int cnt = 0;
        for(int i=0; i<n; i++){
            if((data[i] & 0xC0) != 0x80) cnt++;
        }
        return cnt;
    }

    Str u8_getitem(int i) const{
        i = _unicode_index_to_byte(i);
        return substr(i, utf8len(data[i]));
    }

    Str u8_slice(int start, int stop, int step) const{
        std::stringstream ss;
        if(is_ascii){
            for(int i=start; step>0?i<stop:i>stop; i+=step) ss << data[i];
        }else{
            for(int i=start; step>0?i<stop:i>stop; i+=step) ss << u8_getitem(i);
        }
        return ss.str();
    }

    int u8_length() const {
        return _byte_index_to_unicode(size);
    }
};

template<typename... Args>
inline std::string fmt(Args&&... args) {
    std::stringstream ss;
    (ss << ... << args);
    return ss.str();
}

const uint32_t kLoRangeA[] = {170,186,443,448,660,1488,1519,1568,1601,1646,1649,1749,1774,1786,1791,1808,1810,1869,1969,1994,2048,2112,2144,2208,2230,2308,2365,2384,2392,2418,2437,2447,2451,2474,2482,2486,2493,2510,2524,2527,2544,2556,2565,2575,2579,2602,2610,2613,2616,2649,2654,2674,2693,2703,2707,2730,2738,2741,2749,2768,2784,2809,2821,2831,2835,2858,2866,2869,2877,2908,2911,2929,2947,2949,2958,2962,2969,2972,2974,2979,2984,2990,3024,3077,3086,3090,3114,3133,3160,3168,3200,3205,3214,3218,3242,3253,3261,3294,3296,3313,3333,3342,3346,3389,3406,3412,3423,3450,3461,3482,3507,3517,3520,3585,3634,3648,3713,3716,3718,3724,3749,3751,3762,3773,3776,3804,3840,3904,3913,3976,4096,4159,4176,4186,4193,4197,4206,4213,4238,4352,4682,4688,4696,4698,4704,4746,4752,4786,4792,4800,4802,4808,4824,4882,4888,4992,5121,5743,5761,5792,5873,5888,5902,5920,5952,5984,5998,6016,6108,6176,6212,6272,6279,6314,6320,6400,6480,6512,6528,6576,6656,6688,6917,6981,7043,7086,7098,7168,7245,7258,7401,7406,7413,7418,8501,11568,11648,11680,11688,11696,11704,11712,11720,11728,11736,12294,12348,12353,12447,12449,12543,12549,12593,12704,12784,13312,19968,40960,40982,42192,42240,42512,42538,42606,42656,42895,42999,43003,43011,43015,43020,43072,43138,43250,43259,43261,43274,43312,43360,43396,43488,43495,43514,43520,43584,43588,43616,43633,43642,43646,43697,43701,43705,43712,43714,43739,43744,43762,43777,43785,43793,43808,43816,43968,44032,55216,55243,63744,64112,64285,64287,64298,64312,64318,64320,64323,64326,64467,64848,64914,65008,65136,65142,65382,65393,65440,65474,65482,65490,65498,65536,65549,65576,65596,65599,65616,65664,66176,66208,66304,66349,66370,66384,66432,66464,66504,66640,66816,66864,67072,67392,67424,67584,67592,67594,67639,67644,67647,67680,67712,67808,67828,67840,67872,67968,68030,68096,68112,68117,68121,68192,68224,68288,68297,68352,68416,68448,68480,68608,68864,69376,69415,69424,69600,69635,69763,69840,69891,69956,69968,70006,70019,70081,70106,70108,70144,70163,70272,70280,70282,70287,70303,70320,70405,70415,70419,70442,70450,70453,70461,70480,70493,70656,70727,70751,70784,70852,70855,71040,71128,71168,71236,71296,71352,71424,71680,71935,72096,72106,72161,72163,72192,72203,72250,72272,72284,72349,72384,72704,72714,72768,72818,72960,72968,72971,73030,73056,73063,73066,73112,73440,73728,74880,77824,82944,92160,92736,92880,92928,93027,93053,93952,94032,94208,100352,110592,110928,110948,110960,113664,113776,113792,113808,123136,123214,123584,124928,126464,126469,126497,126500,126503,126505,126516,126521,126523,126530,126535,126537,126539,126541,126545,126548,126551,126553,126555,126557,126559,126561,126564,126567,126572,126580,126585,126590,126592,126603,126625,126629,126635,131072,173824,177984,178208,183984,194560};
const uint32_t kLoRangeB[] = {170,186,443,451,660,1514,1522,1599,1610,1647,1747,1749,1775,1788,1791,1808,1839,1957,1969,2026,2069,2136,2154,2228,2237,2361,2365,2384,2401,2432,2444,2448,2472,2480,2482,2489,2493,2510,2525,2529,2545,2556,2570,2576,2600,2608,2611,2614,2617,2652,2654,2676,2701,2705,2728,2736,2739,2745,2749,2768,2785,2809,2828,2832,2856,2864,2867,2873,2877,2909,2913,2929,2947,2954,2960,2965,2970,2972,2975,2980,2986,3001,3024,3084,3088,3112,3129,3133,3162,3169,3200,3212,3216,3240,3251,3257,3261,3294,3297,3314,3340,3344,3386,3389,3406,3414,3425,3455,3478,3505,3515,3517,3526,3632,3635,3653,3714,3716,3722,3747,3749,3760,3763,3773,3780,3807,3840,3911,3948,3980,4138,4159,4181,4189,4193,4198,4208,4225,4238,4680,4685,4694,4696,4701,4744,4749,4784,4789,4798,4800,4805,4822,4880,4885,4954,5007,5740,5759,5786,5866,5880,5900,5905,5937,5969,5996,6000,6067,6108,6210,6264,6276,6312,6314,6389,6430,6509,6516,6571,6601,6678,6740,6963,6987,7072,7087,7141,7203,7247,7287,7404,7411,7414,7418,8504,11623,11670,11686,11694,11702,11710,11718,11726,11734,11742,12294,12348,12438,12447,12538,12543,12591,12686,12730,12799,19893,40943,40980,42124,42231,42507,42527,42539,42606,42725,42895,42999,43009,43013,43018,43042,43123,43187,43255,43259,43262,43301,43334,43388,43442,43492,43503,43518,43560,43586,43595,43631,43638,43642,43695,43697,43702,43709,43712,43714,43740,43754,43762,43782,43790,43798,43814,43822,44002,55203,55238,55291,64109,64217,64285,64296,64310,64316,64318,64321,64324,64433,64829,64911,64967,65019,65140,65276,65391,65437,65470,65479,65487,65495,65500,65547,65574,65594,65597,65613,65629,65786,66204,66256,66335,66368,66377,66421,66461,66499,66511,66717,66855,66915,67382,67413,67431,67589,67592,67637,67640,67644,67669,67702,67742,67826,67829,67861,67897,68023,68031,68096,68115,68119,68149,68220,68252,68295,68324,68405,68437,68466,68497,68680,68899,69404,69415,69445,69622,69687,69807,69864,69926,69956,70002,70006,70066,70084,70106,70108,70161,70187,70278,70280,70285,70301,70312,70366,70412,70416,70440,70448,70451,70457,70461,70480,70497,70708,70730,70751,70831,70853,70855,71086,71131,71215,71236,71338,71352,71450,71723,71935,72103,72144,72161,72163,72192,72242,72250,72272,72329,72349,72440,72712,72750,72768,72847,72966,72969,73008,73030,73061,73064,73097,73112,73458,74649,75075,78894,83526,92728,92766,92909,92975,93047,93071,94026,94032,100343,101106,110878,110930,110951,111355,113770,113788,113800,113817,123180,123214,123627,125124,126467,126495,126498,126500,126503,126514,126519,126521,126523,126530,126535,126537,126539,126543,126546,126548,126551,126553,126555,126557,126559,126562,126564,126570,126578,126583,126588,126590,126601,126619,126627,126633,126651,173782,177972,178205,183969,191456,195101};

inline bool is_unicode_Lo_char(uint32_t c) {
    auto index = std::lower_bound(kLoRangeA, kLoRangeA + 476, c) - kLoRangeA;
    if(c == kLoRangeA[index]) return true;
    index -= 1;
    if(index < 0) return false;
    return c >= kLoRangeA[index] && c <= kLoRangeB[index];
}


struct StrName {
    uint16_t index;
    StrName(): index(0) {}
    StrName(uint16_t index): index(index) {}
    StrName(const char* s): index(get(s).index) {}
    StrName(const Str& s){
        index = get(s.sv()).index;
    }
    std::string_view sv() const { return _r_interned[index-1].sv(); }
    bool empty() const { return index == 0; }

    friend std::ostream& operator<<(std::ostream& os, const StrName& sn){
        return os << sn.sv();
    }

    Str escape() const {
        return _r_interned[index-1].escape();
    }

    bool operator==(const StrName& other) const noexcept {
        return this->index == other.index;
    }

    bool operator!=(const StrName& other) const noexcept {
        return this->index != other.index;
    }

    bool operator<(const StrName& other) const noexcept {
        return this->index < other.index;
    }

    bool operator>(const StrName& other) const noexcept {
        return this->index > other.index;
    }

    static std::map<Str, uint16_t, std::less<>> _interned;
    static std::vector<Str> _r_interned;

    static StrName get(std::string_view s){
        auto it = _interned.find(s);
        if(it != _interned.end()) return StrName(it->second);
        uint16_t index = (uint16_t)(_r_interned.size() + 1);
        _interned[s] = index;
        _r_interned.push_back(s);
        return StrName(index);
    }
};

struct FastStrStream{
    pod_vector<const Str*> parts;

    FastStrStream& operator<<(const Str& s){
        parts.push_back(&s);
        return *this;
    }

    Str str() const{
        int len = 0;
        bool is_ascii = true;
        for(auto& s: parts){
            len += s->length();
            is_ascii &= s->is_ascii;
        }
        Str result(len, is_ascii);
        char* p = result.data;
        for(auto& s: parts){
            memcpy(p, s->data, s->length());
            p += s->length();
        }
        return result;
    }
};

inline std::map<Str, uint16_t, std::less<>> StrName::_interned;
inline std::vector<Str> StrName::_r_interned;

const StrName __class__ = StrName::get("__class__");
const StrName __base__ = StrName::get("__base__");
const StrName __new__ = StrName::get("__new__");
const StrName __iter__ = StrName::get("__iter__");
const StrName __next__ = StrName::get("__next__");
const StrName __str__ = StrName::get("__str__");
const StrName __repr__ = StrName::get("__repr__");
const StrName __getitem__ = StrName::get("__getitem__");
const StrName __setitem__ = StrName::get("__setitem__");
const StrName __delitem__ = StrName::get("__delitem__");
const StrName __contains__ = StrName::get("__contains__");
const StrName __init__ = StrName::get("__init__");
const StrName __json__ = StrName::get("__json__");
const StrName __name__ = StrName::get("__name__");
const StrName __len__ = StrName::get("__len__");
const StrName __get__ = StrName::get("__get__");
const StrName __set__ = StrName::get("__set__");
const StrName __getattr__ = StrName::get("__getattr__");
const StrName __setattr__ = StrName::get("__setattr__");
const StrName __call__ = StrName::get("__call__");
const StrName __doc__ = StrName::get("__doc__");

const StrName m_self = StrName::get("self");
const StrName m_dict = StrName::get("dict");
const StrName m_set = StrName::get("set");
const StrName m_add = StrName::get("add");
const StrName __enter__ = StrName::get("__enter__");
const StrName __exit__ = StrName::get("__exit__");

const StrName __add__ = StrName::get("__add__");
const StrName __sub__ = StrName::get("__sub__");
const StrName __mul__ = StrName::get("__mul__");
// const StrName __truediv__ = StrName::get("__truediv__");
const StrName __floordiv__ = StrName::get("__floordiv__");
const StrName __mod__ = StrName::get("__mod__");
// const StrName __pow__ = StrName::get("__pow__");

const StrName BINARY_SPECIAL_METHODS[] = {
    StrName::get("__add__"), StrName::get("__sub__"), StrName::get("__mul__"),
    StrName::get("__truediv__"), StrName::get("__floordiv__"),
    StrName::get("__mod__"), StrName::get("__pow__")
};

const StrName __lt__ = StrName::get("__lt__");
const StrName __le__ = StrName::get("__le__");
const StrName __eq__ = StrName::get("__eq__");
const StrName __ne__ = StrName::get("__ne__");
const StrName __gt__ = StrName::get("__gt__");
const StrName __ge__ = StrName::get("__ge__");

const StrName __lshift__ = StrName::get("__lshift__");
const StrName __rshift__ = StrName::get("__rshift__");
const StrName __and__ = StrName::get("__and__");
const StrName __or__ = StrName::get("__or__");
const StrName __xor__ = StrName::get("__xor__");

} // namespace pkpy


namespace pkpy {

using List = pod_vector<PyObject*>;

class Tuple {
    PyObject** _args;
    int _size;

    void _alloc(int n){
        this->_args = (n==0) ? nullptr : (PyObject**)pool64.alloc(n * sizeof(void*));
        this->_size = n;
    }

public:
    Tuple(int n){ _alloc(n); }

    Tuple(const Tuple& other){
        _alloc(other._size);
        for(int i=0; i<_size; i++) _args[i] = other._args[i];
    }

    Tuple(Tuple&& other) noexcept {
        this->_args = other._args;
        this->_size = other._size;
        other._args = nullptr;
        other._size = 0;
    }

    Tuple(std::initializer_list<PyObject*> list) : Tuple(list.size()){
        int i = 0;
        for(PyObject* p : list) _args[i++] = p;
    }

    // TODO: poor performance
    // List is allocated by pool128 while tuple is by pool64
    // ...
    Tuple(List&& other) noexcept : Tuple(other.size()){
        for(int i=0; i<_size; i++) _args[i] = other[i];
        other.clear();
    }

    PyObject*& operator[](int i){ return _args[i]; }
    PyObject* operator[](int i) const { return _args[i]; }

    Tuple& operator=(Tuple&& other) noexcept {
        if(_args!=nullptr) pool64.dealloc(_args);
        this->_args = other._args;
        this->_size = other._size;
        other._args = nullptr;
        other._size = 0;
        return *this;
    }

    int size() const { return _size; }

    PyObject** begin() const { return _args; }
    PyObject** end() const { return _args + _size; }

    ~Tuple(){ if(_args!=nullptr) pool64.dealloc(_args); }
};

// a lightweight view for function args, it does not own the memory
struct ArgsView{
    PyObject** _begin;
    PyObject** _end;

    ArgsView(PyObject** begin, PyObject** end) : _begin(begin), _end(end) {}
    ArgsView(const Tuple& t) : _begin(t.begin()), _end(t.end()) {}
    ArgsView(): _begin(nullptr), _end(nullptr) {}

    PyObject** begin() const { return _begin; }
    PyObject** end() const { return _end; }
    int size() const { return _end - _begin; }
    bool empty() const { return _begin == _end; }
    PyObject* operator[](int i) const { return _begin[i]; }

    List to_list() const{
        List ret(size());
        for(int i=0; i<size(); i++) ret[i] = _begin[i];
        return ret;
    }

    Tuple to_tuple() const{
        Tuple ret(size());
        for(int i=0; i<size(); i++) ret[i] = _begin[i];
        return ret;
    }
};
}   // namespace pkpy


namespace pkpy{

const std::vector<uint16_t> kHashSeeds = {9629, 43049, 13267, 59509, 39251, 1249, 35803, 54469, 27689, 9719, 34897, 18973, 30661, 19913, 27919, 32143};

inline uint16_t find_next_power_of_2(uint16_t n){
    uint16_t x = 2;
    while(x < n) x <<= 1;
    return x;
}

#define _hash(key, mask, hash_seed) ( ( (key).index * (hash_seed) >> 8 ) & (mask) )

inline uint16_t find_perfect_hash_seed(uint16_t capacity, const std::vector<StrName>& keys){
    if(keys.empty()) return kHashSeeds[0];
    static std::set<uint16_t> indices;
    indices.clear();
    std::pair<uint16_t, float> best_score = {kHashSeeds[0], 0.0f};
    for(int i=0; i<kHashSeeds.size(); i++){
        indices.clear();
        for(auto key: keys){
            uint16_t index = _hash(key, capacity-1, kHashSeeds[i]);
            indices.insert(index);
        }
        float score = indices.size() / (float)keys.size();
        if(score > best_score.second) best_score = {kHashSeeds[i], score};
    }
    return best_score.first;
}

template<typename T>
struct NameDictImpl {
    using Item = std::pair<StrName, T>;
    static constexpr uint16_t __Capacity = 8;
    // ensure the initial capacity is ok for memory pool
    static_assert(std::is_pod_v<T>);
    static_assert(sizeof(Item) * __Capacity <= 128);

    float _load_factor;
    uint16_t _capacity;
    uint16_t _size;
    uint16_t _hash_seed;
    uint16_t _mask;
    Item* _items;

    void _alloc(int cap){
        _items = (Item*)pool128.alloc(cap * sizeof(Item));
        memset(_items, 0, cap * sizeof(Item));
    }

    NameDictImpl(float load_factor=0.67, uint16_t capacity=__Capacity, uint16_t hash_seed=kHashSeeds[0]):
        _load_factor(load_factor), _capacity(capacity), _size(0), 
        _hash_seed(hash_seed), _mask(capacity-1) {
        _alloc(capacity);
    }

    NameDictImpl(const NameDictImpl& other) {
        memcpy(this, &other, sizeof(NameDictImpl));
        _alloc(_capacity);
        for(int i=0; i<_capacity; i++){
            _items[i] = other._items[i];
        }
    }

    NameDictImpl& operator=(const NameDictImpl& other) {
        pool128.dealloc(_items);
        memcpy(this, &other, sizeof(NameDictImpl));
        _alloc(_capacity);
        for(int i=0; i<_capacity; i++){
            _items[i] = other._items[i];
        }
        return *this;
    }
    
    ~NameDictImpl(){ pool128.dealloc(_items); }

    NameDictImpl(NameDictImpl&&) = delete;
    NameDictImpl& operator=(NameDictImpl&&) = delete;
    uint16_t size() const { return _size; }

#define HASH_PROBE(key, ok, i)          \
ok = false;                             \
i = _hash(key, _mask, _hash_seed);      \
while(!_items[i].first.empty()) {       \
    if(_items[i].first == (key)) { ok = true; break; }  \
    i = (i + 1) & _mask;                                \
}

    T operator[](StrName key) const {
        bool ok; uint16_t i;
        HASH_PROBE(key, ok, i);
        if(!ok) throw std::out_of_range(fmt("NameDict key not found: ", key));
        return _items[i].second;
    }

    void set(StrName key, T val){
        bool ok; uint16_t i;
        HASH_PROBE(key, ok, i);
        if(!ok) {
            _size++;
            if(_size > _capacity*_load_factor){
                _rehash(true);
                HASH_PROBE(key, ok, i);
            }
            _items[i].first = key;
        }
        _items[i].second = val;
    }

    void _rehash(bool resize){
        Item* old_items = _items;
        uint16_t old_capacity = _capacity;
        if(resize){
            _capacity = find_next_power_of_2(_capacity * 2);
            _mask = _capacity - 1;
        }
        _alloc(_capacity);
        for(uint16_t i=0; i<old_capacity; i++){
            if(old_items[i].first.empty()) continue;
            bool ok; uint16_t j;
            HASH_PROBE(old_items[i].first, ok, j);
            if(ok) FATAL_ERROR();
            _items[j] = old_items[i];
        }
        pool128.dealloc(old_items);
    }

    void _try_perfect_rehash(){
        _hash_seed = find_perfect_hash_seed(_capacity, keys());
        _rehash(false); // do not resize
    }

    T try_get(StrName key) const{
        bool ok; uint16_t i;
        HASH_PROBE(key, ok, i);
        if(!ok){
            if constexpr(std::is_pointer_v<T>) return nullptr;
            else if constexpr(std::is_same_v<int, T>) return -1;
            else return Discarded();
        }
        return _items[i].second;
    }

    bool try_set(StrName key, T val){
        bool ok; uint16_t i;
        HASH_PROBE(key, ok, i);
        if(!ok) return false;
        _items[i].second = val;
        return true;
    }

    bool contains(StrName key) const {
        bool ok; uint16_t i;
        HASH_PROBE(key, ok, i);
        return ok;
    }

    void update(const NameDictImpl& other){
        for(uint16_t i=0; i<other._capacity; i++){
            auto& item = other._items[i];
            if(!item.first.empty()) set(item.first, item.second);
        }
    }

    void erase(StrName key){
        bool ok; uint16_t i;
        HASH_PROBE(key, ok, i);
        if(!ok) throw std::out_of_range(fmt("NameDict key not found: ", key));
        _items[i].first = StrName();
        _items[i].second = nullptr;
        _size--;
    }

    std::vector<Item> items() const {
        std::vector<Item> v;
        for(uint16_t i=0; i<_capacity; i++){
            if(_items[i].first.empty()) continue;
            v.push_back(_items[i]);
        }
        return v;
    }

    std::vector<StrName> keys() const {
        std::vector<StrName> v;
        for(uint16_t i=0; i<_capacity; i++){
            if(_items[i].first.empty()) continue;
            v.push_back(_items[i].first);
        }
        return v;
    }

    void clear(){
        for(uint16_t i=0; i<_capacity; i++){
            _items[i].first = StrName();
            _items[i].second = nullptr;
        }
        _size = 0;
    }
#undef HASH_PROBE
#undef _hash
};

using NameDict = NameDictImpl<PyObject*>;
using NameDict_ = shared_ptr<NameDict>;
using NameDictInt = NameDictImpl<int>;

} // namespace pkpy


namespace pkpy{

struct NeedMoreLines {
    NeedMoreLines(bool is_compiling_class) : is_compiling_class(is_compiling_class) {}
    bool is_compiling_class;
};

struct HandledException {};
struct UnhandledException {};
struct ToBeRaisedException {};

enum CompileMode {
    EXEC_MODE,
    EVAL_MODE,
    REPL_MODE,
    JSON_MODE,
};

struct SourceData {
    std::string source;
    Str filename;
    std::vector<const char*> line_starts;
    CompileMode mode;

    std::pair<const char*,const char*> get_line(int lineno) const {
        if(lineno == -1) return {nullptr, nullptr};
        lineno -= 1;
        if(lineno < 0) lineno = 0;
        const char* _start = line_starts.at(lineno);
        const char* i = _start;
        while(*i != '\n' && *i != '\0') i++;
        return {_start, i};
    }

    SourceData(const Str& source, const Str& filename, CompileMode mode) {
        int index = 0;
        // Skip utf8 BOM if there is any.
        if (strncmp(source.begin(), "\xEF\xBB\xBF", 3) == 0) index += 3;
        // Remove all '\r'
        std::stringstream ss;
        while(index < source.length()){
            if(source[index] != '\r') ss << source[index];
            index++;
        }

        this->filename = filename;
        this->source = ss.str();
        line_starts.push_back(this->source.c_str());
        this->mode = mode;
    }

    Str snapshot(int lineno, const char* cursor=nullptr){
        std::stringstream ss;
        ss << "  " << "File \"" << filename << "\", line " << lineno << '\n';
        std::pair<const char*,const char*> pair = get_line(lineno);
        Str line = "<?>";
        int removed_spaces = 0;
        if(pair.first && pair.second){
            line = Str(pair.first, pair.second-pair.first).lstrip();
            removed_spaces = pair.second - pair.first - line.length();
            if(line.empty()) line = "<?>";
        }
        ss << "    " << line;
        if(cursor && line != "<?>" && cursor >= pair.first && cursor <= pair.second){
            auto column = cursor - pair.first - removed_spaces;
            if(column >= 0) ss << "\n    " << std::string(column, ' ') << "^";
        }
        return ss.str();
    }
};

class Exception {
    using StackTrace = stack<Str>;
    StrName type;
    Str msg;
    StackTrace stacktrace;
public:
    Exception(StrName type, Str msg): type(type), msg(msg) {}
    bool match_type(StrName type) const { return this->type == type;}
    bool is_re = true;

    void st_push(Str snapshot){
        if(stacktrace.size() >= 8) return;
        stacktrace.push(snapshot);
    }

    Str summary() const {
        StackTrace st(stacktrace);
        std::stringstream ss;
        if(is_re) ss << "Traceback (most recent call last):\n";
        while(!st.empty()) { ss << st.top() << '\n'; st.pop(); }
        if (!msg.empty()) ss << type.sv() << ": " << msg;
        else ss << type.sv();
        return ss.str();
    }
};

}   // namespace pkpy


namespace pkpy{

typedef uint8_t TokenIndex;

constexpr const char* kTokens[] = {
    "is not", "not in", "yield from",
    "@eof", "@eol", "@sof",
    "@id", "@num", "@str", "@fstr",
    "@indent", "@dedent",
    /*****************************************/
    "+", "+=", "-", "-=",   // (INPLACE_OP - 1) can get '=' removed
    "*", "*=", "/", "/=", "//", "//=", "%", "%=",
    "&", "&=", "|", "|=", "^", "^=", 
    "<<", "<<=", ">>", ">>=",
    /*****************************************/
    ".", ",", ":", ";", "#", "(", ")", "[", "]", "{", "}",
    "**", "=", ">", "<", "...", "->", "?", "@", "==", "!=", ">=", "<=",
    /** KW_BEGIN **/
    "class", "import", "as", "def", "lambda", "pass", "del", "from", "with", "yield",
    "None", "in", "is", "and", "or", "not", "True", "False", "global", "try", "except", "finally",
    "goto", "label",      // extended keywords, not available in cpython
    "while", "for", "if", "elif", "else", "break", "continue", "return", "assert", "raise"
};

using TokenValue = std::variant<std::monostate, i64, f64, Str>;
const TokenIndex kTokenCount = sizeof(kTokens) / sizeof(kTokens[0]);

constexpr TokenIndex TK(const char token[]) {
    for(int k=0; k<kTokenCount; k++){
        const char* i = kTokens[k];
        const char* j = token;
        while(*i && *j && *i == *j) { i++; j++;}
        if(*i == *j) return k;
    }
#ifdef __GNUC__
    // for old version of gcc, it is not smart enough to ignore FATAL_ERROR()
    // so we must do a normal return
    return 255;
#else
    FATAL_ERROR();
#endif
}

#define TK_STR(t) kTokens[t]
const std::map<std::string_view, TokenIndex> kTokenKwMap = [](){
    std::map<std::string_view, TokenIndex> map;
    for(int k=TK("class"); k<kTokenCount; k++) map[kTokens[k]] = k;
    return map;
}();


struct Token{
  TokenIndex type;
  const char* start;
  int length;
  int line;
  TokenValue value;

  Str str() const { return Str(start, length);}
  std::string_view sv() const { return std::string_view(start, length);}

  std::string info() const {
    std::stringstream ss;
    ss << line << ": " << TK_STR(type) << " '" << (
        sv()=="\n" ? "\\n" : sv()
    ) << "'";
    return ss.str();
  }
};

// https://docs.python.org/3/reference/expressions.html#operator-precedence
enum Precedence {
  PREC_NONE,
  PREC_TUPLE,         // ,
  PREC_LAMBDA,        // lambda
  PREC_TERNARY,       // ?:
  PREC_LOGICAL_OR,    // or
  PREC_LOGICAL_AND,   // and
  PREC_LOGICAL_NOT,   // not
  PREC_EQUALITY,      // == !=
  PREC_TEST,          // in / is / is not / not in
  PREC_COMPARISION,   // < > <= >=
  PREC_BITWISE_OR,    // |
  PREC_BITWISE_XOR,   // ^
  PREC_BITWISE_AND,   // &
  PREC_BITWISE_SHIFT, // << >>
  PREC_TERM,          // + -
  PREC_FACTOR,        // * / % //
  PREC_UNARY,         // - not
  PREC_EXPONENT,      // **
  PREC_CALL,          // ()
  PREC_SUBSCRIPT,     // []
  PREC_ATTRIB,        // .index
  PREC_PRIMARY,
};

enum StringType { NORMAL_STRING, RAW_STRING, F_STRING };

struct Lexer {
    shared_ptr<SourceData> src;
    const char* token_start;
    const char* curr_char;
    int current_line = 1;
    std::vector<Token> nexts;
    stack<int> indents;
    int brackets_level = 0;
    bool used = false;

    char peekchar() const{ return *curr_char; }

    bool match_n_chars(int n, char c0){
        const char* c = curr_char;
        for(int i=0; i<n; i++){
            if(*c == '\0') return false;
            if(*c != c0) return false;
            c++;
        }
        for(int i=0; i<n; i++) eatchar_include_newline();
        return true;
    }

    int eat_spaces(){
        int count = 0;
        while (true) {
            switch (peekchar()) {
                case ' ' : count+=1; break;
                case '\t': count+=4; break;
                default: return count;
            }
            eatchar();
        }
    }

    bool eat_indentation(){
        if(brackets_level > 0) return true;
        int spaces = eat_spaces();
        if(peekchar() == '#') skip_line_comment();
        if(peekchar() == '\0' || peekchar() == '\n') return true;
        // https://docs.python.org/3/reference/lexical_analysis.html#indentation
        if(spaces > indents.top()){
            indents.push(spaces);
            nexts.push_back(Token{TK("@indent"), token_start, 0, current_line});
        } else if(spaces < indents.top()){
            while(spaces < indents.top()){
                indents.pop();
                nexts.push_back(Token{TK("@dedent"), token_start, 0, current_line});
            }
            if(spaces != indents.top()){
                return false;
            }
        }
        return true;
    }

    char eatchar() {
        char c = peekchar();
        if(c == '\n') throw std::runtime_error("eatchar() cannot consume a newline");
        curr_char++;
        return c;
    }

    char eatchar_include_newline() {
        char c = peekchar();
        curr_char++;
        if (c == '\n'){
            current_line++;
            src->line_starts.push_back(curr_char);
        }
        return c;
    }

    int eat_name() {
        curr_char--;
        while(true){
            unsigned char c = peekchar();
            int u8bytes = utf8len(c, true);
            if(u8bytes == 0) return 1;
            if(u8bytes == 1){
                if(isalpha(c) || c=='_' || isdigit(c)) {
                    curr_char++;
                    continue;
                }else{
                    break;
                }
            }
            // handle multibyte char
            std::string u8str(curr_char, u8bytes);
            if(u8str.size() != u8bytes) return 2;
            uint32_t value = 0;
            for(int k=0; k < u8bytes; k++){
                uint8_t b = u8str[k];
                if(k==0){
                    if(u8bytes == 2) value = (b & 0b00011111) << 6;
                    else if(u8bytes == 3) value = (b & 0b00001111) << 12;
                    else if(u8bytes == 4) value = (b & 0b00000111) << 18;
                }else{
                    value |= (b & 0b00111111) << (6*(u8bytes-k-1));
                }
            }
            if(is_unicode_Lo_char(value)) curr_char += u8bytes;
            else break;
        }

        int length = (int)(curr_char - token_start);
        if(length == 0) return 3;
        std::string_view name(token_start, length);

        if(src->mode == JSON_MODE){
            if(name == "true"){
                add_token(TK("True"));
            } else if(name == "false"){
                add_token(TK("False"));
            } else if(name == "null"){
                add_token(TK("None"));
            } else {
                return 4;
            }
            return 0;
        }

        if(kTokenKwMap.count(name)){
            if(name == "not"){
                if(strncmp(curr_char, " in", 3) == 0){
                    curr_char += 3;
                    add_token(TK("not in"));
                    return 0;
                }
            }else if(name == "is"){
                if(strncmp(curr_char, " not", 4) == 0){
                    curr_char += 4;
                    add_token(TK("is not"));
                    return 0;
                }
            }else if(name == "yield"){
                if(strncmp(curr_char, " from", 5) == 0){
                    curr_char += 5;
                    add_token(TK("yield from"));
                    return 0;
                }
            }
            add_token(kTokenKwMap.at(name));
        } else {
            add_token(TK("@id"));
        }
        return 0;
    }

    void skip_line_comment() {
        char c;
        while ((c = peekchar()) != '\0') {
            if (c == '\n') return;
            eatchar();
        }
    }
    
    bool matchchar(char c) {
        if (peekchar() != c) return false;
        eatchar_include_newline();
        return true;
    }

    void add_token(TokenIndex type, TokenValue value={}) {
        switch(type){
            case TK("{"): case TK("["): case TK("("): brackets_level++; break;
            case TK(")"): case TK("]"): case TK("}"): brackets_level--; break;
        }
        nexts.push_back( Token{
            type,
            token_start,
            (int)(curr_char - token_start),
            current_line - ((type == TK("@eol")) ? 1 : 0),
            value
        });
    }

    void add_token_2(char c, TokenIndex one, TokenIndex two) {
        if (matchchar(c)) add_token(two);
        else add_token(one);
    }

    Str eat_string_until(char quote, bool raw) {
        bool quote3 = match_n_chars(2, quote);
        std::vector<char> buff;
        while (true) {
            char c = eatchar_include_newline();
            if (c == quote){
                if(quote3 && !match_n_chars(2, quote)){
                    buff.push_back(c);
                    continue;
                }
                break;
            }
            if (c == '\0'){
                if(quote3 && src->mode == REPL_MODE){
                    throw NeedMoreLines(false);
                }
                SyntaxError("EOL while scanning string literal");
            }
            if (c == '\n'){
                if(!quote3) SyntaxError("EOL while scanning string literal");
                else{
                    buff.push_back(c);
                    continue;
                }
            }
            if (!raw && c == '\\') {
                switch (eatchar_include_newline()) {
                    case '"':  buff.push_back('"');  break;
                    case '\'': buff.push_back('\''); break;
                    case '\\': buff.push_back('\\'); break;
                    case 'n':  buff.push_back('\n'); break;
                    case 'r':  buff.push_back('\r'); break;
                    case 't':  buff.push_back('\t'); break;
                    default: SyntaxError("invalid escape char");
                }
            } else {
                buff.push_back(c);
            }
        }
        return Str(buff.data(), buff.size());
    }

    void eat_string(char quote, StringType type) {
        Str s = eat_string_until(quote, type == RAW_STRING);
        if(type == F_STRING){
            add_token(TK("@fstr"), s);
        }else{
            add_token(TK("@str"), s);
        }
    }

    void eat_number() {
        static const std::regex pattern("^(0x)?[0-9a-fA-F]+(\\.[0-9]+)?");
        std::smatch m;

        const char* i = token_start;
        while(*i != '\n' && *i != '\0') i++;
        std::string s = std::string(token_start, i);

        try{
            if (std::regex_search(s, m, pattern)) {
                // here is m.length()-1, since the first char was eaten by lex_token()
                for(int j=0; j<m.length()-1; j++) eatchar();

                int base = 10;
                size_t size;
                if (m[1].matched) base = 16;
                if (m[2].matched) {
                    if(base == 16) SyntaxError("hex literal should not contain a dot");
                    add_token(TK("@num"), Number::stof(m[0], &size));
                } else {
                    add_token(TK("@num"), Number::stoi(m[0], &size, base));
                }
                if (size != m.length()) FATAL_ERROR();
            }
        }catch(std::exception& _){
            SyntaxError("invalid number literal");
        } 
    }

    bool lex_one_token() {
        while (peekchar() != '\0') {
            token_start = curr_char;
            char c = eatchar_include_newline();
            switch (c) {
                case '\'': case '"': eat_string(c, NORMAL_STRING); return true;
                case '#': skip_line_comment(); break;
                case '{': add_token(TK("{")); return true;
                case '}': add_token(TK("}")); return true;
                case ',': add_token(TK(",")); return true;
                case ':': add_token(TK(":")); return true;
                case ';': add_token(TK(";")); return true;
                case '(': add_token(TK("(")); return true;
                case ')': add_token(TK(")")); return true;
                case '[': add_token(TK("[")); return true;
                case ']': add_token(TK("]")); return true;
                case '@': add_token(TK("@")); return true;
                case '%': add_token_2('=', TK("%"), TK("%=")); return true;
                case '&': add_token_2('=', TK("&"), TK("&=")); return true;
                case '|': add_token_2('=', TK("|"), TK("|=")); return true;
                case '^': add_token_2('=', TK("^"), TK("^=")); return true;
                case '?': add_token(TK("?")); return true;
                case '.': {
                    if(matchchar('.')) {
                        if(matchchar('.')) {
                            add_token(TK("..."));
                        } else {
                            SyntaxError("invalid token '..'");
                        }
                    } else {
                        add_token(TK("."));
                    }
                    return true;
                }
                case '=': add_token_2('=', TK("="), TK("==")); return true;
                case '+': add_token_2('=', TK("+"), TK("+=")); return true;
                case '>': {
                    if(matchchar('=')) add_token(TK(">="));
                    else if(matchchar('>')) add_token_2('=', TK(">>"), TK(">>="));
                    else add_token(TK(">"));
                    return true;
                }
                case '<': {
                    if(matchchar('=')) add_token(TK("<="));
                    else if(matchchar('<')) add_token_2('=', TK("<<"), TK("<<="));
                    else add_token(TK("<"));
                    return true;
                }
                case '-': {
                    if(matchchar('=')) add_token(TK("-="));
                    else if(matchchar('>')) add_token(TK("->"));
                    else add_token(TK("-"));
                    return true;
                }
                case '!':
                    if(matchchar('=')) add_token(TK("!="));
                    else SyntaxError("expected '=' after '!'");
                    break;
                case '*':
                    if (matchchar('*')) {
                        add_token(TK("**"));  // '**'
                    } else {
                        add_token_2('=', TK("*"), TK("*="));
                    }
                    return true;
                case '/':
                    if(matchchar('/')) {
                        add_token_2('=', TK("//"), TK("//="));
                    } else {
                        add_token_2('=', TK("/"), TK("/="));
                    }
                    return true;
                case ' ': case '\t': eat_spaces(); break;
                case '\n': {
                    add_token(TK("@eol"));
                    if(!eat_indentation()) IndentationError("unindent does not match any outer indentation level");
                    return true;
                }
                default: {
                    if(c == 'f'){
                        if(matchchar('\'')) {eat_string('\'', F_STRING); return true;}
                        if(matchchar('"')) {eat_string('"', F_STRING); return true;}
                    }else if(c == 'r'){
                        if(matchchar('\'')) {eat_string('\'', RAW_STRING); return true;}
                        if(matchchar('"')) {eat_string('"', RAW_STRING); return true;}
                    }
                    if (c >= '0' && c <= '9') {
                        eat_number();
                        return true;
                    }
                    switch (eat_name())
                    {
                        case 0: break;
                        case 1: SyntaxError("invalid char: " + std::string(1, c));
                        case 2: SyntaxError("invalid utf8 sequence: " + std::string(1, c));
                        case 3: SyntaxError("@id contains invalid char"); break;
                        case 4: SyntaxError("invalid JSON token"); break;
                        default: FATAL_ERROR();
                    }
                    return true;
                }
            }
        }

        token_start = curr_char;
        while(indents.size() > 1){
            indents.pop();
            add_token(TK("@dedent"));
            return true;
        }
        add_token(TK("@eof"));
        return false;
    }

    /***** Error Reporter *****/
    void throw_err(Str type, Str msg){
        int lineno = current_line;
        const char* cursor = curr_char;
        if(peekchar() == '\n'){
            lineno--;
            cursor--;
        }
        throw_err(type, msg, lineno, cursor);
    }

    void throw_err(Str type, Str msg, int lineno, const char* cursor){
        auto e = Exception("SyntaxError", msg);
        e.st_push(src->snapshot(lineno, cursor));
        throw e;
    }
    void SyntaxError(Str msg){ throw_err("SyntaxError", msg); }
    void SyntaxError(){ throw_err("SyntaxError", "invalid syntax"); }
    void IndentationError(Str msg){ throw_err("IndentationError", msg); }

    Lexer(shared_ptr<SourceData> src) {
        this->src = src;
        this->token_start = src->source.c_str();
        this->curr_char = src->source.c_str();
        this->nexts.push_back(Token{TK("@sof"), token_start, 0, current_line});
        this->indents.push(0);
    }

    std::vector<Token> run() {
        if(used) FATAL_ERROR();
        used = true;
        while (lex_one_token());
        return std::move(nexts);
    }
};

} // namespace pkpy



namespace pkpy {
    
struct CodeObject;
struct Frame;
struct Function;
class VM;

typedef PyObject* (*NativeFuncC)(VM*, ArgsView);
typedef int (*LuaStyleFuncC)(VM*);

struct NativeFunc {
    NativeFuncC f;
    int argc;       // DONOT include self
    bool method;

    // this is designed for lua style C bindings
    // access it via `CAST(NativeFunc&, args[-2])._lua_f`
    LuaStyleFuncC _lua_f;
    
    NativeFunc(NativeFuncC f, int argc, bool method) : f(f), argc(argc), method(method), _lua_f(nullptr) {}
    PyObject* operator()(VM* vm, ArgsView args) const;
};

typedef shared_ptr<CodeObject> CodeObject_;

struct FuncDecl {
    struct KwArg {
        int key;                // index in co->varnames
        PyObject* value;        // default value
    };
    CodeObject_ code;           // code object of this function
    pod_vector<int> args;       // indices in co->varnames
    int starred_arg = -1;       // index in co->varnames, -1 if no *arg
    pod_vector<KwArg> kwargs;   // indices in co->varnames
    bool nested = false;        // whether this function is nested
    void _gc_mark() const;
};

using FuncDecl_ = shared_ptr<FuncDecl>;

struct Function{
    FuncDecl_ decl;
    bool is_simple;
    PyObject* _module;
    NameDict_ _closure;
};

struct BoundMethod {
    PyObject* self;
    PyObject* func;
    BoundMethod(PyObject* self, PyObject* func) : self(self), func(func) {}
};

struct Range {
    i64 start = 0;
    i64 stop = -1;
    i64 step = 1;
};

struct Bytes{
    std::vector<char> _data;
    bool _ok;

    int size() const noexcept { return _data.size(); }
    int operator[](int i) const noexcept { return (int)(uint8_t)_data[i]; }
    const char* data() const noexcept { return _data.data(); }

    bool operator==(const Bytes& rhs) const noexcept {
        return _data == rhs._data;
    }
    bool operator!=(const Bytes& rhs) const noexcept {
        return _data != rhs._data;
    }

    std::string str() const noexcept { return std::string(_data.begin(), _data.end()); }

    Bytes() : _data(), _ok(false) {}
    Bytes(std::vector<char>&& data) : _data(std::move(data)), _ok(true) {}
    Bytes(const std::string& data) : _data(data.begin(), data.end()), _ok(true) {}
    operator bool() const noexcept { return _ok; }
};

using Super = std::pair<PyObject*, Type>;

struct Slice {
    PyObject* start;
    PyObject* stop;
    PyObject* step;
    Slice(PyObject* start, PyObject* stop, PyObject* step) : start(start), stop(stop), step(step) {}
};

class BaseIter {
protected:
    VM* vm;
public:
    BaseIter(VM* vm) : vm(vm) {}
    virtual PyObject* next() = 0;
    virtual ~BaseIter() = default;
};

struct GCHeader {
    bool enabled;   // whether this object is managed by GC
    bool marked;    // whether this object is marked
    GCHeader() : enabled(true), marked(false) {}
};

struct PyObject{
    GCHeader gc;
    Type type;
    NameDict* _attr;

    bool is_attr_valid() const noexcept { return _attr != nullptr; }
    NameDict& attr() noexcept { return *_attr; }
    PyObject* attr(StrName name) const noexcept { return (*_attr)[name]; }
    virtual void* value() = 0;
    virtual void _obj_gc_mark() = 0;

    PyObject(Type type) : type(type), _attr(nullptr) {}

    virtual ~PyObject() {
        if(_attr == nullptr) return;
        _attr->~NameDict();
        pool64.dealloc(_attr);
    }

    void enable_instance_dict(float lf=kInstAttrLoadFactor) noexcept {
        _attr = new(pool64.alloc<NameDict>()) NameDict(lf);
    }
};

template <typename, typename=void> struct has_gc_marker : std::false_type {};
template <typename T> struct has_gc_marker<T, std::void_t<decltype(&T::_gc_mark)>> : std::true_type {};

template <typename T>
struct Py_ final: PyObject {
    T _value;
    void* value() override { return &_value; }
    void _obj_gc_mark() override {
        if constexpr (has_gc_marker<T>::value) {
            _value._gc_mark();
        }
    }
    Py_(Type type, const T& value) : PyObject(type), _value(value) {}
    Py_(Type type, T&& value) : PyObject(type), _value(std::move(value)) {}
};

struct MappingProxy{
    PyObject* obj;
    MappingProxy(PyObject* obj) : obj(obj) {}
    NameDict& attr() noexcept { return obj->attr(); }
};

#define OBJ_GET(T, obj) (((Py_<T>*)(obj))->_value)
// #define OBJ_GET(T, obj) (*reinterpret_cast<T*>((obj)->value()))

#define OBJ_MARK(obj) \
    if(!is_tagged(obj) && !(obj)->gc.marked) {                      \
        (obj)->gc.marked = true;                                    \
        (obj)->_obj_gc_mark();                                      \
        if((obj)->is_attr_valid()) gc_mark_namedict((obj)->attr()); \
    }

inline void gc_mark_namedict(NameDict& t){
    if(t.size() == 0) return;
    for(uint16_t i=0; i<t._capacity; i++){
        if(t._items[i].first.empty()) continue;
        OBJ_MARK(t._items[i].second);
    }
}

Str obj_type_name(VM* vm, Type type);

#if DEBUG_NO_BUILTIN_MODULES
#define OBJ_NAME(obj) Str("<?>")
#else
#define OBJ_NAME(obj) OBJ_GET(Str, vm->getattr(obj, __name__))
#endif

const int kTpIntIndex = 2;
const int kTpFloatIndex = 3;

inline bool is_type(PyObject* obj, Type type) {
#if DEBUG_EXTRA_CHECK
    if(obj == nullptr) throw std::runtime_error("is_type() called with nullptr");
    if(is_special(obj)) throw std::runtime_error("is_type() called with special object");
#endif
    switch(type.index){
        case kTpIntIndex: return is_int(obj);
        case kTpFloatIndex: return is_float(obj);
        default: return !is_tagged(obj) && obj->type == type;
    }
}

inline bool is_non_tagged_type(PyObject* obj, Type type) {
#if DEBUG_EXTRA_CHECK
    if(obj == nullptr) throw std::runtime_error("is_non_tagged_type() called with nullptr");
    if(is_special(obj)) throw std::runtime_error("is_non_tagged_type() called with special object");
#endif
    return !is_tagged(obj) && obj->type == type;
}

union BitsCvt {
    i64 _int;
    f64 _float;
    BitsCvt(i64 val) : _int(val) {}
    BitsCvt(f64 val) : _float(val) {}
};

template <typename, typename=void> struct is_py_class : std::false_type {};
template <typename T> struct is_py_class<T, std::void_t<decltype(T::_type)>> : std::true_type {};

template<typename T> void _check_py_class(VM*, PyObject*);
template<typename T> T py_pointer_cast(VM*, PyObject*);

template<typename __T>
__T py_cast(VM* vm, PyObject* obj) {
    using T = std::decay_t<__T>;
    if constexpr(std::is_pointer_v<T>){
        return py_pointer_cast<T>(vm, obj);
    }else if constexpr(is_py_class<T>::value){
        _check_py_class<T>(vm, obj);
        return OBJ_GET(T, obj);
    }else {
        return Discarded();
    }
}

template<typename __T>
__T _py_cast(VM* vm, PyObject* obj) {
    using T = std::decay_t<__T>;
    if constexpr(std::is_pointer_v<__T>){
        return py_pointer_cast<__T>(vm, obj);
    }else if constexpr(is_py_class<T>::value){
        return OBJ_GET(T, obj);
    }else{
        return Discarded();
    }
}

#define VAR(x) py_var(vm, x)
#define CAST(T, x) py_cast<T>(vm, x)
#define _CAST(T, x) _py_cast<T>(vm, x)

/*****************************************************************/
template<>
struct Py_<List> final: PyObject {
    List _value;
    void* value() override { return &_value; }

    Py_(Type type, List&& val): PyObject(type), _value(std::move(val)) {}
    Py_(Type type, const List& val): PyObject(type), _value(val) {}

    void _obj_gc_mark() override {
        for(PyObject* obj: _value) OBJ_MARK(obj);
    }
};

template<>
struct Py_<Tuple> final: PyObject {
    Tuple _value;
    void* value() override { return &_value; }

    Py_(Type type, Tuple&& val): PyObject(type), _value(std::move(val)) {}
    Py_(Type type, const Tuple& val): PyObject(type), _value(val) {}

    void _obj_gc_mark() override {
        for(PyObject* obj: _value) OBJ_MARK(obj);
    }
};

template<>
struct Py_<MappingProxy> final: PyObject {
    MappingProxy _value;
    void* value() override { return &_value; }
    Py_(Type type, MappingProxy val): PyObject(type), _value(val) {}
    void _obj_gc_mark() override {
        OBJ_MARK(_value.obj);
    }
};

template<>
struct Py_<BoundMethod> final: PyObject {
    BoundMethod _value;
    void* value() override { return &_value; }
    Py_(Type type, BoundMethod val): PyObject(type), _value(val) {}
    void _obj_gc_mark() override {
        OBJ_MARK(_value.self);
        OBJ_MARK(_value.func);
    }
};

template<>
struct Py_<Slice> final: PyObject {
    Slice _value;
    void* value() override { return &_value; }
    Py_(Type type, Slice val): PyObject(type), _value(val) {}
    void _obj_gc_mark() override {
        OBJ_MARK(_value.start);
        OBJ_MARK(_value.stop);
        OBJ_MARK(_value.step);
    }
};

template<>
struct Py_<Function> final: PyObject {
    Function _value;
    void* value() override { return &_value; }
    Py_(Type type, Function val): PyObject(type), _value(val) {
        enable_instance_dict();
    }
    void _obj_gc_mark() override {
        _value.decl->_gc_mark();
        if(_value._module != nullptr) OBJ_MARK(_value._module);
        if(_value._closure != nullptr) gc_mark_namedict(*_value._closure);
    }
};

template<>
struct Py_<NativeFunc> final: PyObject {
    NativeFunc _value;
    void* value() override { return &_value; }
    Py_(Type type, NativeFunc val): PyObject(type), _value(val) {
        enable_instance_dict();
    }
    void _obj_gc_mark() override {}
};

template<>
struct Py_<Super> final: PyObject {
    Super _value;
    void* value() override { return &_value; }
    Py_(Type type, Super val): PyObject(type), _value(val) {}
    void _obj_gc_mark() override {
        OBJ_MARK(_value.first);
    }
};

template<>
struct Py_<DummyInstance> final: PyObject {
    void* value() override { return nullptr; }
    Py_(Type type, DummyInstance val): PyObject(type) {
        enable_instance_dict();
    }
    void _obj_gc_mark() override {}
};

template<>
struct Py_<Type> final: PyObject {
    Type _value;
    void* value() override { return &_value; }
    Py_(Type type, Type val): PyObject(type), _value(val) {
        enable_instance_dict(kTypeAttrLoadFactor);
    }
    void _obj_gc_mark() override {}
};

template<>
struct Py_<DummyModule> final: PyObject {
    void* value() override { return nullptr; }
    Py_(Type type, DummyModule val): PyObject(type) {
        enable_instance_dict(kTypeAttrLoadFactor);
    }
    void _obj_gc_mark() override {}
};


}   // namespace pkpy


namespace pkpy{

enum NameScope { NAME_LOCAL, NAME_GLOBAL, NAME_GLOBAL_UNKNOWN };

enum Opcode {
    #define OPCODE(name) OP_##name,
    #ifdef OPCODE

/**************************/
OPCODE(NO_OP)
/**************************/
OPCODE(POP_TOP)
OPCODE(DUP_TOP)
OPCODE(ROT_TWO)
OPCODE(PRINT_EXPR)
/**************************/
OPCODE(LOAD_CONST)
OPCODE(LOAD_NONE)
OPCODE(LOAD_TRUE)
OPCODE(LOAD_FALSE)
OPCODE(LOAD_INTEGER)
OPCODE(LOAD_ELLIPSIS)
OPCODE(LOAD_FUNCTION)
OPCODE(LOAD_NULL)
/**************************/
OPCODE(LOAD_FAST)
OPCODE(LOAD_NAME)
OPCODE(LOAD_NONLOCAL)
OPCODE(LOAD_GLOBAL)
OPCODE(LOAD_ATTR)
OPCODE(LOAD_METHOD)
OPCODE(LOAD_SUBSCR)

OPCODE(STORE_FAST)
OPCODE(STORE_NAME)
OPCODE(STORE_GLOBAL)
OPCODE(STORE_ATTR)
OPCODE(STORE_SUBSCR)

OPCODE(DELETE_FAST)
OPCODE(DELETE_NAME)
OPCODE(DELETE_GLOBAL)
OPCODE(DELETE_ATTR)
OPCODE(DELETE_SUBSCR)
/**************************/
OPCODE(BUILD_LIST)
OPCODE(BUILD_DICT)
OPCODE(BUILD_SET)
OPCODE(BUILD_SLICE)
OPCODE(BUILD_TUPLE)
OPCODE(BUILD_STRING)
/**************************/
OPCODE(BINARY_OP)
OPCODE(BINARY_ADD)
OPCODE(BINARY_SUB)
OPCODE(BINARY_MUL)
OPCODE(BINARY_FLOORDIV)
OPCODE(BINARY_MOD)

OPCODE(COMPARE_LT)
OPCODE(COMPARE_LE)
OPCODE(COMPARE_EQ)
OPCODE(COMPARE_NE)
OPCODE(COMPARE_GT)
OPCODE(COMPARE_GE)

OPCODE(BITWISE_LSHIFT)
OPCODE(BITWISE_RSHIFT)
OPCODE(BITWISE_AND)
OPCODE(BITWISE_OR)
OPCODE(BITWISE_XOR)

OPCODE(IS_OP)
OPCODE(CONTAINS_OP)
/**************************/
OPCODE(JUMP_ABSOLUTE)
OPCODE(POP_JUMP_IF_FALSE)
OPCODE(JUMP_IF_TRUE_OR_POP)
OPCODE(JUMP_IF_FALSE_OR_POP)
OPCODE(LOOP_CONTINUE)
OPCODE(LOOP_BREAK)
OPCODE(GOTO)
/**************************/
OPCODE(BEGIN_CALL)
OPCODE(CALL)
OPCODE(RETURN_VALUE)
OPCODE(YIELD_VALUE)
/**************************/
OPCODE(LIST_APPEND)
OPCODE(DICT_ADD)
OPCODE(SET_ADD)
/**************************/
OPCODE(UNARY_NEGATIVE)
OPCODE(UNARY_NOT)
/**************************/
OPCODE(GET_ITER)
OPCODE(FOR_ITER)
/**************************/
OPCODE(IMPORT_NAME)
OPCODE(IMPORT_STAR)
/**************************/
OPCODE(UNPACK_SEQUENCE)
OPCODE(UNPACK_EX)
OPCODE(UNPACK_UNLIMITED)
/**************************/
OPCODE(BEGIN_CLASS)
OPCODE(END_CLASS)
OPCODE(STORE_CLASS_ATTR)
/**************************/
OPCODE(WITH_ENTER)
OPCODE(WITH_EXIT)
/**************************/
OPCODE(ASSERT)
OPCODE(EXCEPTION_MATCH)
OPCODE(RAISE)
OPCODE(RE_RAISE)
/**************************/
OPCODE(SETUP_DOCSTRING)
OPCODE(FORMAT_STRING)
#endif
    #undef OPCODE
};

inline const char* OP_NAMES[] = {
    #define OPCODE(name) #name,
    #ifdef OPCODE

/**************************/
OPCODE(NO_OP)
/**************************/
OPCODE(POP_TOP)
OPCODE(DUP_TOP)
OPCODE(ROT_TWO)
OPCODE(PRINT_EXPR)
/**************************/
OPCODE(LOAD_CONST)
OPCODE(LOAD_NONE)
OPCODE(LOAD_TRUE)
OPCODE(LOAD_FALSE)
OPCODE(LOAD_INTEGER)
OPCODE(LOAD_ELLIPSIS)
OPCODE(LOAD_FUNCTION)
OPCODE(LOAD_NULL)
/**************************/
OPCODE(LOAD_FAST)
OPCODE(LOAD_NAME)
OPCODE(LOAD_NONLOCAL)
OPCODE(LOAD_GLOBAL)
OPCODE(LOAD_ATTR)
OPCODE(LOAD_METHOD)
OPCODE(LOAD_SUBSCR)

OPCODE(STORE_FAST)
OPCODE(STORE_NAME)
OPCODE(STORE_GLOBAL)
OPCODE(STORE_ATTR)
OPCODE(STORE_SUBSCR)

OPCODE(DELETE_FAST)
OPCODE(DELETE_NAME)
OPCODE(DELETE_GLOBAL)
OPCODE(DELETE_ATTR)
OPCODE(DELETE_SUBSCR)
/**************************/
OPCODE(BUILD_LIST)
OPCODE(BUILD_DICT)
OPCODE(BUILD_SET)
OPCODE(BUILD_SLICE)
OPCODE(BUILD_TUPLE)
OPCODE(BUILD_STRING)
/**************************/
OPCODE(BINARY_OP)
OPCODE(BINARY_ADD)
OPCODE(BINARY_SUB)
OPCODE(BINARY_MUL)
OPCODE(BINARY_FLOORDIV)
OPCODE(BINARY_MOD)

OPCODE(COMPARE_LT)
OPCODE(COMPARE_LE)
OPCODE(COMPARE_EQ)
OPCODE(COMPARE_NE)
OPCODE(COMPARE_GT)
OPCODE(COMPARE_GE)

OPCODE(BITWISE_LSHIFT)
OPCODE(BITWISE_RSHIFT)
OPCODE(BITWISE_AND)
OPCODE(BITWISE_OR)
OPCODE(BITWISE_XOR)

OPCODE(IS_OP)
OPCODE(CONTAINS_OP)
/**************************/
OPCODE(JUMP_ABSOLUTE)
OPCODE(POP_JUMP_IF_FALSE)
OPCODE(JUMP_IF_TRUE_OR_POP)
OPCODE(JUMP_IF_FALSE_OR_POP)
OPCODE(LOOP_CONTINUE)
OPCODE(LOOP_BREAK)
OPCODE(GOTO)
/**************************/
OPCODE(BEGIN_CALL)
OPCODE(CALL)
OPCODE(RETURN_VALUE)
OPCODE(YIELD_VALUE)
/**************************/
OPCODE(LIST_APPEND)
OPCODE(DICT_ADD)
OPCODE(SET_ADD)
/**************************/
OPCODE(UNARY_NEGATIVE)
OPCODE(UNARY_NOT)
/**************************/
OPCODE(GET_ITER)
OPCODE(FOR_ITER)
/**************************/
OPCODE(IMPORT_NAME)
OPCODE(IMPORT_STAR)
/**************************/
OPCODE(UNPACK_SEQUENCE)
OPCODE(UNPACK_EX)
OPCODE(UNPACK_UNLIMITED)
/**************************/
OPCODE(BEGIN_CLASS)
OPCODE(END_CLASS)
OPCODE(STORE_CLASS_ATTR)
/**************************/
OPCODE(WITH_ENTER)
OPCODE(WITH_EXIT)
/**************************/
OPCODE(ASSERT)
OPCODE(EXCEPTION_MATCH)
OPCODE(RAISE)
OPCODE(RE_RAISE)
/**************************/
OPCODE(SETUP_DOCSTRING)
OPCODE(FORMAT_STRING)
#endif
    #undef OPCODE
};

struct Bytecode{
    uint16_t op;
    uint16_t block;
    int arg;
};

enum CodeBlockType {
    NO_BLOCK,
    FOR_LOOP,
    WHILE_LOOP,
    CONTEXT_MANAGER,
    TRY_EXCEPT,
};

#define BC_NOARG       -1
#define BC_KEEPLINE     -1

struct CodeBlock {
    CodeBlockType type;
    int parent;         // parent index in blocks
    int for_loop_depth; // this is used for exception handling
    int start;          // start index of this block in codes, inclusive
    int end;            // end index of this block in codes, exclusive

    CodeBlock(CodeBlockType type, int parent, int for_loop_depth, int start):
        type(type), parent(parent), for_loop_depth(for_loop_depth), start(start), end(-1) {}
};

struct CodeObject {
    shared_ptr<SourceData> src;
    Str name;
    bool is_generator = false;

    CodeObject(shared_ptr<SourceData> src, const Str& name):
        src(src), name(name) {}

    std::vector<Bytecode> codes;
    std::vector<int> lines; // line number for each bytecode
    List consts;
    std::vector<StrName> varnames;      // local variables
    NameDictInt varnames_inv;
    std::set<Str> global_names;
    std::vector<CodeBlock> blocks = { CodeBlock(NO_BLOCK, -1, 0, 0) };
    NameDictInt labels;
    std::vector<FuncDecl_> func_decls;

    void optimize(VM* vm);

    void _gc_mark() const {
        for(PyObject* v : consts) OBJ_MARK(v);
        for(auto& decl: func_decls) decl->_gc_mark();
    }
};

} // namespace pkpy


namespace pkpy{

// weak reference fast locals
struct FastLocals{
    // this is a weak reference
    const NameDictInt* varnames_inv;
    PyObject** a;

    int size() const{
        return varnames_inv->size();
    }

    PyObject*& operator[](int i){ return a[i]; }
    PyObject* operator[](int i) const { return a[i]; }

    FastLocals(const CodeObject* co, PyObject** a): varnames_inv(&co->varnames_inv), a(a) {}
    FastLocals(const FastLocals& other): varnames_inv(other.varnames_inv), a(other.a) {}

    PyObject* try_get(StrName name){
        int index = varnames_inv->try_get(name);
        if(index == -1) return nullptr;
        return a[index];
    }

    bool contains(StrName name){
        return varnames_inv->contains(name);
    }

    void erase(StrName name){
        int index = varnames_inv->try_get(name);
        if(index == -1) FATAL_ERROR();
        a[index] = nullptr;
    }

    bool try_set(StrName name, PyObject* value){
        int index = varnames_inv->try_get(name);
        if(index == -1) return false;
        a[index] = value;
        return true;
    }

    NameDict_ to_namedict(){
        NameDict_ dict = make_sp<NameDict>();
        // TODO: optimize this
        // NameDict.items() is expensive
        for(auto& kv: varnames_inv->items()){
            dict->set(kv.first, a[kv.second]);
        }
        return dict;
    }
};

template<size_t MAX_SIZE>
struct ValueStackImpl {
    // We allocate extra MAX_SIZE/128 places to keep `_sp` valid when `is_overflow() == true`.
    PyObject* _begin[MAX_SIZE + MAX_SIZE/128];
    PyObject** _sp;

    ValueStackImpl(): _sp(_begin) {}

    PyObject*& top(){ return _sp[-1]; }
    PyObject* top() const { return _sp[-1]; }
    PyObject*& second(){ return _sp[-2]; }
    PyObject* second() const { return _sp[-2]; }
    PyObject*& third(){ return _sp[-3]; }
    PyObject* third() const { return _sp[-3]; }
    PyObject*& peek(int n){ return _sp[-n]; }
    PyObject* peek(int n) const { return _sp[-n]; }
    void push(PyObject* v){ *_sp++ = v; }
    void pop(){ --_sp; }
    PyObject* popx(){ return *--_sp; }
    ArgsView view(int n){ return ArgsView(_sp-n, _sp); }
    void shrink(int n){ _sp -= n; }
    int size() const { return _sp - _begin; }
    bool empty() const { return _sp == _begin; }
    PyObject** begin() { return _begin; }
    PyObject** end() { return _sp; }
    void reset(PyObject** sp) {
#if DEBUG_EXTRA_CHECK
        if(sp < _begin || sp > _begin + MAX_SIZE) FATAL_ERROR();
#endif
        _sp = sp;
    }
    void clear() { _sp = _begin; }
    bool is_overflow() const { return _sp >= _begin + MAX_SIZE; }
    
    ValueStackImpl(const ValueStackImpl&) = delete;
    ValueStackImpl(ValueStackImpl&&) = delete;
    ValueStackImpl& operator=(const ValueStackImpl&) = delete;
    ValueStackImpl& operator=(ValueStackImpl&&) = delete;
};

using ValueStack = ValueStackImpl<32768>;

struct Frame {
    int _ip = -1;
    int _next_ip = 0;
    ValueStack* _s;
    // This is for unwinding only, use `actual_sp_base()` for value stack access
    PyObject** _sp_base;

    const CodeObject* co;
    PyObject* _module;
    PyObject* _callable;
    FastLocals _locals;

    NameDict& f_globals() noexcept { return _module->attr(); }
    
    PyObject* f_closure_try_get(StrName name){
        if(_callable == nullptr) return nullptr;
        Function& fn = OBJ_GET(Function, _callable);
        if(fn._closure == nullptr) return nullptr;
        return fn._closure->try_get(name);
    }

    Frame(ValueStack* _s, PyObject** p0, const CodeObject* co, PyObject* _module, PyObject* _callable)
            : _s(_s), _sp_base(p0), co(co), _module(_module), _callable(_callable), _locals(co, p0) { }

    Frame(ValueStack* _s, PyObject** p0, const CodeObject* co, PyObject* _module, PyObject* _callable, FastLocals _locals)
            : _s(_s), _sp_base(p0), co(co), _module(_module), _callable(_callable), _locals(_locals) { }

    Frame(ValueStack* _s, PyObject** p0, const CodeObject_& co, PyObject* _module)
            : _s(_s), _sp_base(p0), co(co.get()), _module(_module), _callable(nullptr), _locals(co.get(), p0) {}

    Bytecode next_bytecode() {
        _ip = _next_ip++;
        return co->codes[_ip];
    }

    Str snapshot(){
        int line = co->lines[_ip];
        return co->src->snapshot(line);
    }

    PyObject** actual_sp_base() const { return _locals.a; }
    int stack_size() const { return _s->_sp - actual_sp_base(); }
    ArgsView stack_view() const { return ArgsView(actual_sp_base(), _s->_sp); }

    void jump_abs(int i){ _next_ip = i; }
    // void jump_rel(int i){ _next_ip += i; }

    bool jump_to_exception_handler(){
        // try to find a parent try block
        int block = co->codes[_ip].block;
        while(block >= 0){
            if(co->blocks[block].type == TRY_EXCEPT) break;
            block = co->blocks[block].parent;
        }
        if(block < 0) return false;
        PyObject* obj = _s->popx();         // pop exception object
        // get the stack size of the try block (depth of for loops)
        int _stack_size = co->blocks[block].for_loop_depth;
        if(stack_size() < _stack_size) throw std::runtime_error("invalid stack size");
        _s->reset(actual_sp_base() + _stack_size);          // rollback the stack   
        _s->push(obj);                                      // push exception object
        _next_ip = co->blocks[block].end;
        return true;
    }

    int _exit_block(int i){
        if(co->blocks[i].type == FOR_LOOP) _s->pop();
        return co->blocks[i].parent;
    }

    void jump_abs_break(int target){
        const Bytecode& prev = co->codes[_ip];
        int i = prev.block;
        _next_ip = target;
        if(_next_ip >= co->codes.size()){
            while(i>=0) i = _exit_block(i);
        }else{
            const Bytecode& next = co->codes[target];
            while(i>=0 && i!=next.block) i = _exit_block(i);
            if(i!=next.block) throw std::runtime_error("invalid jump");
        }
    }

    void _gc_mark() const {
        OBJ_MARK(_module);
        if(_callable != nullptr) OBJ_MARK(_callable);
        co->_gc_mark();
    }
};

}; // namespace pkpy


namespace pkpy {
struct ManagedHeap{
    std::vector<PyObject*> _no_gc;
    std::vector<PyObject*> gen;
    VM* vm;
    ManagedHeap(VM* vm): vm(vm) {}
    
    static const int kMinGCThreshold = 3072;
    int gc_threshold = kMinGCThreshold;
    int gc_counter = 0;

    /********************/
    int _gc_lock_counter = 0;
    struct ScopeLock{
        ManagedHeap* heap;
        ScopeLock(ManagedHeap* heap): heap(heap){
            heap->_gc_lock_counter++;
        }
        ~ScopeLock(){
            heap->_gc_lock_counter--;
        }
    };

    ScopeLock gc_scope_lock(){
        return ScopeLock(this);
    }
    /********************/

    template<typename T>
    PyObject* gcnew(Type type, T&& val){
        using __T = Py_<std::decay_t<T>>;
        PyObject* obj = new(pool64.alloc<__T>()) __T(type, std::forward<T>(val));
        gen.push_back(obj);
        gc_counter++;
        return obj;
    }

    template<typename T>
    PyObject* _new(Type type, T&& val){
        using __T = Py_<std::decay_t<T>>;
        PyObject* obj = new(pool64.alloc<__T>()) __T(type, std::forward<T>(val));
        obj->gc.enabled = false;
        _no_gc.push_back(obj);
        return obj;
    }

#if DEBUG_GC_STATS
    inline static std::map<Type, int> deleted;
#endif

    int sweep(){
        std::vector<PyObject*> alive;
        for(PyObject* obj: gen){
            if(obj->gc.marked){
                obj->gc.marked = false;
                alive.push_back(obj);
            }else{
#if DEBUG_GC_STATS
                deleted[obj->type] += 1;
#endif
                obj->~PyObject(), pool64.dealloc(obj);
            }
        }

        // clear _no_gc marked flag
        for(PyObject* obj: _no_gc) obj->gc.marked = false;

        int freed = gen.size() - alive.size();
        // std::cout << "GC: " << alive.size() << "/" << gen.size() << " (" << freed << " freed)" << std::endl;
        gen.clear();
        gen.swap(alive);
        return freed;
    }

    void _auto_collect(){
#if !DEBUG_NO_AUTO_GC
        if(_gc_lock_counter > 0) return;
        if(gc_counter < gc_threshold) return;
        gc_counter = 0;
        collect();
        gc_threshold = gen.size() * 2;
        if(gc_threshold < kMinGCThreshold) gc_threshold = kMinGCThreshold;
#endif
    }

    int collect(){
        if(_gc_lock_counter > 0) FATAL_ERROR();
        mark();
        int freed = sweep();
        return freed;
    }

    void mark();

    ~ManagedHeap(){
        for(PyObject* obj: _no_gc) obj->~PyObject(), pool64.dealloc(obj);
        for(PyObject* obj: gen) obj->~PyObject(), pool64.dealloc(obj);
#if DEBUG_GC_STATS
        for(auto& [type, count]: deleted){
            std::cout << "GC: " << obj_type_name(vm, type) << "=" << count << std::endl;
        }
#endif
    }
};

inline void FuncDecl::_gc_mark() const{
    code->_gc_mark();
    for(int i=0; i<kwargs.size(); i++) OBJ_MARK(kwargs[i].value);
}

}   // namespace pkpy


namespace pkpy{

/* Stack manipulation macros */
// https://github.com/python/cpython/blob/3.9/Python/ceval.c#L1123
#define TOP()             (s_data.top())
#define SECOND()          (s_data.second())
#define THIRD()           (s_data.third())
#define PEEK(n)           (s_data.peek(n))
#define STACK_SHRINK(n)   (s_data.shrink(n))
#define PUSH(v)           (s_data.push(v))
#define POP()             (s_data.pop())
#define POPX()            (s_data.popx())
#define STACK_VIEW(n)     (s_data.view(n))

typedef Bytes (*ReadFileCwdFunc)(const Str& name);
inline ReadFileCwdFunc _read_file_cwd = [](const Str& name) { return Bytes(); };
inline int set_read_file_cwd(ReadFileCwdFunc func) { _read_file_cwd = func; return 0; }

#define DEF_NATIVE_2(ctype, ptype)                                      \
    template<> inline ctype py_cast<ctype>(VM* vm, PyObject* obj) {     \
        vm->check_type(obj, vm->ptype);                                 \
        return OBJ_GET(ctype, obj);                                     \
    }                                                                   \
    template<> inline ctype _py_cast<ctype>(VM* vm, PyObject* obj) {    \
        return OBJ_GET(ctype, obj);                                     \
    }                                                                   \
    template<> inline ctype& py_cast<ctype&>(VM* vm, PyObject* obj) {   \
        vm->check_type(obj, vm->ptype);                                 \
        return OBJ_GET(ctype, obj);                                     \
    }                                                                   \
    template<> inline ctype& _py_cast<ctype&>(VM* vm, PyObject* obj) {  \
        return OBJ_GET(ctype, obj);                                     \
    }                                                                   \
    inline PyObject* py_var(VM* vm, const ctype& value) { return vm->heap.gcnew(vm->ptype, value);}     \
    inline PyObject* py_var(VM* vm, ctype&& value) { return vm->heap.gcnew(vm->ptype, std::move(value));}


class Generator final: public BaseIter {
    Frame frame;
    int state;      // 0,1,2
    List s_backup;
public:
    Generator(VM* vm, Frame&& frame, ArgsView buffer): BaseIter(vm), frame(std::move(frame)), state(0) {
        for(PyObject* obj: buffer) s_backup.push_back(obj);
    }

    PyObject* next() override;
    void _gc_mark() const;
};

struct PyTypeInfo{
    PyObject* obj;
    Type base;
    Str name;
};

struct FrameId{
    std::vector<pkpy::Frame>* data;
    int index;
    FrameId(std::vector<pkpy::Frame>* data, int index) : data(data), index(index) {}
    Frame* operator->() const { return &data->operator[](index); }
};

class VM {
    VM* vm;     // self reference for simplify code
public:
    ManagedHeap heap;
    ValueStack s_data;
    stack< Frame > callstack;
    std::vector<PyTypeInfo> _all_types;
    void (*_gc_marker_ex)(VM*) = nullptr;

    NameDict _modules;                                  // loaded modules
    std::map<StrName, Str> _lazy_modules;               // lazy loaded modules

    PyObject* None;
    PyObject* True;
    PyObject* False;
    PyObject* Ellipsis;
    PyObject* builtins;         // builtins module
    PyObject* StopIteration;
    PyObject* _main;            // __main__ module

    std::stringstream _stdout_buffer;
    std::stringstream _stderr_buffer;
    std::ostream* _stdout;
    std::ostream* _stderr;

    bool _initialized;

    // for quick access
    Type tp_object, tp_type, tp_int, tp_float, tp_bool, tp_str;
    Type tp_list, tp_tuple;
    Type tp_function, tp_native_func, tp_iterator, tp_bound_method;
    Type tp_slice, tp_range, tp_module;
    Type tp_super, tp_exception, tp_bytes, tp_mappingproxy;

    const bool enable_os;

    VM(bool use_stdio=true, bool enable_os=true) : heap(this), enable_os(enable_os) {
        this->vm = this;
        this->_stdout = use_stdio ? &std::cout : &_stdout_buffer;
        this->_stderr = use_stdio ? &std::cerr : &_stderr_buffer;
        callstack.reserve(8);
        _initialized = false;
        init_builtin_types();
        _initialized = true;
    }

    bool is_stdio_used() const { return _stdout == &std::cout; }

    std::string read_output(){
        if(is_stdio_used()) UNREACHABLE();
        std::stringstream* s_out = (std::stringstream*)(vm->_stdout);
        std::stringstream* s_err = (std::stringstream*)(vm->_stderr);
        pkpy::Str _stdout = s_out->str();
        pkpy::Str _stderr = s_err->str();
        std::stringstream ss;
        ss << '{' << "\"stdout\": " << _stdout.escape(false);
        ss << ", " << "\"stderr\": " << _stderr.escape(false) << '}';
        s_out->str(""); s_err->str("");
        return ss.str();
    }

    FrameId top_frame() {
#if DEBUG_EXTRA_CHECK
        if(callstack.empty()) FATAL_ERROR();
#endif
        return FrameId(&callstack.data(), callstack.size()-1);
    }

    PyObject* asStr(PyObject* obj){
        PyObject* self;
        PyObject* f = get_unbound_method(obj, __str__, &self, false);
        if(self != PY_NULL) return call_method(self, f);
        return asRepr(obj);
    }

    PyObject* asIter(PyObject* obj){
        if(is_type(obj, tp_iterator)) return obj;
        PyObject* self;
        PyObject* iter_f = get_unbound_method(obj, __iter__, &self, false);
        if(self != PY_NULL) return call_method(self, iter_f);
        TypeError(OBJ_NAME(_t(obj)).escape() + " object is not iterable");
        return nullptr;
    }

    PyObject* asList(PyObject* it){
        if(is_non_tagged_type(it, tp_list)) return it;
        return call(_t(tp_list), it);
    }

    PyObject* find_name_in_mro(PyObject* cls, StrName name){
        PyObject* val;
        do{
            val = cls->attr().try_get(name);
            if(val != nullptr) return val;
            Type cls_t = OBJ_GET(Type, cls);
            Type base = _all_types[cls_t].base;
            if(base.index == -1) break;
            cls = _all_types[base].obj;
        }while(true);
        return nullptr;
    }

    bool isinstance(PyObject* obj, Type cls_t){
        Type obj_t = OBJ_GET(Type, _t(obj));
        do{
            if(obj_t == cls_t) return true;
            Type base = _all_types[obj_t].base;
            if(base.index == -1) break;
            obj_t = base;
        }while(true);
        return false;
    }

    PyObject* exec(Str source, Str filename, CompileMode mode, PyObject* _module=nullptr){
        if(_module == nullptr) _module = _main;
        try {
            CodeObject_ code = compile(source, filename, mode);
#if DEBUG_DIS_EXEC
            if(_module == _main) std::cout << disassemble(code) << '\n';
#endif
            return _exec(code, _module);
        }catch (const Exception& e){
            *_stderr << e.summary() << '\n';

        }
#if !DEBUG_FULL_EXCEPTION
        catch (const std::exception& e) {
            *_stderr << "An std::exception occurred! It could be a bug.\n";
            *_stderr << e.what() << '\n';
        }
#endif
        callstack.clear();
        s_data.clear();
        return nullptr;
    }

    template<typename ...Args>
    PyObject* _exec(Args&&... args){
        callstack.emplace(&s_data, s_data._sp, std::forward<Args>(args)...);
        return _run_top_frame();
    }

    void _pop_frame(){
        Frame* frame = &callstack.top();
        s_data.reset(frame->_sp_base);
        callstack.pop();
    }

    void _push_varargs(){ }
    void _push_varargs(PyObject* _0){ PUSH(_0); }
    void _push_varargs(PyObject* _0, PyObject* _1){ PUSH(_0); PUSH(_1); }
    void _push_varargs(PyObject* _0, PyObject* _1, PyObject* _2){ PUSH(_0); PUSH(_1); PUSH(_2); }
    void _push_varargs(PyObject* _0, PyObject* _1, PyObject* _2, PyObject* _3){ PUSH(_0); PUSH(_1); PUSH(_2); PUSH(_3); }

    template<typename... Args>
    PyObject* call(PyObject* callable, Args&&... args){
        PUSH(callable);
        PUSH(PY_NULL);
        _push_varargs(args...);
        return vectorcall(sizeof...(args));
    }

    template<typename... Args>
    PyObject* call_method(PyObject* self, PyObject* callable, Args&&... args){
        PUSH(callable);
        PUSH(self);
        _push_varargs(args...);
        return vectorcall(sizeof...(args));
    }

    template<typename... Args>
    PyObject* call_method(PyObject* self, StrName name, Args&&... args){
        PyObject* callable = get_unbound_method(self, name, &self);
        return call_method(self, callable, args...);
    }

    PyObject* property(NativeFuncC fget, NativeFuncC fset=nullptr){
        PyObject* p = builtins->attr("property");
        PyObject* _0 = heap.gcnew(tp_native_func, NativeFunc(fget, 1, false));
        PyObject* _1 = vm->None;
        if(fset != nullptr) _1 = heap.gcnew(tp_native_func, NativeFunc(fset, 2, false));
        return call(p, _0, _1);
    }

    PyObject* new_type_object(PyObject* mod, StrName name, Type base){
        PyObject* obj = heap._new<Type>(tp_type, _all_types.size());
        PyTypeInfo info{
            obj,
            base,
            (mod!=nullptr && mod!=builtins) ? Str(OBJ_NAME(mod)+"."+name.sv()): name.sv()
        };
        if(mod != nullptr) mod->attr().set(name, obj);
        _all_types.push_back(info);
        return obj;
    }

    Type _new_type_object(StrName name, Type base=0) {
        PyObject* obj = new_type_object(nullptr, name, base);
        return OBJ_GET(Type, obj);
    }

    PyObject* _find_type(const Str& type){
        PyObject* obj = builtins->attr().try_get(type);
        if(obj == nullptr){
            for(auto& t: _all_types) if(t.name == type) return t.obj;
            throw std::runtime_error(fmt("type not found: ", type));
        }
        return obj;
    }

    template<int ARGC>
    void bind_func(Str type, Str name, NativeFuncC fn) {
        bind_func<ARGC>(_find_type(type), name, fn);
    }

    template<int ARGC>
    void bind_method(Str type, Str name, NativeFuncC fn) {
        bind_method<ARGC>(_find_type(type), name, fn);
    }

    template<int ARGC, typename... Args>
    void bind_static_method(Args&&... args) {
        bind_func<ARGC>(std::forward<Args>(args)...);
    }

    template<int ARGC>
    void _bind_methods(std::vector<Str> types, Str name, NativeFuncC fn) {
        for(auto& type: types) bind_method<ARGC>(type, name, fn);
    }

    template<int ARGC>
    void bind_builtin_func(Str name, NativeFuncC fn) {
        bind_func<ARGC>(builtins, name, fn);
    }

    int normalized_index(int index, int size){
        if(index < 0) index += size;
        if(index < 0 || index >= size){
            IndexError(std::to_string(index) + " not in [0, " + std::to_string(size) + ")");
        }
        return index;
    }

    template<typename P>
    PyObject* PyIter(P&& value) {
        static_assert(std::is_base_of_v<BaseIter, std::decay_t<P>>);
        return heap.gcnew<P>(tp_iterator, std::forward<P>(value));
    }

    PyObject* PyIterNext(PyObject* obj){
        if(is_non_tagged_type(obj, tp_iterator)){
            BaseIter* iter = static_cast<BaseIter*>(obj->value());
            return iter->next();
        }
        return call_method(obj, __next__);
    }
    
    /***** Error Reporter *****/
    void _error(StrName name, const Str& msg){
        _error(Exception(name, msg));
    }

    void _raise(){
        bool ok = top_frame()->jump_to_exception_handler();
        if(ok) throw HandledException();
        else throw UnhandledException();
    }

    void StackOverflowError() { _error("StackOverflowError", ""); }
    void IOError(const Str& msg) { _error("IOError", msg); }
    void NotImplementedError(){ _error("NotImplementedError", ""); }
    void TypeError(const Str& msg){ _error("TypeError", msg); }
    void ZeroDivisionError(){ _error("ZeroDivisionError", "division by zero"); }
    void IndexError(const Str& msg){ _error("IndexError", msg); }
    void ValueError(const Str& msg){ _error("ValueError", msg); }
    void NameError(StrName name){ _error("NameError", fmt("name ", name.escape() + " is not defined")); }

    void AttributeError(PyObject* obj, StrName name){
        // OBJ_NAME calls getattr, which may lead to a infinite recursion
        _error("AttributeError", fmt("type ", OBJ_NAME(_t(obj)).escape(), " has no attribute ", name.escape()));
    }

    void AttributeError(Str msg){ _error("AttributeError", msg); }

    void check_type(PyObject* obj, Type type){
        if(is_type(obj, type)) return;
        TypeError("expected " + OBJ_NAME(_t(type)).escape() + ", but got " + OBJ_NAME(_t(obj)).escape());
    }

    PyObject* _t(Type t){
        return _all_types[t.index].obj;
    }

    PyObject* _t(PyObject* obj){
        if(is_int(obj)) return _t(tp_int);
        if(is_float(obj)) return _t(tp_float);
        return _all_types[OBJ_GET(Type, _t(obj->type)).index].obj;
    }

    ~VM() {
        callstack.clear();
        s_data.clear();
        _all_types.clear();
        _modules.clear();
        _lazy_modules.clear();
    }

    void _log_s_data(const char* title = nullptr);
    PyObject* vectorcall(int ARGC, int KWARGC=0, bool op_call=false);
    CodeObject_ compile(Str source, Str filename, CompileMode mode, bool unknown_global_scope=false);
    PyObject* num_negated(PyObject* obj);
    f64 num_to_float(PyObject* obj);
    bool asBool(PyObject* obj);
    i64 hash(PyObject* obj);
    PyObject* asRepr(PyObject* obj);
    PyObject* new_module(StrName name);
    Str disassemble(CodeObject_ co);
    void init_builtin_types();
    PyObject* _py_call(PyObject** sp_base, PyObject* callable, ArgsView args, ArgsView kwargs);
    PyObject* getattr(PyObject* obj, StrName name, bool throw_err=true);
    PyObject* get_unbound_method(PyObject* obj, StrName name, PyObject** self, bool throw_err=true, bool fallback=false);
    void parse_int_slice(const Slice& s, int length, int& start, int& stop, int& step);
    PyObject* format(Str, PyObject*);
    void setattr(PyObject* obj, StrName name, PyObject* value);
    template<int ARGC>
    void bind_method(PyObject*, Str, NativeFuncC);
    template<int ARGC>
    void bind_func(PyObject*, Str, NativeFuncC);
    void _error(Exception);
    PyObject* _run_top_frame();
    void post_init();
};

inline PyObject* NativeFunc::operator()(VM* vm, ArgsView args) const{
    int args_size = args.size() - (int)method;  // remove self
    if(argc != -1 && args_size != argc) {
        vm->TypeError(fmt("expected ", argc, " arguments, but got ", args_size));
    }
    return f(vm, args);
}

inline void CodeObject::optimize(VM* vm){
    // uint32_t base_n = (uint32_t)(names.size() / kLocalsLoadFactor + 0.5);
    // perfect_locals_capacity = std::max(find_next_capacity(base_n), NameDict::__Capacity);
    // perfect_hash_seed = find_perfect_hash_seed(perfect_locals_capacity, names);
}

DEF_NATIVE_2(Str, tp_str)
DEF_NATIVE_2(List, tp_list)
DEF_NATIVE_2(Tuple, tp_tuple)
DEF_NATIVE_2(Function, tp_function)
DEF_NATIVE_2(NativeFunc, tp_native_func)
DEF_NATIVE_2(BoundMethod, tp_bound_method)
DEF_NATIVE_2(Range, tp_range)
DEF_NATIVE_2(Slice, tp_slice)
DEF_NATIVE_2(Exception, tp_exception)
DEF_NATIVE_2(Bytes, tp_bytes)
DEF_NATIVE_2(MappingProxy, tp_mappingproxy)

#define PY_CAST_INT(T)                                  \
template<> inline T py_cast<T>(VM* vm, PyObject* obj){  \
    vm->check_type(obj, vm->tp_int);                    \
    return (T)(BITS(obj) >> 2);                         \
}                                                       \
template<> inline T _py_cast<T>(VM* vm, PyObject* obj){ \
    return (T)(BITS(obj) >> 2);                         \
}

PY_CAST_INT(char)
PY_CAST_INT(short)
PY_CAST_INT(int)
PY_CAST_INT(long)
PY_CAST_INT(long long)
PY_CAST_INT(unsigned char)
PY_CAST_INT(unsigned short)
PY_CAST_INT(unsigned int)
PY_CAST_INT(unsigned long)
PY_CAST_INT(unsigned long long)


template<> inline float py_cast<float>(VM* vm, PyObject* obj){
    vm->check_type(obj, vm->tp_float);
    i64 bits = BITS(obj);
    bits = (bits >> 2) << 2;
    return BitsCvt(bits)._float;
}
template<> inline float _py_cast<float>(VM* vm, PyObject* obj){
    i64 bits = BITS(obj);
    bits = (bits >> 2) << 2;
    return BitsCvt(bits)._float;
}
template<> inline double py_cast<double>(VM* vm, PyObject* obj){
    vm->check_type(obj, vm->tp_float);
    i64 bits = BITS(obj);
    bits = (bits >> 2) << 2;
    return BitsCvt(bits)._float;
}
template<> inline double _py_cast<double>(VM* vm, PyObject* obj){
    i64 bits = BITS(obj);
    bits = (bits >> 2) << 2;
    return BitsCvt(bits)._float;
}


#define PY_VAR_INT(T)                                       \
    inline PyObject* py_var(VM* vm, T _val){                \
        i64 val = static_cast<i64>(_val);                   \
        if(((val << 2) >> 2) != val){                       \
            vm->_error("OverflowError", std::to_string(val) + " is out of range");  \
        }                                                                           \
        val = (val << 2) | 0b01;                                                    \
        return reinterpret_cast<PyObject*>(val);                                    \
    }

PY_VAR_INT(char)
PY_VAR_INT(short)
PY_VAR_INT(int)
PY_VAR_INT(long)
PY_VAR_INT(long long)
PY_VAR_INT(unsigned char)
PY_VAR_INT(unsigned short)
PY_VAR_INT(unsigned int)
PY_VAR_INT(unsigned long)
PY_VAR_INT(unsigned long long)

#define PY_VAR_FLOAT(T)                             \
    inline PyObject* py_var(VM* vm, T _val){        \
        f64 val = static_cast<f64>(_val);           \
        i64 bits = BitsCvt(val)._int;               \
        bits = (bits >> 2) << 2;                    \
        bits |= 0b10;                               \
        return reinterpret_cast<PyObject*>(bits);   \
    }

PY_VAR_FLOAT(float)
PY_VAR_FLOAT(double)

inline PyObject* py_var(VM* vm, bool val){
    return val ? vm->True : vm->False;
}

template<> inline bool py_cast<bool>(VM* vm, PyObject* obj){
    vm->check_type(obj, vm->tp_bool);
    return obj == vm->True;
}
template<> inline bool _py_cast<bool>(VM* vm, PyObject* obj){
    return obj == vm->True;
}

inline PyObject* py_var(VM* vm, const char val[]){
    return VAR(Str(val));
}

inline PyObject* py_var(VM* vm, std::string val){
    return VAR(Str(std::move(val)));
}

inline PyObject* py_var(VM* vm, std::string_view val){
    return VAR(Str(val));
}

template<typename T>
void _check_py_class(VM* vm, PyObject* obj){
    vm->check_type(obj, T::_type(vm));
}

inline PyObject* VM::num_negated(PyObject* obj){
    if (is_int(obj)){
        return VAR(-CAST(i64, obj));
    }else if(is_float(obj)){
        return VAR(-CAST(f64, obj));
    }
    TypeError("expected 'int' or 'float', got " + OBJ_NAME(_t(obj)).escape());
    return nullptr;
}

inline f64 VM::num_to_float(PyObject* obj){
    if(is_float(obj)){
        return CAST(f64, obj);
    } else if (is_int(obj)){
        return (f64)CAST(i64, obj);
    }
    TypeError("expected 'int' or 'float', got " + OBJ_NAME(_t(obj)).escape());
    return 0;
}

inline bool VM::asBool(PyObject* obj){
    if(is_non_tagged_type(obj, tp_bool)) return obj == True;
    if(obj == None) return false;
    if(is_int(obj)) return CAST(i64, obj) != 0;
    if(is_float(obj)) return CAST(f64, obj) != 0.0;
    PyObject* self;
    PyObject* len_f = get_unbound_method(obj, __len__, &self, false);
    if(self != PY_NULL){
        PyObject* ret = call_method(self, len_f);
        return CAST(i64, ret) > 0;
    }
    return true;
}

inline void VM::parse_int_slice(const Slice& s, int length, int& start, int& stop, int& step){
    auto clip = [](int value, int min, int max){
        if(value < min) return min;
        if(value > max) return max;
        return value;
    };
    if(s.step == None) step = 1;
    else step = CAST(int, s.step);
    if(step == 0) ValueError("slice step cannot be zero");
    if(step > 0){
        if(s.start == None){
            start = 0;
        }else{
            start = CAST(int, s.start);
            if(start < 0) start += length;
            start = clip(start, 0, length);
        }
        if(s.stop == None){
            stop = length;
        }else{
            stop = CAST(int, s.stop);
            if(stop < 0) stop += length;
            stop = clip(stop, 0, length);
        }
    }else{
        if(s.start == None){
            start = length - 1;
        }else{
            start = CAST(int, s.start);
            if(start < 0) start += length;
            start = clip(start, -1, length - 1);
        }
        if(s.stop == None){
            stop = -1;
        }else{
            stop = CAST(int, s.stop);
            if(stop < 0) stop += length;
            stop = clip(stop, -1, length - 1);
        }
    }
}

inline i64 VM::hash(PyObject* obj){
    if (is_non_tagged_type(obj, tp_str)) return CAST(Str&, obj).hash();
    if (is_int(obj)) return CAST(i64, obj);
    if (is_non_tagged_type(obj, tp_tuple)) {
        i64 x = 1000003;
        const Tuple& items = CAST(Tuple&, obj);
        for (int i=0; i<items.size(); i++) {
            i64 y = hash(items[i]);
            // recommended by Github Copilot
            x = x ^ (y + 0x9e3779b9 + (x << 6) + (x >> 2));
        }
        return x;
    }
    if (is_non_tagged_type(obj, tp_type)) return BITS(obj);
    if (is_non_tagged_type(obj, tp_iterator)) return BITS(obj);
    if (is_non_tagged_type(obj, tp_bool)) return _CAST(bool, obj) ? 1 : 0;
    if (is_float(obj)){
        f64 val = CAST(f64, obj);
        return (i64)std::hash<f64>()(val);
    }
    TypeError("unhashable type: " +  OBJ_NAME(_t(obj)).escape());
    return 0;
}

inline PyObject* VM::asRepr(PyObject* obj){
    return call_method(obj, __repr__);
}

inline PyObject* VM::format(Str spec, PyObject* obj){
    if(spec.empty()) return asStr(obj);
    char type;
    switch(spec.end()[-1]){
        case 'f': case 'd': case 's':
            type = spec.end()[-1];
            spec = spec.substr(0, spec.length() - 1);
            break;
        default: type = ' '; break;
    }

    char pad_c = ' ';
    if(spec[0] == '0'){
        pad_c = '0';
        spec = spec.substr(1);
    }
    char align;
    if(spec[0] == '>'){
        align = '>';
        spec = spec.substr(1);
    }else if(spec[0] == '<'){
        align = '<';
        spec = spec.substr(1);
    }else{
        if(is_int(obj) || is_float(obj)) align = '>';
        else align = '<';
    }

    int dot = spec.index(".");
    int width, precision;
    try{
        if(dot >= 0){
            width = Number::stoi(spec.substr(0, dot).str());
            precision = Number::stoi(spec.substr(dot+1).str());
        }else{
            width = Number::stoi(spec.str());
            precision = -1;
        }
    }catch(...){
        ValueError("invalid format specifer");
    }

    if(type != 'f' && dot >= 0) ValueError("precision not allowed in the format specifier");
    Str ret;
    if(type == 'f'){
        f64 val = num_to_float(obj);
        if(precision < 0) precision = 6;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(precision) << val;
        ret = ss.str();
    }else if(type == 'd'){
        ret = std::to_string(CAST(i64, obj));
    }else if(type == 's'){
        ret = CAST(Str&, obj);
    }else{
        ret = CAST(Str&, asStr(obj));
    }
    if(width > ret.length()){
        int pad = width - ret.length();
        std::string padding(pad, pad_c);
        if(align == '>') ret = padding.c_str() + ret;
        else ret = ret + padding.c_str();
    }
    return VAR(ret);
}

inline PyObject* VM::new_module(StrName name) {
    PyObject* obj = heap._new<DummyModule>(tp_module, DummyModule());
    obj->attr().set(__name__, VAR(name.sv()));
    // we do not allow override in order to avoid memory leak
    // it is because Module objects are not garbage collected
    if(_modules.contains(name)) FATAL_ERROR();
    _modules.set(name, obj);
    return obj;
}

inline std::string _opcode_argstr(VM* vm, Bytecode byte, const CodeObject* co){
    std::string argStr = byte.arg == -1 ? "" : std::to_string(byte.arg);
    switch(byte.op){
        case OP_LOAD_CONST:
            if(vm != nullptr){
                argStr += fmt(" (", CAST(Str, vm->asRepr(co->consts[byte.arg])), ")");
            }
            break;
        case OP_LOAD_NAME: case OP_LOAD_GLOBAL: case OP_LOAD_NONLOCAL: case OP_STORE_GLOBAL:
        case OP_LOAD_ATTR: case OP_LOAD_METHOD: case OP_STORE_ATTR: case OP_DELETE_ATTR:
        case OP_IMPORT_NAME: case OP_BEGIN_CLASS:
        case OP_DELETE_GLOBAL:
            argStr += fmt(" (", StrName(byte.arg).sv(), ")");
            break;
        case OP_LOAD_FAST: case OP_STORE_FAST: case OP_DELETE_FAST:
            argStr += fmt(" (", co->varnames[byte.arg].sv(), ")");
            break;
        case OP_BINARY_OP:
            argStr += fmt(" (", BINARY_SPECIAL_METHODS[byte.arg], ")");
            break;
        case OP_LOAD_FUNCTION:
            argStr += fmt(" (", co->func_decls[byte.arg]->code->name, ")");
            break;
    }
    return argStr;
}

inline Str VM::disassemble(CodeObject_ co){
    auto pad = [](const Str& s, const int n){
        if(s.length() >= n) return s.substr(0, n);
        return s + std::string(n - s.length(), ' ');
    };

    std::vector<int> jumpTargets;
    for(auto byte : co->codes){
        if(byte.op == OP_JUMP_ABSOLUTE || byte.op == OP_POP_JUMP_IF_FALSE){
            jumpTargets.push_back(byte.arg);
        }
    }
    std::stringstream ss;
    int prev_line = -1;
    for(int i=0; i<co->codes.size(); i++){
        const Bytecode& byte = co->codes[i];
        Str line = std::to_string(co->lines[i]);
        if(co->lines[i] == prev_line) line = "";
        else{
            if(prev_line != -1) ss << "\n";
            prev_line = co->lines[i];
        }

        std::string pointer;
        if(std::find(jumpTargets.begin(), jumpTargets.end(), i) != jumpTargets.end()){
            pointer = "-> ";
        }else{
            pointer = "   ";
        }
        ss << pad(line, 8) << pointer << pad(std::to_string(i), 3);
        ss << " " << pad(OP_NAMES[byte.op], 20) << " ";
        // ss << pad(byte.arg == -1 ? "" : std::to_string(byte.arg), 5);
        std::string argStr = _opcode_argstr(this, byte, co.get());
        ss << pad(argStr, 40);      // may overflow
        ss << co->blocks[byte.block].type;
        if(i != co->codes.size() - 1) ss << '\n';
    }

    for(auto& decl: co->func_decls){
        ss << "\n\n" << "Disassembly of " << decl->code->name << ":\n";
        ss << disassemble(decl->code);
    }
    ss << "\n";
    return Str(ss.str());
}

inline void VM::_log_s_data(const char* title) {
    if(!_initialized) return;
    if(callstack.empty()) return;
    std::stringstream ss;
    if(title) ss << title << " | ";
    std::map<PyObject**, int> sp_bases;
    for(Frame& f: callstack.data()){
        if(f._sp_base == nullptr) FATAL_ERROR();
        sp_bases[f._sp_base] += 1;
    }
    FrameId frame = top_frame();
    int line = frame->co->lines[frame->_ip];
    ss << frame->co->name << ":" << line << " [";
    for(PyObject** p=s_data.begin(); p!=s_data.end(); p++){
        ss << std::string(sp_bases[p], '|');
        if(sp_bases[p] > 0) ss << " ";
        PyObject* obj = *p;
        if(obj == nullptr) ss << "(nil)";
        else if(obj == PY_BEGIN_CALL) ss << "BEGIN_CALL";
        else if(obj == PY_NULL) ss << "NULL";
        else if(is_int(obj)) ss << CAST(i64, obj);
        else if(is_float(obj)) ss << CAST(f64, obj);
        else if(is_type(obj, tp_str)) ss << CAST(Str, obj).escape();
        else if(obj == None) ss << "None";
        else if(obj == True) ss << "True";
        else if(obj == False) ss << "False";
        else if(is_type(obj, tp_function)){
            auto& f = CAST(Function&, obj);
            ss << f.decl->code->name << "(...)";
        } else if(is_type(obj, tp_type)){
            Type t = OBJ_GET(Type, obj);
            ss << "<class " + _all_types[t].name.escape() + ">";
        } else if(is_type(obj, tp_list)){
            auto& t = CAST(List&, obj);
            ss << "list(size=" << t.size() << ")";
        } else if(is_type(obj, tp_tuple)){
            auto& t = CAST(Tuple&, obj);
            ss << "tuple(size=" << t.size() << ")";
        } else ss << "(" << obj_type_name(this, obj->type) << ")";
        ss << ", ";
    }
    std::string output = ss.str();
    if(!s_data.empty()) {
        output.pop_back(); output.pop_back();
    }
    output.push_back(']');
    Bytecode byte = frame->co->codes[frame->_ip];
    std::cout << output << " " << OP_NAMES[byte.op] << " " << _opcode_argstr(nullptr, byte, frame->co) << std::endl;
}

inline void VM::init_builtin_types(){
    _all_types.push_back({heap._new<Type>(Type(1), Type(0)), -1, "object"});
    _all_types.push_back({heap._new<Type>(Type(1), Type(1)), 0, "type"});
    tp_object = 0; tp_type = 1;

    tp_int = _new_type_object("int");
    tp_float = _new_type_object("float");
    if(tp_int.index != kTpIntIndex || tp_float.index != kTpFloatIndex) FATAL_ERROR();

    tp_bool = _new_type_object("bool");
    tp_str = _new_type_object("str");
    tp_list = _new_type_object("list");
    tp_tuple = _new_type_object("tuple");
    tp_slice = _new_type_object("slice");
    tp_range = _new_type_object("range");
    tp_module = _new_type_object("module");
    tp_function = _new_type_object("function");
    tp_native_func = _new_type_object("native_func");
    tp_iterator = _new_type_object("iterator");
    tp_bound_method = _new_type_object("bound_method");
    tp_super = _new_type_object("super");
    tp_exception = _new_type_object("Exception");
    tp_bytes = _new_type_object("bytes");
    tp_mappingproxy = _new_type_object("mappingproxy");

    this->None = heap._new<Dummy>(_new_type_object("NoneType"), {});
    this->Ellipsis = heap._new<Dummy>(_new_type_object("ellipsis"), {});
    this->True = heap._new<Dummy>(tp_bool, {});
    this->False = heap._new<Dummy>(tp_bool, {});
    this->StopIteration = heap._new<Dummy>(_new_type_object("StopIterationType"), {});

    this->builtins = new_module("builtins");
    this->_main = new_module("__main__");
    
    // setup public types
    builtins->attr().set("type", _t(tp_type));
    builtins->attr().set("object", _t(tp_object));
    builtins->attr().set("bool", _t(tp_bool));
    builtins->attr().set("int", _t(tp_int));
    builtins->attr().set("float", _t(tp_float));
    builtins->attr().set("str", _t(tp_str));
    builtins->attr().set("list", _t(tp_list));
    builtins->attr().set("tuple", _t(tp_tuple));
    builtins->attr().set("range", _t(tp_range));
    builtins->attr().set("bytes", _t(tp_bytes));
    builtins->attr().set("StopIteration", StopIteration);
    builtins->attr().set("slice", _t(tp_slice));

    post_init();
    for(int i=0; i<_all_types.size(); i++){
        _all_types[i].obj->attr()._try_perfect_rehash();
    }
    for(auto [k, v]: _modules.items()) v->attr()._try_perfect_rehash();
}

inline PyObject* VM::vectorcall(int ARGC, int KWARGC, bool op_call){
    bool is_varargs = ARGC == 0xFFFF;
    PyObject** p0;
    PyObject** p1 = s_data._sp - KWARGC*2;
    if(is_varargs){
        p0 = p1 - 1;
        while(*p0 != PY_BEGIN_CALL) p0--;
        // [BEGIN_CALL, callable, <self>, args..., kwargs...]
        //      ^p0                                ^p1      ^_sp
        ARGC = p1 - (p0 + 3);
    }else{
        p0 = p1 - ARGC - 2 - (int)is_varargs;
        // [callable, <self>, args..., kwargs...]
        //      ^p0                    ^p1      ^_sp
    }
    PyObject* callable = p1[-(ARGC + 2)];
    bool method_call = p1[-(ARGC + 1)] != PY_NULL;


    // handle boundmethod, do a patch
    if(is_non_tagged_type(callable, tp_bound_method)){
        if(method_call) FATAL_ERROR();
        auto& bm = CAST(BoundMethod&, callable);
        callable = bm.func;      // get unbound method
        p1[-(ARGC + 2)] = bm.func;
        p1[-(ARGC + 1)] = bm.self;
        method_call = true;
        // [unbound, self, args..., kwargs...]
    }

    ArgsView args(p1 - ARGC - int(method_call), p1);

    if(is_non_tagged_type(callable, tp_native_func)){
        const auto& f = OBJ_GET(NativeFunc, callable);
        if(KWARGC != 0) TypeError("native_func does not accept keyword arguments");
        PyObject* ret = f(this, args);
        s_data.reset(p0);
        return ret;
    }

    ArgsView kwargs(p1, s_data._sp);

    if(is_non_tagged_type(callable, tp_function)){
        // ret is nullptr or a generator
        PyObject* ret = _py_call(p0, callable, args, kwargs);
        // stack resetting is handled by _py_call
        if(ret != nullptr) return ret;
        if(op_call) return PY_OP_CALL;
        return _run_top_frame();
    }

    if(is_non_tagged_type(callable, tp_type)){
        if(method_call) FATAL_ERROR();
        // [type, NULL, args..., kwargs...]

        // TODO: derived __new__ ?
        PyObject* new_f = callable->attr().try_get(__new__);
        PyObject* obj;
        if(new_f != nullptr){
            PUSH(new_f);
            PUSH(PY_NULL);
            for(PyObject* obj: args) PUSH(obj);
            for(PyObject* obj: kwargs) PUSH(obj);
            obj = vectorcall(ARGC, KWARGC);
            if(!isinstance(obj, OBJ_GET(Type, callable))) return obj;
        }else{
            obj = heap.gcnew<DummyInstance>(OBJ_GET(Type, callable), {});
        }
        PyObject* self;
        callable = get_unbound_method(obj, __init__, &self, false);
        if (self != PY_NULL) {
            // replace `NULL` with `self`
            p1[-(ARGC + 2)] = callable;
            p1[-(ARGC + 1)] = self;
            // [init_f, self, args..., kwargs...]
            vectorcall(ARGC, KWARGC);
            // We just discard the return value of `__init__`
            // in cpython it raises a TypeError if the return value is not None
        }else{
            // manually reset the stack
            s_data.reset(p0);
        }
        return obj;
    }

    // handle `__call__` overload
    PyObject* self;
    PyObject* call_f = get_unbound_method(callable, __call__, &self, false);
    if(self != PY_NULL){
        p1[-(ARGC + 2)] = call_f;
        p1[-(ARGC + 1)] = self;
        // [call_f, self, args..., kwargs...]
        return vectorcall(ARGC, KWARGC, false);
    }
    TypeError(OBJ_NAME(_t(callable)).escape() + " object is not callable");
    return nullptr;
}

inline PyObject* VM::_py_call(PyObject** p0, PyObject* callable, ArgsView args, ArgsView kwargs){
    // callable must be a `function` object
    if(s_data.is_overflow()) StackOverflowError();

    const Function& fn = CAST(Function&, callable);
    const CodeObject* co = fn.decl->code.get();
    PyObject* _module = fn._module != nullptr ? fn._module : callstack.top()._module;

    if(args.size() < fn.decl->args.size()){
        vm->TypeError(fmt(
            "expected ",
            fn.decl->args.size(),
            " positional arguments, but got ",
            args.size(),
            " (", fn.decl->code->name, ')'
        ));
    }

    // if this function is simple, a.k.a, no kwargs and no *args and not a generator
    // we can use a fast path to avoid using buffer copy
    if(fn.is_simple && kwargs.size()==0){
        if(args.size() > fn.decl->args.size()) TypeError("too many positional arguments");
        int spaces = co->varnames.size() - fn.decl->args.size();
        for(int j=0; j<spaces; j++) PUSH(nullptr);
        callstack.emplace(&s_data, p0, co, _module, callable, FastLocals(co, args.begin()));
        return nullptr;
    }

    int i = 0;
    static THREAD_LOCAL PyObject* buffer[PK_MAX_CO_VARNAMES];

    // prepare args
    for(int index: fn.decl->args) buffer[index] = args[i++];
    // set extra varnames to nullptr
    for(int j=i; j<co->varnames.size(); j++) buffer[j] = nullptr;

    // prepare kwdefaults
    for(auto& kv: fn.decl->kwargs) buffer[kv.key] = kv.value;
    
    // handle *args
    if(fn.decl->starred_arg != -1){
        List vargs;        // handle *args
        while(i < args.size()) vargs.push_back(args[i++]);
        buffer[fn.decl->starred_arg] = VAR(Tuple(std::move(vargs)));
    }else{
        // kwdefaults override
        for(auto& kv: fn.decl->kwargs){
            if(i < args.size()){
                buffer[kv.key] = args[i++];
            }else{
                break;
            }
        }
        if(i < args.size()) TypeError(fmt("too many arguments", " (", fn.decl->code->name, ')'));
    }
    
    for(int i=0; i<kwargs.size(); i+=2){
        StrName key = CAST(int, kwargs[i]);
        int index = co->varnames_inv.try_get(key);
        if(index<0) TypeError(fmt(key.escape(), " is an invalid keyword argument for ", co->name, "()"));
        buffer[index] = kwargs[i+1];
    }
    
    s_data.reset(p0);
    if(co->is_generator){
        PyObject* ret = PyIter(Generator(
            this,
            Frame(&s_data, nullptr, co, _module, callable),
            ArgsView(buffer, buffer + co->varnames.size())
        ));
        return ret;
    }

    // copy buffer to stack
    for(int i=0; i<co->varnames.size(); i++) PUSH(buffer[i]);
    callstack.emplace(&s_data, p0, co, _module, callable);
    return nullptr;
}

// https://docs.python.org/3/howto/descriptor.html#invocation-from-an-instance
inline PyObject* VM::getattr(PyObject* obj, StrName name, bool throw_err){
    PyObject* objtype = _t(obj);
    // handle super() proxy
    if(is_non_tagged_type(obj, tp_super)){
        const Super& super = OBJ_GET(Super, obj);
        obj = super.first;
        objtype = _t(super.second);
    }
    PyObject* cls_var = find_name_in_mro(objtype, name);
    if(cls_var != nullptr){
        // handle descriptor
        PyObject* descr_get = _t(cls_var)->attr().try_get(__get__);
        if(descr_get != nullptr) return call_method(cls_var, descr_get, obj);
    }
    // handle instance __dict__
    if(!is_tagged(obj) && obj->is_attr_valid()){
        PyObject* val = obj->attr().try_get(name);
        if(val != nullptr) return val;
    }
    if(cls_var != nullptr){
        // bound method is non-data descriptor
        if(is_non_tagged_type(cls_var, tp_function) || is_non_tagged_type(cls_var, tp_native_func)){
            return VAR(BoundMethod(obj, cls_var));
        }
        return cls_var;
    }
    if(throw_err) AttributeError(obj, name);
    return nullptr;
}

// used by OP_LOAD_METHOD
// try to load a unbound method (fallback to `getattr` if not found)
inline PyObject* VM::get_unbound_method(PyObject* obj, StrName name, PyObject** self, bool throw_err, bool fallback){
    *self = PY_NULL;
    PyObject* objtype = _t(obj);
    // handle super() proxy
    if(is_non_tagged_type(obj, tp_super)){
        const Super& super = OBJ_GET(Super, obj);
        obj = super.first;
        objtype = _t(super.second);
    }
    PyObject* cls_var = find_name_in_mro(objtype, name);

    if(fallback){
        if(cls_var != nullptr){
            // handle descriptor
            PyObject* descr_get = _t(cls_var)->attr().try_get(__get__);
            if(descr_get != nullptr) return call_method(cls_var, descr_get, obj);
        }
        // handle instance __dict__
        if(!is_tagged(obj) && obj->is_attr_valid()){
            PyObject* val = obj->attr().try_get(name);
            if(val != nullptr) return val;
        }
    }

    if(cls_var != nullptr){
        if(is_non_tagged_type(cls_var, tp_function) || is_non_tagged_type(cls_var, tp_native_func)){
            *self = obj;
        }
        return cls_var;
    }
    if(throw_err) AttributeError(obj, name);
    return nullptr;
}

inline void VM::setattr(PyObject* obj, StrName name, PyObject* value){
    PyObject* objtype = _t(obj);
    // handle super() proxy
    if(is_non_tagged_type(obj, tp_super)){
        Super& super = OBJ_GET(Super, obj);
        obj = super.first;
        objtype = _t(super.second);
    }
    PyObject* cls_var = find_name_in_mro(objtype, name);
    if(cls_var != nullptr){
        // handle descriptor
        PyObject* cls_var_t = _t(cls_var);
        if(cls_var_t->attr().contains(__get__)){
            PyObject* descr_set = cls_var_t->attr().try_get(__set__);
            if(descr_set != nullptr){
                call_method(cls_var, descr_set, obj, value);
            }else{
                TypeError(fmt("readonly attribute: ", name.escape()));
            }
            return;
        }
    }
    // handle instance __dict__
    if(is_tagged(obj) || !obj->is_attr_valid()) TypeError("cannot set attribute");
    obj->attr().set(name, value);
}

template<int ARGC>
void VM::bind_method(PyObject* obj, Str name, NativeFuncC fn) {
    check_type(obj, tp_type);
    obj->attr().set(name, VAR(NativeFunc(fn, ARGC, true)));
}

template<int ARGC>
void VM::bind_func(PyObject* obj, Str name, NativeFuncC fn) {
    obj->attr().set(name, VAR(NativeFunc(fn, ARGC, false)));
}

inline void VM::_error(Exception e){
    if(callstack.empty()){
        e.is_re = false;
        throw e;
    }
    PUSH(VAR(e));
    _raise();
}

inline void ManagedHeap::mark() {
    for(PyObject* obj: _no_gc) OBJ_MARK(obj);
    for(auto& frame : vm->callstack.data()) frame._gc_mark();
    for(PyObject* obj: vm->s_data) if(obj!=nullptr) OBJ_MARK(obj);
    if(vm->_gc_marker_ex != nullptr) vm->_gc_marker_ex(vm);
}

inline Str obj_type_name(VM *vm, Type type){
    return vm->_all_types[type].name;
}

#undef PY_VAR_INT
#undef PY_VAR_FLOAT

}   // namespace pkpy


namespace pkpy{

inline PyObject* VM::_run_top_frame(){
    FrameId frame = top_frame();
    const int base_id = frame.index;
    bool need_raise = false;

    // shared registers
    PyObject *_0, *_1, *_2;
    StrName _name;

    while(true){
#if DEBUG_EXTRA_CHECK
        if(frame.index < base_id) FATAL_ERROR();
#endif
        try{
            if(need_raise){ need_raise = false; _raise(); }
            // if(s_data.is_overflow()) StackOverflowError();
/**********************************************************************/
/* NOTE: 
 * Be aware of accidental gc!
 * DO NOT leave any strong reference of PyObject* in the C stack
 */
{
#define DISPATCH_OP_CALL() { frame = top_frame(); goto __NEXT_FRAME; }
__NEXT_FRAME:
    Bytecode byte = frame->next_bytecode();
    // cache
    const CodeObject* co = frame->co;
    const auto& co_consts = co->consts;
    const auto& co_blocks = co->blocks;

#if PK_ENABLE_COMPUTED_GOTO
static void* OP_LABELS[] = {
    #define OPCODE(name) &&CASE_OP_##name,
    #ifdef OPCODE

/**************************/
OPCODE(NO_OP)
/**************************/
OPCODE(POP_TOP)
OPCODE(DUP_TOP)
OPCODE(ROT_TWO)
OPCODE(PRINT_EXPR)
/**************************/
OPCODE(LOAD_CONST)
OPCODE(LOAD_NONE)
OPCODE(LOAD_TRUE)
OPCODE(LOAD_FALSE)
OPCODE(LOAD_INTEGER)
OPCODE(LOAD_ELLIPSIS)
OPCODE(LOAD_FUNCTION)
OPCODE(LOAD_NULL)
/**************************/
OPCODE(LOAD_FAST)
OPCODE(LOAD_NAME)
OPCODE(LOAD_NONLOCAL)
OPCODE(LOAD_GLOBAL)
OPCODE(LOAD_ATTR)
OPCODE(LOAD_METHOD)
OPCODE(LOAD_SUBSCR)

OPCODE(STORE_FAST)
OPCODE(STORE_NAME)
OPCODE(STORE_GLOBAL)
OPCODE(STORE_ATTR)
OPCODE(STORE_SUBSCR)

OPCODE(DELETE_FAST)
OPCODE(DELETE_NAME)
OPCODE(DELETE_GLOBAL)
OPCODE(DELETE_ATTR)
OPCODE(DELETE_SUBSCR)
/**************************/
OPCODE(BUILD_LIST)
OPCODE(BUILD_DICT)
OPCODE(BUILD_SET)
OPCODE(BUILD_SLICE)
OPCODE(BUILD_TUPLE)
OPCODE(BUILD_STRING)
/**************************/
OPCODE(BINARY_OP)
OPCODE(BINARY_ADD)
OPCODE(BINARY_SUB)
OPCODE(BINARY_MUL)
OPCODE(BINARY_FLOORDIV)
OPCODE(BINARY_MOD)

OPCODE(COMPARE_LT)
OPCODE(COMPARE_LE)
OPCODE(COMPARE_EQ)
OPCODE(COMPARE_NE)
OPCODE(COMPARE_GT)
OPCODE(COMPARE_GE)

OPCODE(BITWISE_LSHIFT)
OPCODE(BITWISE_RSHIFT)
OPCODE(BITWISE_AND)
OPCODE(BITWISE_OR)
OPCODE(BITWISE_XOR)

OPCODE(IS_OP)
OPCODE(CONTAINS_OP)
/**************************/
OPCODE(JUMP_ABSOLUTE)
OPCODE(POP_JUMP_IF_FALSE)
OPCODE(JUMP_IF_TRUE_OR_POP)
OPCODE(JUMP_IF_FALSE_OR_POP)
OPCODE(LOOP_CONTINUE)
OPCODE(LOOP_BREAK)
OPCODE(GOTO)
/**************************/
OPCODE(BEGIN_CALL)
OPCODE(CALL)
OPCODE(RETURN_VALUE)
OPCODE(YIELD_VALUE)
/**************************/
OPCODE(LIST_APPEND)
OPCODE(DICT_ADD)
OPCODE(SET_ADD)
/**************************/
OPCODE(UNARY_NEGATIVE)
OPCODE(UNARY_NOT)
/**************************/
OPCODE(GET_ITER)
OPCODE(FOR_ITER)
/**************************/
OPCODE(IMPORT_NAME)
OPCODE(IMPORT_STAR)
/**************************/
OPCODE(UNPACK_SEQUENCE)
OPCODE(UNPACK_EX)
OPCODE(UNPACK_UNLIMITED)
/**************************/
OPCODE(BEGIN_CLASS)
OPCODE(END_CLASS)
OPCODE(STORE_CLASS_ATTR)
/**************************/
OPCODE(WITH_ENTER)
OPCODE(WITH_EXIT)
/**************************/
OPCODE(ASSERT)
OPCODE(EXCEPTION_MATCH)
OPCODE(RAISE)
OPCODE(RE_RAISE)
/**************************/
OPCODE(SETUP_DOCSTRING)
OPCODE(FORMAT_STRING)
#endif
    #undef OPCODE
};

#define DISPATCH() { byte = frame->next_bytecode(); goto *OP_LABELS[byte.op];}
#define TARGET(op) CASE_OP_##op:
goto *OP_LABELS[byte.op];

#else
#define TARGET(op) case OP_##op:
#define DISPATCH() { byte = frame->next_bytecode(); goto __NEXT_STEP;}

__NEXT_STEP:;
#if DEBUG_CEVAL_STEP
    _log_s_data();
#endif
#if DEBUG_CEVAL_STEP_MIN
    std::cout << OP_NAMES[byte.op] << std::endl;
#endif
    switch (byte.op)
    {
#endif
    TARGET(NO_OP) DISPATCH();
    /*****************************************/
    TARGET(POP_TOP) POP(); DISPATCH();
    TARGET(DUP_TOP) PUSH(TOP()); DISPATCH();
    TARGET(ROT_TWO) std::swap(TOP(), SECOND()); DISPATCH();
    TARGET(PRINT_EXPR)
        if(TOP() != None) *_stdout << CAST(Str&, asRepr(TOP())) << '\n';
        POP();
        DISPATCH();
    /*****************************************/
    TARGET(LOAD_CONST)
        heap._auto_collect();
        PUSH(co_consts[byte.arg]);
        DISPATCH();
    TARGET(LOAD_NONE) PUSH(None); DISPATCH();
    TARGET(LOAD_TRUE) PUSH(True); DISPATCH();
    TARGET(LOAD_FALSE) PUSH(False); DISPATCH();
    TARGET(LOAD_INTEGER) PUSH(VAR(byte.arg)); DISPATCH();
    TARGET(LOAD_ELLIPSIS) PUSH(Ellipsis); DISPATCH();
    TARGET(LOAD_FUNCTION) {
        FuncDecl_ decl = co->func_decls[byte.arg];
        bool is_simple = decl->starred_arg==-1 && decl->kwargs.size()==0 && !decl->code->is_generator;
        PyObject* obj;
        if(decl->nested){
            obj = VAR(Function({decl, is_simple, frame->_module, frame->_locals.to_namedict()}));
        }else{
            obj = VAR(Function({decl, is_simple, frame->_module}));
        }
        PUSH(obj);
    } DISPATCH();
    TARGET(LOAD_NULL) PUSH(PY_NULL); DISPATCH();
    /*****************************************/
    TARGET(LOAD_FAST) {
        heap._auto_collect();
        _0 = frame->_locals[byte.arg];
        if(_0 == nullptr) vm->NameError(co->varnames[byte.arg]);
        PUSH(_0);
    } DISPATCH();
    TARGET(LOAD_NAME)
        heap._auto_collect();
        _name = StrName(byte.arg);
        _0 = frame->_locals.try_get(_name);
        if(_0 != nullptr) { PUSH(_0); DISPATCH(); }
        _0 = frame->f_closure_try_get(_name);
        if(_0 != nullptr) { PUSH(_0); DISPATCH(); }
        _0 = frame->f_globals().try_get(_name);
        if(_0 != nullptr) { PUSH(_0); DISPATCH(); }
        _0 = vm->builtins->attr().try_get(_name);
        if(_0 != nullptr) { PUSH(_0); DISPATCH(); }
        vm->NameError(_name);
        DISPATCH();
    TARGET(LOAD_NONLOCAL) {
        heap._auto_collect();
        _name = StrName(byte.arg);
        _0 = frame->f_closure_try_get(_name);
        if(_0 != nullptr) { PUSH(_0); DISPATCH(); }
        _0 = frame->f_globals().try_get(_name);
        if(_0 != nullptr) { PUSH(_0); DISPATCH(); }
        _0 = vm->builtins->attr().try_get(_name);
        if(_0 != nullptr) { PUSH(_0); DISPATCH(); }
        vm->NameError(_name);
        DISPATCH();
    } DISPATCH();
    TARGET(LOAD_GLOBAL)
        heap._auto_collect();
        _name = StrName(byte.arg);
        _0 = frame->f_globals().try_get(_name);
        if(_0 != nullptr) { PUSH(_0); DISPATCH(); }
        _0 = vm->builtins->attr().try_get(_name);
        if(_0 != nullptr) { PUSH(_0); DISPATCH(); }
        vm->NameError(_name);
        DISPATCH();
    TARGET(LOAD_ATTR)
        TOP() = getattr(TOP(), StrName(byte.arg));
        DISPATCH();
    TARGET(LOAD_METHOD)
        TOP() = get_unbound_method(TOP(), StrName(byte.arg), &_0, true, true);
        PUSH(_0);
        DISPATCH();
    TARGET(LOAD_SUBSCR)
        _1 = POPX();
        _0 = TOP();
        TOP() = call_method(_0, __getitem__, _1);
        DISPATCH();
    TARGET(STORE_FAST)
        frame->_locals[byte.arg] = POPX();
        DISPATCH();
    TARGET(STORE_NAME)
        _name = StrName(byte.arg);
        _0 = POPX();
        if(frame->_callable != nullptr){
            bool ok = frame->_locals.try_set(_name, _0);
            if(!ok) vm->NameError(_name);
        }else{
            frame->f_globals().set(_name, _0);
        }
        DISPATCH();
    TARGET(STORE_GLOBAL)
        frame->f_globals().set(StrName(byte.arg), POPX());
        DISPATCH();
    TARGET(STORE_ATTR) {
        _0 = TOP();         // a
        _1 = SECOND();      // val
        setattr(_0, StrName(byte.arg), _1);
        STACK_SHRINK(2);
    } DISPATCH();
    TARGET(STORE_SUBSCR)
        _2 = POPX();        // b
        _1 = POPX();        // a
        _0 = POPX();        // val
        call_method(_1, __setitem__, _2, _0);
        DISPATCH();
    TARGET(DELETE_FAST)
        _0 = frame->_locals[byte.arg];
        if(_0 == nullptr) vm->NameError(co->varnames[byte.arg]);
        frame->_locals[byte.arg] = nullptr;
        DISPATCH();
    TARGET(DELETE_NAME)
        _name = StrName(byte.arg);
        if(frame->_callable != nullptr){
            if(!frame->_locals.contains(_name)) vm->NameError(_name);
            frame->_locals.erase(_name);
        }else{
            if(!frame->f_globals().contains(_name)) vm->NameError(_name);
            frame->f_globals().erase(_name);
        }
        DISPATCH();
    TARGET(DELETE_GLOBAL)
        _name = StrName(byte.arg);
        if(frame->f_globals().contains(_name)){
            frame->f_globals().erase(_name);
        }else{
            NameError(_name);
        }
        DISPATCH();
    TARGET(DELETE_ATTR)
        _0 = POPX();
        _name = StrName(byte.arg);
        if(!_0->is_attr_valid()) TypeError("cannot delete attribute");
        if(!_0->attr().contains(_name)) AttributeError(_0, _name);
        _0->attr().erase(_name);
        DISPATCH();
    TARGET(DELETE_SUBSCR)
        _1 = POPX();
        _0 = POPX();
        call_method(_0, __delitem__, _1);
        DISPATCH();
    /*****************************************/
    TARGET(BUILD_LIST)
        _0 = VAR(STACK_VIEW(byte.arg).to_list());
        STACK_SHRINK(byte.arg);
        PUSH(_0);
        DISPATCH();
    TARGET(BUILD_DICT) {
        PyObject* t = VAR(STACK_VIEW(byte.arg).to_tuple());
        PyObject* obj = call(builtins->attr(m_dict), t);
        STACK_SHRINK(byte.arg);
        PUSH(obj);
    } DISPATCH();
    TARGET(BUILD_SET) {
        PyObject* t = VAR(STACK_VIEW(byte.arg).to_tuple());
        PyObject* obj = call(builtins->attr(m_set), t);
        STACK_SHRINK(byte.arg);
        PUSH(obj);
    } DISPATCH();
    TARGET(BUILD_SLICE)
        _2 = POPX();    // step
        _1 = POPX();    // stop
        _0 = POPX();    // start
        PUSH(VAR(Slice(_0, _1, _2)));
        DISPATCH();
    TARGET(BUILD_TUPLE)
        _0 = VAR(STACK_VIEW(byte.arg).to_tuple());
        STACK_SHRINK(byte.arg);
        PUSH(_0);
        DISPATCH();
    TARGET(BUILD_STRING) {
        std::stringstream ss;
        auto view = STACK_VIEW(byte.arg);
        for(PyObject* obj : view) ss << CAST(Str&, asStr(obj));
        STACK_SHRINK(byte.arg);
        PUSH(VAR(ss.str()));
    } DISPATCH();
    /*****************************************/
    TARGET(BINARY_OP)
        _1 = POPX();  // b
        _0 = TOP();   // a
        TOP() = call_method(_0, BINARY_SPECIAL_METHODS[byte.arg], _1);
        DISPATCH();

#define INT_BINARY_OP(op, func)                             \
        if(is_both_int(TOP(), SECOND())){                   \
            i64 b = _CAST(i64, TOP());                      \
            i64 a = _CAST(i64, SECOND());                   \
            POP();                                          \
            TOP() = VAR(a op b);                            \
        }else{                                              \
            _1 = POPX();                                    \
            _0 = TOP();                                     \
            TOP() = call_method(_0, func, _1);              \
        }

    TARGET(BINARY_ADD)
        INT_BINARY_OP(+, __add__)
        DISPATCH()
    TARGET(BINARY_SUB)
        INT_BINARY_OP(-, __sub__)
        DISPATCH()
    TARGET(BINARY_MUL)
        INT_BINARY_OP(*, __mul__)
        DISPATCH()
    TARGET(BINARY_FLOORDIV)
        INT_BINARY_OP(/, __floordiv__)
        DISPATCH()
    TARGET(BINARY_MOD)
        INT_BINARY_OP(%, __mod__)
        DISPATCH()
    TARGET(COMPARE_LT)
        INT_BINARY_OP(<, __lt__)
        DISPATCH()
    TARGET(COMPARE_LE)
        INT_BINARY_OP(<=, __le__)
        DISPATCH()
    TARGET(COMPARE_EQ)
        INT_BINARY_OP(==, __eq__)
        DISPATCH()
    TARGET(COMPARE_NE)
        INT_BINARY_OP(!=, __ne__)
        DISPATCH()
    TARGET(COMPARE_GT)
        INT_BINARY_OP(>, __gt__)
        DISPATCH()
    TARGET(COMPARE_GE)
        INT_BINARY_OP(>=, __ge__)
        DISPATCH()
    TARGET(BITWISE_LSHIFT)
        INT_BINARY_OP(<<, __lshift__)
        DISPATCH()
    TARGET(BITWISE_RSHIFT)
        INT_BINARY_OP(>>, __rshift__)
        DISPATCH()
    TARGET(BITWISE_AND)
        INT_BINARY_OP(&, __and__)
        DISPATCH()
    TARGET(BITWISE_OR)
        INT_BINARY_OP(|, __or__)
        DISPATCH()
    TARGET(BITWISE_XOR)
        INT_BINARY_OP(^, __xor__)
        DISPATCH()
#undef INT_BINARY_OP

    TARGET(IS_OP)
        _1 = POPX();    // rhs
        _0 = TOP();     // lhs
        if(byte.arg == 1){
            TOP() = VAR(_0 != _1);
        }else{
            TOP() = VAR(_0 == _1);
        }
        DISPATCH();
    TARGET(CONTAINS_OP)
        // a in b -> b __contains__ a
        _0 = call_method(TOP(), __contains__, SECOND());
        POP();
        if(byte.arg == 1){
            TOP() = VAR(!CAST(bool, _0));
        }else{
            TOP() = VAR(CAST(bool, _0));
        }
        DISPATCH();
    /*****************************************/
    TARGET(JUMP_ABSOLUTE)
        frame->jump_abs(byte.arg);
        DISPATCH();
    TARGET(POP_JUMP_IF_FALSE)
        if(!asBool(POPX())) frame->jump_abs(byte.arg);
        DISPATCH();
    TARGET(JUMP_IF_TRUE_OR_POP)
        if(asBool(TOP()) == true) frame->jump_abs(byte.arg);
        else POP();
        DISPATCH();
    TARGET(JUMP_IF_FALSE_OR_POP)
        if(asBool(TOP()) == false) frame->jump_abs(byte.arg);
        else POP();
        DISPATCH();
    TARGET(LOOP_CONTINUE)
        frame->jump_abs(co_blocks[byte.block].start);
        DISPATCH();
    TARGET(LOOP_BREAK)
        frame->jump_abs_break(co_blocks[byte.block].end);
        DISPATCH();
    TARGET(GOTO) {
        StrName name(byte.arg);
        int index = co->labels.try_get(name);
        if(index < 0) _error("KeyError", fmt("label ", name.escape(), " not found"));
        frame->jump_abs_break(index);
    } DISPATCH();
    /*****************************************/
    TARGET(BEGIN_CALL)
        PUSH(PY_BEGIN_CALL);
        DISPATCH();
    TARGET(CALL)
        _0 = vectorcall(
            byte.arg & 0xFFFF,          // ARGC
            (byte.arg>>16) & 0xFFFF,    // KWARGC
            true
        );
        if(_0 == PY_OP_CALL) DISPATCH_OP_CALL();
        PUSH(_0);
        DISPATCH();
    TARGET(RETURN_VALUE)
        _0 = POPX();
        _pop_frame();
        if(frame.index == base_id){       // [ frameBase<- ]
            return _0;
        }else{
            frame = top_frame();
            PUSH(_0);
            goto __NEXT_FRAME;
        }
    TARGET(YIELD_VALUE)
        return PY_OP_YIELD;
    /*****************************************/
    TARGET(LIST_APPEND)
        _0 = POPX();
        CAST(List&, SECOND()).push_back(_0);
        DISPATCH();
    TARGET(DICT_ADD) {
        _0 = POPX();
        Tuple& t = CAST(Tuple&, _0);
        call_method(SECOND(), __setitem__, t[0], t[1]);
    } DISPATCH();
    TARGET(SET_ADD)
        _0 = POPX();
        call_method(SECOND(), m_add, _0);
        DISPATCH();
    /*****************************************/
    TARGET(UNARY_NEGATIVE)
        TOP() = num_negated(TOP());
        DISPATCH();
    TARGET(UNARY_NOT)
        TOP() = VAR(!asBool(TOP()));
        DISPATCH();
    /*****************************************/
    TARGET(GET_ITER)
        TOP() = asIter(TOP());
        DISPATCH();
    TARGET(FOR_ITER)
        _0 = PyIterNext(TOP());
        if(_0 != StopIteration){
            PUSH(_0);
        }else{
            frame->jump_abs_break(co_blocks[byte.block].end);
        }
        DISPATCH();
    /*****************************************/
    TARGET(IMPORT_NAME) {
        StrName name(byte.arg);
        PyObject* ext_mod = _modules.try_get(name);
        if(ext_mod == nullptr){
            Str source;
            auto it = _lazy_modules.find(name);
            if(it == _lazy_modules.end()){
                Bytes b = _read_file_cwd(fmt(name, ".py"));
                if(!b) _error("ImportError", fmt("module ", name.escape(), " not found"));
                source = Str(b.str());
            }else{
                source = it->second;
                _lazy_modules.erase(it);
            }
            CodeObject_ code = compile(source, Str(name.sv())+".py", EXEC_MODE);
            PyObject* new_mod = new_module(name);
            _exec(code, new_mod);
            new_mod->attr()._try_perfect_rehash();
            PUSH(new_mod);
        }else{
            PUSH(ext_mod);
        }
    } DISPATCH();
    TARGET(IMPORT_STAR) {
        PyObject* obj = POPX();
        for(auto& [name, value]: obj->attr().items()){
            std::string_view s = name.sv();
            if(s.empty() || s[0] == '_') continue;
            frame->f_globals().set(name, value);
        }
    }; DISPATCH();
    /*****************************************/
    TARGET(UNPACK_SEQUENCE)
    TARGET(UNPACK_EX) {
        auto _lock = heap.gc_scope_lock();  // lock the gc via RAII!!
        PyObject* iter = asIter(POPX());
        for(int i=0; i<byte.arg; i++){
            PyObject* item = PyIterNext(iter);
            if(item == StopIteration) ValueError("not enough values to unpack");
            PUSH(item);
        }
        // handle extra items
        if(byte.op == OP_UNPACK_EX){
            List extras;
            while(true){
                PyObject* item = PyIterNext(iter);
                if(item == StopIteration) break;
                extras.push_back(item);
            }
            PUSH(VAR(extras));
        }else{
            if(PyIterNext(iter) != StopIteration) ValueError("too many values to unpack");
        }
    } DISPATCH();
    TARGET(UNPACK_UNLIMITED) {
        auto _lock = heap.gc_scope_lock();  // lock the gc via RAII!!
        PyObject* iter = asIter(POPX());
        _0 = PyIterNext(iter);
        while(_0 != StopIteration){
            PUSH(_0);
            _0 = PyIterNext(iter);
        }
    } DISPATCH();
    /*****************************************/
    TARGET(BEGIN_CLASS) {
        StrName name(byte.arg);
        PyObject* super_cls = POPX();
        if(super_cls == None) super_cls = _t(tp_object);
        check_type(super_cls, tp_type);
        PyObject* cls = new_type_object(frame->_module, name, OBJ_GET(Type, super_cls));
        PUSH(cls);
    } DISPATCH();
    TARGET(END_CLASS)
        _0 = POPX();
        _0->attr()._try_perfect_rehash();
        DISPATCH();
    TARGET(STORE_CLASS_ATTR)
        _name = StrName(byte.arg);
        _0 = POPX();
        TOP()->attr().set(_name, _0);
        DISPATCH();
    /*****************************************/
    // // TODO: using "goto" inside with block may cause __exit__ not called
    TARGET(WITH_ENTER)
        call_method(POPX(), __enter__);
        DISPATCH();
    TARGET(WITH_EXIT)
        call_method(POPX(), __exit__);
        DISPATCH();
    /*****************************************/
    TARGET(ASSERT) {
        PyObject* obj = TOP();
        Str msg;
        if(is_type(obj, tp_tuple)){
            auto& t = CAST(Tuple&, obj);
            if(t.size() != 2) ValueError("assert tuple must have 2 elements");
            obj = t[0];
            msg = CAST(Str&, asStr(t[1]));
        }
        bool ok = asBool(obj);
        POP();
        if(!ok) _error("AssertionError", msg);
    } DISPATCH();
    TARGET(EXCEPTION_MATCH) {
        const auto& e = CAST(Exception&, TOP());
        _name = StrName(byte.arg);
        PUSH(VAR(e.match_type(_name)));
    } DISPATCH();
    TARGET(RAISE) {
        PyObject* obj = POPX();
        Str msg = obj == None ? "" : CAST(Str, asStr(obj));
        _error(StrName(byte.arg), msg);
    } DISPATCH();
    TARGET(RE_RAISE) _raise(); DISPATCH();
    /*****************************************/
    TARGET(SETUP_DOCSTRING)
        TOP()->attr().set(__doc__, co_consts[byte.arg]);
        DISPATCH();
    TARGET(FORMAT_STRING) {
        _0 = POPX();
        const Str& spec = CAST(Str&, co_consts[byte.arg]);
        PUSH(format(spec, _0));
    } DISPATCH();
#if !PK_ENABLE_COMPUTED_GOTO
#if DEBUG_EXTRA_CHECK
    default: throw std::runtime_error(fmt(OP_NAMES[byte.op], " is not implemented"));
#else
    default: UNREACHABLE();
#endif
    }
#endif
}

#undef DISPATCH
#undef TARGET
#undef DISPATCH_OP_CALL
/**********************************************************************/
            UNREACHABLE();
        }catch(HandledException& e){
            continue;
        }catch(UnhandledException& e){
            PyObject* obj = POPX();
            Exception& _e = CAST(Exception&, obj);
            _e.st_push(frame->snapshot());
            _pop_frame();
            if(callstack.empty()){
#if DEBUG_FULL_EXCEPTION
                std::cerr << _e.summary() << std::endl;
#endif
                throw _e;
            }
            frame = top_frame();
            PUSH(obj);
            if(frame.index < base_id) throw ToBeRaisedException();
            need_raise = true;
        }catch(ToBeRaisedException& e){
            need_raise = true;
        }
    }
}

#undef TOP
#undef SECOND
#undef THIRD
#undef PEEK
#undef STACK_SHRINK
#undef PUSH
#undef POP
#undef POPX
#undef STACK_VIEW

} // namespace pkpy


#include <algorithm>

namespace pkpy{

struct CodeEmitContext;

struct Expr{
    int line = 0;
    virtual ~Expr() = default;
    virtual void emit(CodeEmitContext* ctx) = 0;
    virtual std::string str() const = 0;

    virtual bool is_starred() const { return false; }
    virtual bool is_literal() const { return false; }
    virtual bool is_json_object() const { return false; }
    virtual bool is_attrib() const { return false; }

    // for OP_DELETE_XXX
    [[nodiscard]] virtual bool emit_del(CodeEmitContext* ctx) { return false; }

    // for OP_STORE_XXX
    [[nodiscard]] virtual bool emit_store(CodeEmitContext* ctx) { return false; }
};

struct CodeEmitContext{
    VM* vm;
    CodeObject_ co;
    stack<Expr_> s_expr;
    int level;
    CodeEmitContext(VM* vm, CodeObject_ co, int level): vm(vm), co(co), level(level) {}

    int curr_block_i = 0;
    bool is_compiling_class = false;
    int for_loop_depth = 0;

    bool is_curr_block_loop() const {
        return co->blocks[curr_block_i].type == FOR_LOOP || co->blocks[curr_block_i].type == WHILE_LOOP;
    }

    void enter_block(CodeBlockType type){
        if(type == FOR_LOOP) for_loop_depth++;
        co->blocks.push_back(CodeBlock(
            type, curr_block_i, for_loop_depth, (int)co->codes.size()
        ));
        curr_block_i = co->blocks.size()-1;
    }

    void exit_block(){
        if(co->blocks[curr_block_i].type == FOR_LOOP) for_loop_depth--;
        co->blocks[curr_block_i].end = co->codes.size();
        curr_block_i = co->blocks[curr_block_i].parent;
        if(curr_block_i < 0) FATAL_ERROR();
    }

    // clear the expression stack and generate bytecode
    void emit_expr(){
        if(s_expr.size() != 1){
            throw std::runtime_error("s_expr.size() != 1\n" + _log_s_expr());
        }
        Expr_ expr = s_expr.popx();
        expr->emit(this);
    }

    std::string _log_s_expr(){
        std::stringstream ss;
        for(auto& e: s_expr.data()) ss << e->str() << " ";
        return ss.str();
    }

    int emit(Opcode opcode, int arg, int line) {
        co->codes.push_back(
            Bytecode{(uint16_t)opcode, (uint16_t)curr_block_i, arg}
        );
        co->lines.push_back(line);
        int i = co->codes.size() - 1;
        if(line==BC_KEEPLINE){
            if(i>=1) co->lines[i] = co->lines[i-1];
            else co->lines[i] = 1;
        }
        return i;
    }

    void patch_jump(int index) {
        int target = co->codes.size();
        co->codes[index].arg = target;
    }

    bool add_label(StrName name){
        if(co->labels.contains(name)) return false;
        co->labels.set(name, co->codes.size());
        return true;
    }

    int add_varname(StrName name){
        int index = co->varnames_inv.try_get(name);
        if(index >= 0) return index;
        co->varnames.push_back(name);
        index = co->varnames.size() - 1;
        co->varnames_inv.set(name, index);
        return index;
    }

    int add_const(PyObject* v){
        // simple deduplication, only works for int/float
        for(int i=0; i<co->consts.size(); i++){
            if(co->consts[i] == v) return i;
        }
        co->consts.push_back(v);
        return co->consts.size() - 1;
    }

    int add_func_decl(FuncDecl_ decl){
        co->func_decls.push_back(decl);
        return co->func_decls.size() - 1;
    }
};

struct NameExpr: Expr{
    StrName name;
    NameScope scope;
    NameExpr(StrName name, NameScope scope): name(name), scope(scope) {}

    std::string str() const override { return fmt("Name(", name.escape(), ")"); }

    void emit(CodeEmitContext* ctx) override {
        int index = ctx->co->varnames_inv.try_get(name);
        if(scope == NAME_LOCAL && index >= 0){
            ctx->emit(OP_LOAD_FAST, index, line);
        }else{
            Opcode op = ctx->level <= 1 ? OP_LOAD_GLOBAL : OP_LOAD_NONLOCAL;
            // we cannot determine the scope when calling exec()/eval()
            if(scope == NAME_GLOBAL_UNKNOWN) op = OP_LOAD_NAME;
            ctx->emit(op, StrName(name).index, line);
        }
    }

    bool emit_del(CodeEmitContext* ctx) override {
        switch(scope){
            case NAME_LOCAL:
                ctx->emit(OP_DELETE_FAST, ctx->add_varname(name), line);
                break;
            case NAME_GLOBAL:
                ctx->emit(OP_DELETE_GLOBAL, StrName(name).index, line);
                break;
            case NAME_GLOBAL_UNKNOWN:
                ctx->emit(OP_DELETE_NAME, StrName(name).index, line);
                break;
            default: FATAL_ERROR(); break;
        }
        return true;
    }

    bool emit_store(CodeEmitContext* ctx) override {
        if(ctx->is_compiling_class){
            int index = StrName(name).index;
            ctx->emit(OP_STORE_CLASS_ATTR, index, line);
            return true;
        }
        switch(scope){
            case NAME_LOCAL:
                ctx->emit(OP_STORE_FAST, ctx->add_varname(name), line);
                break;
            case NAME_GLOBAL:
                ctx->emit(OP_STORE_GLOBAL, StrName(name).index, line);
                break;
            case NAME_GLOBAL_UNKNOWN:
                ctx->emit(OP_STORE_NAME, StrName(name).index, line);
                break;
            default: FATAL_ERROR(); break;
        }
        return true;
    }
};

struct StarredExpr: Expr{
    Expr_ child;
    StarredExpr(Expr_&& child): child(std::move(child)) {}
    std::string str() const override { return "Starred()"; }

    bool is_starred() const override { return true; }

    void emit(CodeEmitContext* ctx) override {
        child->emit(ctx);
        ctx->emit(OP_UNPACK_UNLIMITED, BC_NOARG, line);
    }

    bool emit_store(CodeEmitContext* ctx) override {
        // simply proxy to child
        return child->emit_store(ctx);
    }
};


struct NotExpr: Expr{
    Expr_ child;
    NotExpr(Expr_&& child): child(std::move(child)) {}
    std::string str() const override { return "Not()"; }

    void emit(CodeEmitContext* ctx) override {
        child->emit(ctx);
        ctx->emit(OP_UNARY_NOT, BC_NOARG, line);
    }
};

struct AndExpr: Expr{
    Expr_ lhs;
    Expr_ rhs;
    std::string str() const override { return "And()"; }

    void emit(CodeEmitContext* ctx) override {
        lhs->emit(ctx);
        int patch = ctx->emit(OP_JUMP_IF_FALSE_OR_POP, BC_NOARG, line);
        rhs->emit(ctx);
        ctx->patch_jump(patch);
    }
};

struct OrExpr: Expr{
    Expr_ lhs;
    Expr_ rhs;
    std::string str() const override { return "Or()"; }

    void emit(CodeEmitContext* ctx) override {
        lhs->emit(ctx);
        int patch = ctx->emit(OP_JUMP_IF_TRUE_OR_POP, BC_NOARG, line);
        rhs->emit(ctx);
        ctx->patch_jump(patch);
    }
};

// [None, True, False, ...]
struct Literal0Expr: Expr{
    TokenIndex token;
    Literal0Expr(TokenIndex token): token(token) {}
    std::string str() const override { return TK_STR(token); }

    void emit(CodeEmitContext* ctx) override {
        switch (token) {
            case TK("None"):    ctx->emit(OP_LOAD_NONE, BC_NOARG, line); break;
            case TK("True"):    ctx->emit(OP_LOAD_TRUE, BC_NOARG, line); break;
            case TK("False"):   ctx->emit(OP_LOAD_FALSE, BC_NOARG, line); break;
            case TK("..."):     ctx->emit(OP_LOAD_ELLIPSIS, BC_NOARG, line); break;
            default: FATAL_ERROR();
        }
    }

    bool is_json_object() const override { return true; }
};

// @num, @str which needs to invoke OP_LOAD_CONST
struct LiteralExpr: Expr{
    TokenValue value;
    LiteralExpr(TokenValue value): value(value) {}
    std::string str() const override {
        if(std::holds_alternative<i64>(value)){
            return std::to_string(std::get<i64>(value));
        }

        if(std::holds_alternative<f64>(value)){
            return std::to_string(std::get<f64>(value));
        }

        if(std::holds_alternative<Str>(value)){
            Str s = std::get<Str>(value).escape();
            return s.str();
        }

        FATAL_ERROR();
    }

    void emit(CodeEmitContext* ctx) override {
        VM* vm = ctx->vm;
        PyObject* obj = nullptr;
        if(std::holds_alternative<i64>(value)){
            i64 _val = std::get<i64>(value);
            if(_val >= INT16_MIN && _val <= INT16_MAX){
                ctx->emit(OP_LOAD_INTEGER, (int)_val, line);
                return;
            }
            obj = VAR(_val);
        }
        if(std::holds_alternative<f64>(value)){
            obj = VAR(std::get<f64>(value));
        }
        if(std::holds_alternative<Str>(value)){
            obj = VAR(std::get<Str>(value));
        }
        if(obj == nullptr) FATAL_ERROR();
        ctx->emit(OP_LOAD_CONST, ctx->add_const(obj), line);
    }

    bool is_literal() const override { return true; }
    bool is_json_object() const override { return true; }
};

struct NegatedExpr: Expr{
    Expr_ child;
    NegatedExpr(Expr_&& child): child(std::move(child)) {}
    std::string str() const override { return "Negated()"; }

    void emit(CodeEmitContext* ctx) override {
        VM* vm = ctx->vm;
        // if child is a int of float, do constant folding
        if(child->is_literal()){
            LiteralExpr* lit = static_cast<LiteralExpr*>(child.get());
            if(std::holds_alternative<i64>(lit->value)){
                i64 _val = -std::get<i64>(lit->value);
                if(_val >= INT16_MIN && _val <= INT16_MAX){
                    ctx->emit(OP_LOAD_INTEGER, (int)_val, line);
                }else{
                    ctx->emit(OP_LOAD_CONST, ctx->add_const(VAR(_val)), line);
                }
                return;
            }
            if(std::holds_alternative<f64>(lit->value)){
                PyObject* obj = VAR(-std::get<f64>(lit->value));
                ctx->emit(OP_LOAD_CONST, ctx->add_const(obj), line);
                return;
            }
        }
        child->emit(ctx);
        ctx->emit(OP_UNARY_NEGATIVE, BC_NOARG, line);
    }

    bool is_json_object() const override {
        return child->is_literal();
    }
};

struct SliceExpr: Expr{
    Expr_ start;
    Expr_ stop;
    Expr_ step;
    std::string str() const override { return "Slice()"; }

    void emit(CodeEmitContext* ctx) override {
        if(start){
            start->emit(ctx);
        }else{
            ctx->emit(OP_LOAD_NONE, BC_NOARG, line);
        }

        if(stop){
            stop->emit(ctx);
        }else{
            ctx->emit(OP_LOAD_NONE, BC_NOARG, line);
        }

        if(step){
            step->emit(ctx);
        }else{
            ctx->emit(OP_LOAD_NONE, BC_NOARG, line);
        }

        ctx->emit(OP_BUILD_SLICE, BC_NOARG, line);
    }
};

struct DictItemExpr: Expr{
    Expr_ key;
    Expr_ value;
    std::string str() const override { return "DictItem()"; }

    void emit(CodeEmitContext* ctx) override {
        value->emit(ctx);
        key->emit(ctx);     // reverse order
        ctx->emit(OP_BUILD_TUPLE, 2, line);
    }
};

struct SequenceExpr: Expr{
    std::vector<Expr_> items;
    SequenceExpr(std::vector<Expr_>&& items): items(std::move(items)) {}
    virtual Opcode opcode() const = 0;

    void emit(CodeEmitContext* ctx) override {
        for(auto& item: items) item->emit(ctx);
        ctx->emit(opcode(), items.size(), line);
    }
};

struct ListExpr: SequenceExpr{
    using SequenceExpr::SequenceExpr;
    std::string str() const override { return "List()"; }
    Opcode opcode() const override { return OP_BUILD_LIST; }

    bool is_json_object() const override { return true; }
};

struct DictExpr: SequenceExpr{
    using SequenceExpr::SequenceExpr;
    std::string str() const override { return "Dict()"; }
    Opcode opcode() const override { return OP_BUILD_DICT; }

    bool is_json_object() const override { return true; }
};

struct SetExpr: SequenceExpr{
    using SequenceExpr::SequenceExpr;
    std::string str() const override { return "Set()"; }
    Opcode opcode() const override { return OP_BUILD_SET; }
};

struct TupleExpr: SequenceExpr{
    using SequenceExpr::SequenceExpr;
    std::string str() const override { return "Tuple()"; }
    Opcode opcode() const override { return OP_BUILD_TUPLE; }

    bool emit_store(CodeEmitContext* ctx) override {
        // TOS is an iterable
        // items may contain StarredExpr, we should check it
        int starred_i = -1;
        for(int i=0; i<items.size(); i++){
            if(!items[i]->is_starred()) continue;
            if(starred_i == -1) starred_i = i;
            else return false;  // multiple StarredExpr not allowed
        }

        if(starred_i == -1){
            ctx->emit(OP_UNPACK_SEQUENCE, items.size(), line);
        }else{
            // starred assignment target must be in a tuple
            if(items.size() == 1) return false;
            // starred assignment target must be the last one (differ from cpython)
            if(starred_i != items.size()-1) return false;
            // a,*b = [1,2,3]
            // stack is [1,2,3] -> [1,[2,3]]
            ctx->emit(OP_UNPACK_EX, items.size()-1, line);
        }
        // do reverse emit
        for(int i=items.size()-1; i>=0; i--){
            bool ok = items[i]->emit_store(ctx);
            if(!ok) return false;
        }
        return true;
    }

    bool emit_del(CodeEmitContext* ctx) override{
        for(auto& e: items){
            bool ok = e->emit_del(ctx);
            if(!ok) return false;
        }
        return true;
    }
};

struct CompExpr: Expr{
    Expr_ expr;       // loop expr
    Expr_ vars;       // loop vars
    Expr_ iter;       // loop iter
    Expr_ cond;       // optional if condition

    virtual Opcode op0() = 0;
    virtual Opcode op1() = 0;

    void emit(CodeEmitContext* ctx){
        ctx->emit(op0(), 0, line);
        iter->emit(ctx);
        ctx->emit(OP_GET_ITER, BC_NOARG, BC_KEEPLINE);
        ctx->enter_block(FOR_LOOP);
        ctx->emit(OP_FOR_ITER, BC_NOARG, BC_KEEPLINE);
        bool ok = vars->emit_store(ctx);
        // this error occurs in `vars` instead of this line, but...nevermind
        if(!ok) FATAL_ERROR();  // TODO: raise a SyntaxError instead
        if(cond){
            cond->emit(ctx);
            int patch = ctx->emit(OP_POP_JUMP_IF_FALSE, BC_NOARG, BC_KEEPLINE);
            expr->emit(ctx);
            ctx->emit(op1(), BC_NOARG, BC_KEEPLINE);
            ctx->patch_jump(patch);
        }else{
            expr->emit(ctx);
            ctx->emit(op1(), BC_NOARG, BC_KEEPLINE);
        }
        ctx->emit(OP_LOOP_CONTINUE, BC_NOARG, BC_KEEPLINE);
        ctx->exit_block();
    }
};

struct ListCompExpr: CompExpr{
    Opcode op0() override { return OP_BUILD_LIST; }
    Opcode op1() override { return OP_LIST_APPEND; }
    std::string str() const override { return "ListComp()"; }
};

struct DictCompExpr: CompExpr{
    Opcode op0() override { return OP_BUILD_DICT; }
    Opcode op1() override { return OP_DICT_ADD; }
    std::string str() const override { return "DictComp()"; }
};

struct SetCompExpr: CompExpr{
    Opcode op0() override { return OP_BUILD_SET; }
    Opcode op1() override { return OP_SET_ADD; }
    std::string str() const override { return "SetComp()"; }
};

struct LambdaExpr: Expr{
    FuncDecl_ decl;
    std::string str() const override { return "Lambda()"; }

    LambdaExpr(FuncDecl_ decl): decl(decl) {}

    void emit(CodeEmitContext* ctx) override {
        int index = ctx->add_func_decl(decl);
        ctx->emit(OP_LOAD_FUNCTION, index, line);
    }
};

struct FStringExpr: Expr{
    Str src;
    FStringExpr(const Str& src): src(src) {}
    std::string str() const override {
        return fmt("f", src.escape());
    }

    void emit(CodeEmitContext* ctx) override {
        VM* vm = ctx->vm;
        static const std::regex pattern(R"(\{(.*?)\})");
        std::cregex_iterator begin(src.begin(), src.end(), pattern);
        std::cregex_iterator end;
        int size = 0;
        int i = 0;
        for(auto it = begin; it != end; it++) {
            std::cmatch m = *it;
            if (i < m.position()) {
                Str literal = src.substr(i, m.position() - i);
                ctx->emit(OP_LOAD_CONST, ctx->add_const(VAR(literal)), line);
                size++;
            }
            Str expr = m[1].str();
            int conon = expr.index(":");
            if(conon >= 0){
                ctx->emit(
                    OP_LOAD_NAME,
                    StrName(expr.substr(0, conon)).index,
                    line
                );
                Str spec = expr.substr(conon+1);
                ctx->emit(OP_FORMAT_STRING, ctx->add_const(VAR(spec)), line);
            }else{
                ctx->emit(OP_LOAD_NAME, StrName(expr).index, line);
            }
            size++;
            i = (int)(m.position() + m.length());
        }
        if (i < src.length()) {
            Str literal = src.substr(i, src.length() - i);
            ctx->emit(OP_LOAD_CONST, ctx->add_const(VAR(literal)), line);
            size++;
        }
        ctx->emit(OP_BUILD_STRING, size, line);
    }
};

struct SubscrExpr: Expr{
    Expr_ a;
    Expr_ b;
    std::string str() const override { return "Subscr()"; }

    void emit(CodeEmitContext* ctx) override{
        a->emit(ctx);
        b->emit(ctx);
        ctx->emit(OP_LOAD_SUBSCR, BC_NOARG, line);
    }

    bool emit_del(CodeEmitContext* ctx) override {
        a->emit(ctx);
        b->emit(ctx);
        ctx->emit(OP_DELETE_SUBSCR, BC_NOARG, line);
        return true;
    }

    bool emit_store(CodeEmitContext* ctx) override {
        a->emit(ctx);
        b->emit(ctx);
        ctx->emit(OP_STORE_SUBSCR, BC_NOARG, line);
        return true;
    }
};

struct AttribExpr: Expr{
    Expr_ a;
    Str b;
    AttribExpr(Expr_ a, const Str& b): a(std::move(a)), b(b) {}
    AttribExpr(Expr_ a, Str&& b): a(std::move(a)), b(std::move(b)) {}
    std::string str() const override { return "Attrib()"; }

    void emit(CodeEmitContext* ctx) override{
        a->emit(ctx);
        int index = StrName(b).index;
        ctx->emit(OP_LOAD_ATTR, index, line);
    }

    bool emit_del(CodeEmitContext* ctx) override {
        a->emit(ctx);
        int index = StrName(b).index;
        ctx->emit(OP_DELETE_ATTR, index, line);
        return true;
    }

    bool emit_store(CodeEmitContext* ctx) override {
        a->emit(ctx);
        int index = StrName(b).index;
        ctx->emit(OP_STORE_ATTR, index, line);
        return true;
    }

    void emit_method(CodeEmitContext* ctx) {
        a->emit(ctx);
        int index = StrName(b).index;
        ctx->emit(OP_LOAD_METHOD, index, line);
    }

    bool is_attrib() const override { return true; }
};

struct CallExpr: Expr{
    Expr_ callable;
    std::vector<Expr_> args;
    std::vector<std::pair<Str, Expr_>> kwargs;
    std::string str() const override { return "Call()"; }

    bool need_unpack() const {
        for(auto& item: args) if(item->is_starred()) return true;
        return false;
    }

    void emit(CodeEmitContext* ctx) override {
        VM* vm = ctx->vm;
        if(need_unpack()) ctx->emit(OP_BEGIN_CALL, BC_NOARG, line);
        // if callable is a AttrExpr, we should try to use `fast_call` instead of use `boundmethod` proxy
        if(callable->is_attrib()){
            auto p = static_cast<AttribExpr*>(callable.get());
            p->emit_method(ctx);
        }else{
            callable->emit(ctx);
            ctx->emit(OP_LOAD_NULL, BC_NOARG, BC_KEEPLINE);
        }
        // emit args
        for(auto& item: args) item->emit(ctx);
        // emit kwargs
        for(auto& item: kwargs){
            int index = StrName::get(item.first.sv()).index;
            ctx->emit(OP_LOAD_CONST, ctx->add_const(VAR(index)), line);
            item.second->emit(ctx);
        }
        int KWARGC = (int)kwargs.size();
        int ARGC = (int)args.size();
        if(need_unpack()) ARGC = 0xFFFF;
        ctx->emit(OP_CALL, (KWARGC<<16)|ARGC, line);
    }
};

struct BinaryExpr: Expr{
    TokenIndex op;
    Expr_ lhs;
    Expr_ rhs;
    std::string str() const override { return TK_STR(op); }

    void emit(CodeEmitContext* ctx) override {
        lhs->emit(ctx);
        rhs->emit(ctx);
        switch (op) {
            case TK("+"):   ctx->emit(OP_BINARY_ADD, BC_NOARG, line);  break;
            case TK("-"):   ctx->emit(OP_BINARY_SUB, BC_NOARG, line);  break;
            case TK("*"):   ctx->emit(OP_BINARY_MUL, BC_NOARG, line);  break;
            case TK("/"):   ctx->emit(OP_BINARY_OP, 3, line);  break;
            case TK("//"):  ctx->emit(OP_BINARY_FLOORDIV, BC_NOARG, line);  break;
            case TK("%"):   ctx->emit(OP_BINARY_MOD, BC_NOARG, line);  break;
            case TK("**"):  ctx->emit(OP_BINARY_OP, 6, line);  break;

            case TK("<"):   ctx->emit(OP_COMPARE_LT, BC_NOARG, line);  break;
            case TK("<="):  ctx->emit(OP_COMPARE_LE, BC_NOARG, line);  break;
            case TK("=="):  ctx->emit(OP_COMPARE_EQ, BC_NOARG, line);  break;
            case TK("!="):  ctx->emit(OP_COMPARE_NE, BC_NOARG, line);  break;
            case TK(">"):   ctx->emit(OP_COMPARE_GT, BC_NOARG, line);  break;
            case TK(">="):  ctx->emit(OP_COMPARE_GE, BC_NOARG, line);  break;
            case TK("in"):      ctx->emit(OP_CONTAINS_OP, 0, line);   break;
            case TK("not in"):  ctx->emit(OP_CONTAINS_OP, 1, line);   break;
            case TK("is"):      ctx->emit(OP_IS_OP, 0, line);         break;
            case TK("is not"):  ctx->emit(OP_IS_OP, 1, line);         break;

            case TK("<<"):  ctx->emit(OP_BITWISE_LSHIFT, BC_NOARG, line);  break;
            case TK(">>"):  ctx->emit(OP_BITWISE_RSHIFT, BC_NOARG, line);  break;
            case TK("&"):   ctx->emit(OP_BITWISE_AND, BC_NOARG, line);  break;
            case TK("|"):   ctx->emit(OP_BITWISE_OR, BC_NOARG, line);  break;
            case TK("^"):   ctx->emit(OP_BITWISE_XOR, BC_NOARG, line);  break;
            default: FATAL_ERROR();
        }
    }
};


struct TernaryExpr: Expr{
    Expr_ cond;
    Expr_ true_expr;
    Expr_ false_expr;
    std::string str() const override { return "Ternary()"; }

    void emit(CodeEmitContext* ctx) override {
        cond->emit(ctx);
        int patch = ctx->emit(OP_POP_JUMP_IF_FALSE, BC_NOARG, cond->line);
        true_expr->emit(ctx);
        int patch_2 = ctx->emit(OP_JUMP_ABSOLUTE, BC_NOARG, true_expr->line);
        ctx->patch_jump(patch);
        false_expr->emit(ctx);
        ctx->patch_jump(patch_2);
    }
};


} // namespace pkpy


namespace pkpy{

class Compiler;
typedef void (Compiler::*PrattCallback)();

struct PrattRule{
    PrattCallback prefix;
    PrattCallback infix;
    Precedence precedence;
};

class Compiler {
    inline static PrattRule rules[kTokenCount];
    std::unique_ptr<Lexer> lexer;
    stack<CodeEmitContext> contexts;
    VM* vm;
    bool unknown_global_scope;     // for eval/exec() call
    bool used;
    // for parsing token stream
    int i = 0;
    std::vector<Token> tokens;

    const Token& prev() const{ return tokens.at(i-1); }
    const Token& curr() const{ return tokens.at(i); }
    const Token& next() const{ return tokens.at(i+1); }
    const Token& err() const{
        if(i >= tokens.size()) return prev();
        return curr();
    }
    void advance(int delta=1) { i += delta; }

    CodeEmitContext* ctx() { return &contexts.top(); }
    CompileMode mode() const{ return lexer->src->mode; }
    NameScope name_scope() const {
        auto s = contexts.size()>1 ? NAME_LOCAL : NAME_GLOBAL;
        if(unknown_global_scope && s == NAME_GLOBAL) s = NAME_GLOBAL_UNKNOWN;
        return s;
    }

    CodeObject_ push_global_context(){
        CodeObject_ co = make_sp<CodeObject>(lexer->src, lexer->src->filename);
        contexts.push(CodeEmitContext(vm, co, contexts.size()));
        return co;
    }

    FuncDecl_ push_f_context(Str name){
        FuncDecl_ decl = make_sp<FuncDecl>();
        decl->code = make_sp<CodeObject>(lexer->src, name);
        decl->nested = name_scope() == NAME_LOCAL;
        contexts.push(CodeEmitContext(vm, decl->code, contexts.size()));
        return decl;
    }

    void pop_context(){
        if(!ctx()->s_expr.empty()){
            throw std::runtime_error("!ctx()->s_expr.empty()\n" + ctx()->_log_s_expr());
        }
        // if the last op does not return, add a default return None
        if(ctx()->co->codes.empty() || ctx()->co->codes.back().op != OP_RETURN_VALUE){
            ctx()->emit(OP_LOAD_NONE, BC_NOARG, BC_KEEPLINE);
            ctx()->emit(OP_RETURN_VALUE, BC_NOARG, BC_KEEPLINE);
        }
        ctx()->co->optimize(vm);
        if(ctx()->co->varnames.size() > PK_MAX_CO_VARNAMES){
            SyntaxError("maximum number of local variables exceeded");
        }
        contexts.pop();
    }

    static void init_pratt_rules(){
        if(rules[TK(".")].precedence != PREC_NONE) return;
// http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/
#define METHOD(name) &Compiler::name
#define NO_INFIX nullptr, PREC_NONE
        for(TokenIndex i=0; i<kTokenCount; i++) rules[i] = { nullptr, NO_INFIX };
        rules[TK(".")] =        { nullptr,               METHOD(exprAttrib),         PREC_ATTRIB };
        rules[TK("(")] =        { METHOD(exprGroup),     METHOD(exprCall),           PREC_CALL };
        rules[TK("[")] =        { METHOD(exprList),      METHOD(exprSubscr),         PREC_SUBSCRIPT };
        rules[TK("{")] =        { METHOD(exprMap),       NO_INFIX };
        rules[TK("%")] =        { nullptr,               METHOD(exprBinaryOp),       PREC_FACTOR };
        rules[TK("+")] =        { nullptr,               METHOD(exprBinaryOp),       PREC_TERM };
        rules[TK("-")] =        { METHOD(exprUnaryOp),   METHOD(exprBinaryOp),       PREC_TERM };
        rules[TK("*")] =        { METHOD(exprUnaryOp),   METHOD(exprBinaryOp),       PREC_FACTOR };
        rules[TK("/")] =        { nullptr,               METHOD(exprBinaryOp),       PREC_FACTOR };
        rules[TK("//")] =       { nullptr,               METHOD(exprBinaryOp),       PREC_FACTOR };
        rules[TK("**")] =       { nullptr,               METHOD(exprBinaryOp),       PREC_EXPONENT };
        rules[TK(">")] =        { nullptr,               METHOD(exprBinaryOp),       PREC_COMPARISION };
        rules[TK("<")] =        { nullptr,               METHOD(exprBinaryOp),       PREC_COMPARISION };
        rules[TK("==")] =       { nullptr,               METHOD(exprBinaryOp),       PREC_EQUALITY };
        rules[TK("!=")] =       { nullptr,               METHOD(exprBinaryOp),       PREC_EQUALITY };
        rules[TK(">=")] =       { nullptr,               METHOD(exprBinaryOp),       PREC_COMPARISION };
        rules[TK("<=")] =       { nullptr,               METHOD(exprBinaryOp),       PREC_COMPARISION };
        rules[TK("in")] =       { nullptr,               METHOD(exprBinaryOp),       PREC_TEST };
        rules[TK("is")] =       { nullptr,               METHOD(exprBinaryOp),       PREC_TEST };
        rules[TK("<<")] =       { nullptr,               METHOD(exprBinaryOp),       PREC_BITWISE_SHIFT };
        rules[TK(">>")] =       { nullptr,               METHOD(exprBinaryOp),       PREC_BITWISE_SHIFT };
        rules[TK("&")] =        { nullptr,               METHOD(exprBinaryOp),       PREC_BITWISE_AND };
        rules[TK("|")] =        { nullptr,               METHOD(exprBinaryOp),       PREC_BITWISE_OR };
        rules[TK("^")] =        { nullptr,               METHOD(exprBinaryOp),       PREC_BITWISE_XOR };
        rules[TK("if")] =       { nullptr,               METHOD(exprTernary),        PREC_TERNARY };
        rules[TK(",")] =        { nullptr,               METHOD(exprTuple),          PREC_TUPLE };
        rules[TK("not in")] =   { nullptr,               METHOD(exprBinaryOp),       PREC_TEST };
        rules[TK("is not")] =   { nullptr,               METHOD(exprBinaryOp),       PREC_TEST };
        rules[TK("and") ] =     { nullptr,               METHOD(exprAnd),            PREC_LOGICAL_AND };
        rules[TK("or")] =       { nullptr,               METHOD(exprOr),             PREC_LOGICAL_OR };
        rules[TK("not")] =      { METHOD(exprNot),       nullptr,                    PREC_LOGICAL_NOT };
        rules[TK("True")] =     { METHOD(exprLiteral0),  NO_INFIX };
        rules[TK("False")] =    { METHOD(exprLiteral0),  NO_INFIX };
        rules[TK("None")] =     { METHOD(exprLiteral0),  NO_INFIX };
        rules[TK("...")] =      { METHOD(exprLiteral0),  NO_INFIX };
        rules[TK("lambda")] =   { METHOD(exprLambda),    NO_INFIX };
        rules[TK("@id")] =      { METHOD(exprName),      NO_INFIX };
        rules[TK("@num")] =     { METHOD(exprLiteral),   NO_INFIX };
        rules[TK("@str")] =     { METHOD(exprLiteral),   NO_INFIX };
        rules[TK("@fstr")] =    { METHOD(exprFString),   NO_INFIX };
#undef METHOD
#undef NO_INFIX
    }

    bool match(TokenIndex expected) {
        if (curr().type != expected) return false;
        advance();
        return true;
    }

    void consume(TokenIndex expected) {
        if (!match(expected)){
            SyntaxError(
                fmt("expected '", TK_STR(expected), "', but got '", TK_STR(curr().type), "'")
            );
        }
    }

    bool match_newlines_repl(){
        return match_newlines(mode()==REPL_MODE);
    }

    bool match_newlines(bool repl_throw=false) {
        bool consumed = false;
        if (curr().type == TK("@eol")) {
            while (curr().type == TK("@eol")) advance();
            consumed = true;
        }
        if (repl_throw && curr().type == TK("@eof")){
            throw NeedMoreLines(ctx()->is_compiling_class);
        }
        return consumed;
    }

    bool match_end_stmt() {
        if (match(TK(";"))) { match_newlines(); return true; }
        if (match_newlines() || curr().type == TK("@eof")) return true;
        if (curr().type == TK("@dedent")) return true;
        return false;
    }

    void consume_end_stmt() {
        if (!match_end_stmt()) SyntaxError("expected statement end");
    }

    /*************************************************/

    void EXPR(bool push_stack=true) {
        parse_expression(PREC_TUPLE+1, push_stack);
    }

    void EXPR_TUPLE(bool push_stack=true) {
        parse_expression(PREC_TUPLE, push_stack);
    }

    // special case for `for loop` and `comp`
    Expr_ EXPR_VARS(){
        std::vector<Expr_> items;
        do {
            consume(TK("@id"));
            items.push_back(make_expr<NameExpr>(prev().str(), name_scope()));
        } while(match(TK(",")));
        if(items.size()==1) return std::move(items[0]);
        return make_expr<TupleExpr>(std::move(items));
    }

    template <typename T, typename... Args>
    std::unique_ptr<T> make_expr(Args&&... args) {
        std::unique_ptr<T> expr = std::make_unique<T>(std::forward<Args>(args)...);
        expr->line = prev().line;
        return expr;
    }

    
    void exprLiteral(){
        ctx()->s_expr.push(make_expr<LiteralExpr>(prev().value));
    }

    
    void exprFString(){
        ctx()->s_expr.push(make_expr<FStringExpr>(std::get<Str>(prev().value)));
    }

    
    void exprLambda(){
        FuncDecl_ decl = push_f_context("<lambda>");
        auto e = make_expr<LambdaExpr>(decl);
        if(!match(TK(":"))){
            _compile_f_args(e->decl, false);
            consume(TK(":"));
        }
        // https://github.com/blueloveTH/pocketpy/issues/37
        parse_expression(PREC_LAMBDA + 1, false);
        ctx()->emit(OP_RETURN_VALUE, BC_NOARG, BC_KEEPLINE);
        pop_context();
        ctx()->s_expr.push(std::move(e));
    }

    
    void exprTuple(){
        std::vector<Expr_> items;
        items.push_back(ctx()->s_expr.popx());
        do {
            EXPR();         // NOTE: "1," will fail, "1,2" will be ok
            items.push_back(ctx()->s_expr.popx());
        } while(match(TK(",")));
        ctx()->s_expr.push(make_expr<TupleExpr>(
            std::move(items)
        ));
    }

    
    void exprOr(){
        auto e = make_expr<OrExpr>();
        e->lhs = ctx()->s_expr.popx();
        parse_expression(PREC_LOGICAL_OR + 1);
        e->rhs = ctx()->s_expr.popx();
        ctx()->s_expr.push(std::move(e));
    }

    
    void exprAnd(){
        auto e = make_expr<AndExpr>();
        e->lhs = ctx()->s_expr.popx();
        parse_expression(PREC_LOGICAL_AND + 1);
        e->rhs = ctx()->s_expr.popx();
        ctx()->s_expr.push(std::move(e));
    }

    
    void exprTernary(){
        auto e = make_expr<TernaryExpr>();
        e->true_expr = ctx()->s_expr.popx();
        // cond
        parse_expression(PREC_TERNARY + 1);
        e->cond = ctx()->s_expr.popx();
        consume(TK("else"));
        // if false
        parse_expression(PREC_TERNARY + 1);
        e->false_expr = ctx()->s_expr.popx();
        ctx()->s_expr.push(std::move(e));
    }

    
    void exprBinaryOp(){
        auto e = make_expr<BinaryExpr>();
        e->op = prev().type;
        e->lhs = ctx()->s_expr.popx();
        parse_expression(rules[e->op].precedence + 1);
        e->rhs = ctx()->s_expr.popx();
        ctx()->s_expr.push(std::move(e));
    }

    
    void exprNot() {
        parse_expression(PREC_LOGICAL_NOT + 1);
        ctx()->s_expr.push(make_expr<NotExpr>(ctx()->s_expr.popx()));
    }

    
    void exprUnaryOp(){
        TokenIndex op = prev().type;
        parse_expression(PREC_UNARY + 1);
        switch(op){
            case TK("-"):
                ctx()->s_expr.push(make_expr<NegatedExpr>(ctx()->s_expr.popx()));
                break;
            case TK("*"):
                ctx()->s_expr.push(make_expr<StarredExpr>(ctx()->s_expr.popx()));
                break;
            default: FATAL_ERROR();
        }
    }

    
    void exprGroup(){
        match_newlines_repl();
        EXPR_TUPLE();   // () is just for change precedence
        match_newlines_repl();
        consume(TK(")"));
    }

    
    template<typename T>
    void _consume_comp(Expr_ expr){
        static_assert(std::is_base_of<CompExpr, T>::value);
        std::unique_ptr<CompExpr> ce = make_expr<T>();
        ce->expr = std::move(expr);
        ce->vars = EXPR_VARS();
        consume(TK("in"));
        parse_expression(PREC_TERNARY + 1);
        ce->iter = ctx()->s_expr.popx();
        match_newlines_repl();
        if(match(TK("if"))){
            parse_expression(PREC_TERNARY + 1);
            ce->cond = ctx()->s_expr.popx();
        }
        ctx()->s_expr.push(std::move(ce));
        match_newlines_repl();
    }

    
    void exprList() {
        int line = prev().line;
        std::vector<Expr_> items;
        do {
            match_newlines_repl();
            if (curr().type == TK("]")) break;
            EXPR();
            items.push_back(ctx()->s_expr.popx());
            match_newlines_repl();
            if(items.size()==1 && match(TK("for"))){
                _consume_comp<ListCompExpr>(std::move(items[0]));
                consume(TK("]"));
                return;
            }
            match_newlines_repl();
        } while (match(TK(",")));
        consume(TK("]"));
        auto e = make_expr<ListExpr>(std::move(items));
        e->line = line;     // override line
        ctx()->s_expr.push(std::move(e));
    }

    
    void exprMap() {
        bool parsing_dict = false;  // {...} may be dict or set
        std::vector<Expr_> items;
        do {
            match_newlines_repl();
            if (curr().type == TK("}")) break;
            EXPR();
            if(curr().type == TK(":")) parsing_dict = true;
            if(parsing_dict){
                consume(TK(":"));
                EXPR();
                auto dict_item = make_expr<DictItemExpr>();
                dict_item->key = ctx()->s_expr.popx();
                dict_item->value = ctx()->s_expr.popx();
                items.push_back(std::move(dict_item));
            }else{
                items.push_back(ctx()->s_expr.popx());
            }
            match_newlines_repl();
            if(items.size()==1 && match(TK("for"))){
                if(parsing_dict) _consume_comp<DictCompExpr>(std::move(items[0]));
                else _consume_comp<SetCompExpr>(std::move(items[0]));
                consume(TK("}"));
                return;
            }
            match_newlines_repl();
        } while (match(TK(",")));
        consume(TK("}"));
        if(items.size()==0 || parsing_dict){
            auto e = make_expr<DictExpr>(std::move(items));
            ctx()->s_expr.push(std::move(e));
        }else{
            auto e = make_expr<SetExpr>(std::move(items));
            ctx()->s_expr.push(std::move(e));
        }
    }

    
    void exprCall() {
        auto e = make_expr<CallExpr>();
        e->callable = ctx()->s_expr.popx();
        do {
            match_newlines_repl();
            if (curr().type==TK(")")) break;
            if(curr().type==TK("@id") && next().type==TK("=")) {
                consume(TK("@id"));
                Str key = prev().str();
                consume(TK("="));
                EXPR();
                e->kwargs.push_back({key, ctx()->s_expr.popx()});
            } else{
                if(!e->kwargs.empty()) SyntaxError("positional argument follows keyword argument");
                EXPR();
                e->args.push_back(ctx()->s_expr.popx());
            }
            match_newlines_repl();
        } while (match(TK(",")));
        consume(TK(")"));
        if(e->args.size() > 32767) SyntaxError("too many positional arguments");
        if(e->kwargs.size() > 32767) SyntaxError("too many keyword arguments");
        ctx()->s_expr.push(std::move(e));
    }

    
    void exprName(){
        Str name = prev().str();
        NameScope scope = name_scope();
        if(ctx()->co->global_names.count(name)){
            scope = NAME_GLOBAL;
        }
        ctx()->s_expr.push(make_expr<NameExpr>(name, scope));
    }

    
    void exprAttrib() {
        consume(TK("@id"));
        ctx()->s_expr.push(
            make_expr<AttribExpr>(ctx()->s_expr.popx(), prev().str())
        );
    }

    
    void exprSubscr() {
        auto e = make_expr<SubscrExpr>();
        e->a = ctx()->s_expr.popx();
        auto slice = make_expr<SliceExpr>();
        bool is_slice = false;
        // a[<0> <state:1> : state<3> : state<5>]
        int state = 0;
        do{
            switch(state){
                case 0:
                    if(match(TK(":"))){
                        is_slice=true;
                        state=2;
                        break;
                    }
                    if(match(TK("]"))) SyntaxError();
                    EXPR_TUPLE();
                    slice->start = ctx()->s_expr.popx();
                    state=1;
                    break;
                case 1:
                    if(match(TK(":"))){
                        is_slice=true;
                        state=2;
                        break;
                    }
                    if(match(TK("]"))) goto __SUBSCR_END;
                    SyntaxError("expected ':' or ']'");
                    break;
                case 2:
                    if(match(TK(":"))){
                        state=4;
                        break;
                    }
                    if(match(TK("]"))) goto __SUBSCR_END;
                    EXPR_TUPLE();
                    slice->stop = ctx()->s_expr.popx();
                    state=3;
                    break;
                case 3:
                    if(match(TK(":"))){
                        state=4;
                        break;
                    }
                    if(match(TK("]"))) goto __SUBSCR_END;
                    SyntaxError("expected ':' or ']'");
                    break;
                case 4:
                    if(match(TK("]"))) goto __SUBSCR_END;
                    EXPR_TUPLE();
                    slice->step = ctx()->s_expr.popx();
                    state=5;
                    break;
                case 5: consume(TK("]")); goto __SUBSCR_END;
            }
        }while(true);
__SUBSCR_END:
        if(is_slice){
            e->b = std::move(slice);
        }else{
            if(state != 1) FATAL_ERROR();
            e->b = std::move(slice->start);
        }
        ctx()->s_expr.push(std::move(e));
    }

    
    void exprLiteral0() {
        ctx()->s_expr.push(make_expr<Literal0Expr>(prev().type));
    }

    void compile_block_body() {
        consume(TK(":"));
        if(curr().type!=TK("@eol") && curr().type!=TK("@eof")){
            compile_stmt();     // inline block
            return;
        }
        if(!match_newlines(mode()==REPL_MODE)){
            SyntaxError("expected a new line after ':'");
        }
        consume(TK("@indent"));
        while (curr().type != TK("@dedent")) {
            match_newlines();
            compile_stmt();
            match_newlines();
        }
        consume(TK("@dedent"));
    }

    Str _compile_import() {
        if(name_scope() != NAME_GLOBAL) SyntaxError("import statement should be used in global scope");
        consume(TK("@id"));
        Str name = prev().str();
        ctx()->emit(OP_IMPORT_NAME, StrName(name).index, prev().line);
        return name;
    }

    // import a as b
    void compile_normal_import() {
        do {
            Str name = _compile_import();
            if (match(TK("as"))) {
                consume(TK("@id"));
                name = prev().str();
            }
            ctx()->emit(OP_STORE_GLOBAL, StrName(name).index, prev().line);
        } while (match(TK(",")));
        consume_end_stmt();
    }

    // from a import b as c, d as e
    void compile_from_import() {
        _compile_import();
        consume(TK("import"));
        if (match(TK("*"))) {
            ctx()->emit(OP_IMPORT_STAR, BC_NOARG, prev().line);
            consume_end_stmt();
            return;
        }
        do {
            ctx()->emit(OP_DUP_TOP, BC_NOARG, BC_KEEPLINE);
            consume(TK("@id"));
            Str name = prev().str();
            ctx()->emit(OP_LOAD_ATTR, StrName(name).index, prev().line);
            if (match(TK("as"))) {
                consume(TK("@id"));
                name = prev().str();
            }
            ctx()->emit(OP_STORE_GLOBAL, StrName(name).index, prev().line);
        } while (match(TK(",")));
        ctx()->emit(OP_POP_TOP, BC_NOARG, BC_KEEPLINE);
        consume_end_stmt();
    }

    void parse_expression(int precedence, bool push_stack=true) {
        advance();
        PrattCallback prefix = rules[prev().type].prefix;
        if (prefix == nullptr) SyntaxError(Str("expected an expression, but got ") + TK_STR(prev().type));
        (this->*prefix)();
        while (rules[curr().type].precedence >= precedence) {
            TokenIndex op = curr().type;
            advance();
            PrattCallback infix = rules[op].infix;
            if(infix == nullptr) throw std::runtime_error("(infix == nullptr) is true");
            (this->*infix)();
        }
        if(!push_stack) ctx()->emit_expr();
    }

    
    void compile_if_stmt() {
        EXPR(false);   // condition
        int patch = ctx()->emit(OP_POP_JUMP_IF_FALSE, BC_NOARG, prev().line);
        compile_block_body();
        if (match(TK("elif"))) {
            int exit_patch = ctx()->emit(OP_JUMP_ABSOLUTE, BC_NOARG, prev().line);
            ctx()->patch_jump(patch);
            compile_if_stmt();
            ctx()->patch_jump(exit_patch);
        } else if (match(TK("else"))) {
            int exit_patch = ctx()->emit(OP_JUMP_ABSOLUTE, BC_NOARG, prev().line);
            ctx()->patch_jump(patch);
            compile_block_body();
            ctx()->patch_jump(exit_patch);
        } else {
            ctx()->patch_jump(patch);
        }
    }

    
    void compile_while_loop() {
        ctx()->enter_block(WHILE_LOOP);
        EXPR(false);   // condition
        int patch = ctx()->emit(OP_POP_JUMP_IF_FALSE, BC_NOARG, prev().line);
        compile_block_body();
        ctx()->emit(OP_LOOP_CONTINUE, BC_NOARG, BC_KEEPLINE);
        ctx()->patch_jump(patch);
        ctx()->exit_block();
    }

    
    void compile_for_loop() {
        Expr_ vars = EXPR_VARS();
        consume(TK("in"));
        EXPR_TUPLE(false);
        ctx()->emit(OP_GET_ITER, BC_NOARG, BC_KEEPLINE);
        ctx()->enter_block(FOR_LOOP);
        ctx()->emit(OP_FOR_ITER, BC_NOARG, BC_KEEPLINE);
        bool ok = vars->emit_store(ctx());
        if(!ok) SyntaxError();  // this error occurs in `vars` instead of this line, but...nevermind
        compile_block_body();
        ctx()->emit(OP_LOOP_CONTINUE, BC_NOARG, BC_KEEPLINE);
        ctx()->exit_block();
    }

    void compile_try_except() {
        ctx()->enter_block(TRY_EXCEPT);
        compile_block_body();
        std::vector<int> patches = {
            ctx()->emit(OP_JUMP_ABSOLUTE, BC_NOARG, BC_KEEPLINE)
        };
        ctx()->exit_block();
        do {
            consume(TK("except"));
            if(match(TK("@id"))){
                ctx()->emit(OP_EXCEPTION_MATCH, StrName(prev().str()).index, prev().line);
            }else{
                ctx()->emit(OP_LOAD_TRUE, BC_NOARG, BC_KEEPLINE);
            }
            int patch = ctx()->emit(OP_POP_JUMP_IF_FALSE, BC_NOARG, BC_KEEPLINE);
            // pop the exception on match
            ctx()->emit(OP_POP_TOP, BC_NOARG, BC_KEEPLINE);
            compile_block_body();
            patches.push_back(ctx()->emit(OP_JUMP_ABSOLUTE, BC_NOARG, BC_KEEPLINE));
            ctx()->patch_jump(patch);
        }while(curr().type == TK("except"));
        // no match, re-raise
        ctx()->emit(OP_RE_RAISE, BC_NOARG, BC_KEEPLINE);
        for (int patch : patches) ctx()->patch_jump(patch);
    }

    void compile_decorated(){
        std::vector<Expr_> decorators;
        do{
            EXPR();
            decorators.push_back(ctx()->s_expr.popx());
            if(!match_newlines_repl()) SyntaxError();
        }while(match(TK("@")));
        consume(TK("def"));
        compile_function(decorators);
    }

    bool try_compile_assignment(){
        Expr* lhs_p = ctx()->s_expr.top().get();
        bool inplace;
        switch (curr().type) {
            case TK("+="): case TK("-="): case TK("*="): case TK("/="): case TK("//="): case TK("%="):
            case TK("<<="): case TK(">>="): case TK("&="): case TK("|="): case TK("^="): {
                if(ctx()->is_compiling_class) SyntaxError();
                inplace = true;
                advance();
                auto e = make_expr<BinaryExpr>();
                e->op = prev().type - 1; // -1 to remove =
                e->lhs = ctx()->s_expr.popx();
                EXPR_TUPLE();
                e->rhs = ctx()->s_expr.popx();
                ctx()->s_expr.push(std::move(e));
            } break;
            case TK("="):
                inplace = false;
                advance();
                EXPR_TUPLE();
                break;
            default: return false;
        }
        // std::cout << ctx()->_log_s_expr() << std::endl;
        Expr_ rhs = ctx()->s_expr.popx();

        if(lhs_p->is_starred() || rhs->is_starred()){
            SyntaxError("can't use starred expression here");
        }

        rhs->emit(ctx());
        bool ok = lhs_p->emit_store(ctx());
        if(!ok) SyntaxError();
        if(!inplace) ctx()->s_expr.pop();
        return true;
    }

    void compile_stmt() {
        advance();
        int kw_line = prev().line;  // backup line number
        switch(prev().type){
            case TK("break"):
                if (!ctx()->is_curr_block_loop()) SyntaxError("'break' outside loop");
                ctx()->emit(OP_LOOP_BREAK, BC_NOARG, kw_line);
                consume_end_stmt();
                break;
            case TK("continue"):
                if (!ctx()->is_curr_block_loop()) SyntaxError("'continue' not properly in loop");
                ctx()->emit(OP_LOOP_CONTINUE, BC_NOARG, kw_line);
                consume_end_stmt();
                break;
            case TK("yield"): 
                if (contexts.size() <= 1) SyntaxError("'yield' outside function");
                EXPR_TUPLE(false);
                // if yield present, mark the function as generator
                ctx()->co->is_generator = true;
                ctx()->emit(OP_YIELD_VALUE, BC_NOARG, kw_line);
                consume_end_stmt();
                break;
            case TK("yield from"):
                if (contexts.size() <= 1) SyntaxError("'yield from' outside function");
                EXPR_TUPLE(false);
                // if yield from present, mark the function as generator
                ctx()->co->is_generator = true;
                ctx()->emit(OP_GET_ITER, BC_NOARG, kw_line);
                ctx()->enter_block(FOR_LOOP);
                ctx()->emit(OP_FOR_ITER, BC_NOARG, BC_KEEPLINE);
                ctx()->emit(OP_YIELD_VALUE, BC_NOARG, BC_KEEPLINE);
                ctx()->emit(OP_LOOP_CONTINUE, BC_NOARG, BC_KEEPLINE);
                ctx()->exit_block();
                consume_end_stmt();
                break;
            case TK("return"):
                if (contexts.size() <= 1) SyntaxError("'return' outside function");
                if(match_end_stmt()){
                    ctx()->emit(OP_LOAD_NONE, BC_NOARG, kw_line);
                }else{
                    EXPR_TUPLE(false);
                    consume_end_stmt();
                }
                ctx()->emit(OP_RETURN_VALUE, BC_NOARG, kw_line);
                break;
            /*************************************************/
            case TK("if"): compile_if_stmt(); break;
            case TK("while"): compile_while_loop(); break;
            case TK("for"): compile_for_loop(); break;
            case TK("import"): compile_normal_import(); break;
            case TK("from"): compile_from_import(); break;
            case TK("def"): compile_function(); break;
            case TK("@"): compile_decorated(); break;
            case TK("try"): compile_try_except(); break;
            case TK("pass"): consume_end_stmt(); break;
            /*************************************************/
            case TK("assert"):
                EXPR_TUPLE(false);
                ctx()->emit(OP_ASSERT, BC_NOARG, kw_line);
                consume_end_stmt();
                break;
            case TK("global"):
                do {
                    consume(TK("@id"));
                    ctx()->co->global_names.insert(prev().str());
                } while (match(TK(",")));
                consume_end_stmt();
                break;
            case TK("raise"): {
                consume(TK("@id"));
                int dummy_t = StrName(prev().str()).index;
                if(match(TK("(")) && !match(TK(")"))){
                    EXPR(false); consume(TK(")"));
                }else{
                    ctx()->emit(OP_LOAD_NONE, BC_NOARG, BC_KEEPLINE);
                }
                ctx()->emit(OP_RAISE, dummy_t, kw_line);
                consume_end_stmt();
            } break;
            case TK("del"): {
                EXPR_TUPLE();
                Expr_ e = ctx()->s_expr.popx();
                bool ok = e->emit_del(ctx());
                if(!ok) SyntaxError();
                consume_end_stmt();
            } break;
            case TK("with"): {
                EXPR(false);
                consume(TK("as"));
                consume(TK("@id"));
                StrName name(prev().str());
                ctx()->emit(OP_STORE_NAME, name.index, prev().line);
                ctx()->emit(OP_LOAD_NAME, name.index, prev().line);
                ctx()->emit(OP_WITH_ENTER, BC_NOARG, prev().line);
                compile_block_body();
                ctx()->emit(OP_LOAD_NAME, name.index, prev().line);
                ctx()->emit(OP_WITH_EXIT, BC_NOARG, prev().line);
            } break;
            /*************************************************/
            // TODO: refactor goto/label use special $ syntax
            case TK("label"): {
                if(mode()!=EXEC_MODE) SyntaxError("'label' is only available in EXEC_MODE");
                consume(TK(".")); consume(TK("@id"));
                bool ok = ctx()->add_label(prev().str());
                if(!ok) SyntaxError("label " + prev().str().escape() + " already exists");
                consume_end_stmt();
            } break;
            case TK("goto"):
                if(mode()!=EXEC_MODE) SyntaxError("'goto' is only available in EXEC_MODE");
                consume(TK(".")); consume(TK("@id"));
                ctx()->emit(OP_GOTO, StrName(prev().str()).index, prev().line);
                consume_end_stmt();
                break;
            /*************************************************/
            // handle dangling expression or assignment
            default: {
                advance(-1);    // do revert since we have pre-called advance() at the beginning
                EXPR_TUPLE();
                if(!try_compile_assignment()){
                    ctx()->emit_expr();
                    if(mode()==REPL_MODE && name_scope()==NAME_GLOBAL){
                        ctx()->emit(OP_PRINT_EXPR, BC_NOARG, BC_KEEPLINE);
                    }else{
                        ctx()->emit(OP_POP_TOP, BC_NOARG, BC_KEEPLINE);
                    }
                }
                consume_end_stmt();
            }
        }
    }

    
    void compile_class(){
        consume(TK("@id"));
        int namei = StrName(prev().str()).index;
        int super_namei = -1;
        if(match(TK("(")) && match(TK("@id"))){
            super_namei = StrName(prev().str()).index;
            consume(TK(")"));
        }
        if(super_namei == -1) ctx()->emit(OP_LOAD_NONE, BC_NOARG, prev().line);
        else ctx()->emit(OP_LOAD_GLOBAL, super_namei, prev().line);
        ctx()->emit(OP_BEGIN_CLASS, namei, BC_KEEPLINE);
        ctx()->is_compiling_class = true;
        compile_block_body();
        ctx()->is_compiling_class = false;
        ctx()->emit(OP_END_CLASS, BC_NOARG, BC_KEEPLINE);
    }

    void _compile_f_args(FuncDecl_ decl, bool enable_type_hints){
        int state = 0;      // 0 for args, 1 for *args, 2 for k=v, 3 for **kwargs
        do {
            if(state == 3) SyntaxError("**kwargs should be the last argument");
            match_newlines();
            if(match(TK("*"))){
                if(state < 1) state = 1;
                else SyntaxError("*args should be placed before **kwargs");
            }
            else if(match(TK("**"))){
                state = 3;
            }
            consume(TK("@id"));
            StrName name = prev().str();

            // check duplicate argument name
            for(int i: decl->args){
                if(decl->code->varnames[i] == name) {
                    SyntaxError("duplicate argument name");
                }
            }
            if(decl->starred_arg!=-1 && decl->code->varnames[decl->starred_arg] == name){
                SyntaxError("duplicate argument name");
            }
            for(auto& kv: decl->kwargs){
                if(decl->code->varnames[kv.key] == name){
                    SyntaxError("duplicate argument name");
                }
            }

            // eat type hints
            if(enable_type_hints && match(TK(":"))) consume(TK("@id"));
            if(state == 0 && curr().type == TK("=")) state = 2;
            int index = ctx()->add_varname(name);
            switch (state)
            {
                case 0:
                    decl->args.push_back(index);
                    break;
                case 1:
                    decl->starred_arg = index;
                    state+=1;
                    break;
                case 2: {
                    consume(TK("="));
                    PyObject* value = read_literal();
                    if(value == nullptr){
                        SyntaxError(Str("expect a literal, not ") + TK_STR(curr().type));
                    }
                    decl->kwargs.push_back(FuncDecl::KwArg{index, value});
                } break;
                case 3: SyntaxError("**kwargs is not supported yet"); break;
            }
        } while (match(TK(",")));
    }

    void compile_function(const std::vector<Expr_>& decorators={}){
        Str obj_name;
        Str decl_name;
        consume(TK("@id"));
        decl_name = prev().str();
        if(!ctx()->is_compiling_class && match(TK("@"))){
            consume(TK("@id"));
            obj_name = decl_name;
            decl_name = prev().str();
        }
        FuncDecl_ decl = push_f_context(decl_name);
        consume(TK("("));
        if (!match(TK(")"))) {
            _compile_f_args(decl, true);
            consume(TK(")"));
        }
        if(match(TK("->"))){
            if(!match(TK("None"))) consume(TK("@id"));
        }
        compile_block_body();
        pop_context();

        PyObject* docstring = nullptr;
        if(decl->code->codes.size()>=2 && decl->code->codes[0].op == OP_LOAD_CONST && decl->code->codes[1].op == OP_POP_TOP){
            PyObject* c = decl->code->consts[decl->code->codes[0].arg];
            if(is_type(c, vm->tp_str)){
                decl->code->codes[0].op = OP_NO_OP;
                decl->code->codes[1].op = OP_NO_OP;
                docstring = c;
            }
        }
        ctx()->emit(OP_LOAD_FUNCTION, ctx()->add_func_decl(decl), prev().line);
        if(docstring != nullptr){
            ctx()->emit(OP_SETUP_DOCSTRING, ctx()->add_const(docstring), prev().line);
        }
        // add decorators
        for(auto it=decorators.rbegin(); it!=decorators.rend(); ++it){
            (*it)->emit(ctx());
            ctx()->emit(OP_ROT_TWO, BC_NOARG, (*it)->line);
            ctx()->emit(OP_LOAD_NULL, BC_NOARG, BC_KEEPLINE);
            ctx()->emit(OP_ROT_TWO, BC_NOARG, BC_KEEPLINE);
            ctx()->emit(OP_CALL, 1, (*it)->line);
        }
        if(!ctx()->is_compiling_class){
            if(obj_name.empty()){
                auto e = make_expr<NameExpr>(decl_name, name_scope());
                e->emit_store(ctx());
            } else {
                ctx()->emit(OP_LOAD_GLOBAL, StrName(obj_name).index, prev().line);
                int index = StrName(decl_name).index;
                ctx()->emit(OP_STORE_ATTR, index, prev().line);
            }
        }else{
            int index = StrName(decl_name).index;
            ctx()->emit(OP_STORE_CLASS_ATTR, index, prev().line);
        }
    }

    PyObject* to_object(const TokenValue& value){
        PyObject* obj = nullptr;
        if(std::holds_alternative<i64>(value)){
            obj = VAR(std::get<i64>(value));
        }
        if(std::holds_alternative<f64>(value)){
            obj = VAR(std::get<f64>(value));
        }
        if(std::holds_alternative<Str>(value)){
            obj = VAR(std::get<Str>(value));
        }
        if(obj == nullptr) FATAL_ERROR();
        return obj;
    }

    PyObject* read_literal(){
        advance();
        switch(prev().type){
            case TK("-"): {
                consume(TK("@num"));
                PyObject* val = to_object(prev().value);
                return vm->num_negated(val);
            }
            case TK("@num"): return to_object(prev().value);
            case TK("@str"): return to_object(prev().value);
            case TK("True"): return VAR(true);
            case TK("False"): return VAR(false);
            case TK("None"): return vm->None;
            case TK("..."): return vm->Ellipsis;
            default: break;
        }
        return nullptr;
    }

    void SyntaxError(Str msg){ lexer->throw_err("SyntaxError", msg, err().line, err().start); }
    void SyntaxError(){ lexer->throw_err("SyntaxError", "invalid syntax", err().line, err().start); }
    void IndentationError(Str msg){ lexer->throw_err("IndentationError", msg, err().line, err().start); }

public:
    Compiler(VM* vm, const Str& source, const Str& filename, CompileMode mode, bool unknown_global_scope=false){
        this->vm = vm;
        this->used = false;
        this->unknown_global_scope = unknown_global_scope;
        this->lexer = std::make_unique<Lexer>(
            make_sp<SourceData>(source, filename, mode)
        );
        init_pratt_rules();
    }

    CodeObject_ compile(){
        if(used) FATAL_ERROR();
        used = true;

        tokens = lexer->run();
        // if(lexer->src->filename == "<stdin>"){
        //     for(auto& t: tokens) std::cout << t.info() << std::endl;
        // }

        CodeObject_ code = push_global_context();

        advance();          // skip @sof, so prev() is always valid
        match_newlines();   // skip possible leading '\n'

        if(mode()==EVAL_MODE) {
            EXPR_TUPLE(false);
            consume(TK("@eof"));
            ctx()->emit(OP_RETURN_VALUE, BC_NOARG, BC_KEEPLINE);
            pop_context();
            return code;
        }else if(mode()==JSON_MODE){
            EXPR();
            Expr_ e = ctx()->s_expr.popx();
            if(!e->is_json_object()) SyntaxError("expect a JSON object, literal or array");
            consume(TK("@eof"));
            e->emit(ctx());
            ctx()->emit(OP_RETURN_VALUE, BC_NOARG, BC_KEEPLINE);
            pop_context();
            return code;
        }

        while (!match(TK("@eof"))) {
            if (match(TK("class"))) {
                compile_class();
            } else {
                compile_stmt();
            }
            match_newlines();
        }
        pop_context();
        return code;
    }
};

} // namespace pkpy


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace pkpy{

#ifdef _WIN32

inline std::string getline(bool* eof=nullptr) {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    std::wstringstream wss;
    WCHAR buf;
    DWORD read;
    while (ReadConsoleW(hStdin, &buf, 1, &read, NULL) && buf != L'\n') {
        if(eof && buf == L'\x1A') *eof = true;  // Ctrl+Z
        wss << buf;
    }
    std::wstring wideInput = wss.str();
    int length = WideCharToMultiByte(CP_UTF8, 0, wideInput.c_str(), (int)wideInput.length(), NULL, 0, NULL, NULL);
    std::string output;
    output.resize(length);
    WideCharToMultiByte(CP_UTF8, 0, wideInput.c_str(), (int)wideInput.length(), &output[0], length, NULL, NULL);
    if(!output.empty() && output.back() == '\r') output.pop_back();
    return output;
}

#else

inline std::string getline(bool* eof=nullptr){
    std::string line;
    if(!std::getline(std::cin, line)){
        if(eof) *eof = true;
    }
    return line;
}

#endif

class REPL {
protected:
    int need_more_lines = 0;
    std::string buffer;
    VM* vm;
public:
    REPL(VM* vm) : vm(vm){
        (*vm->_stdout) << ("pocketpy " PK_VERSION " (" __DATE__ ", " __TIME__ ") ");
        (*vm->_stdout) << "[" << std::to_string(sizeof(void*) * 8) << " bit]" "\n";
        (*vm->_stdout) << ("https://github.com/blueloveTH/pocketpy" "\n");
        (*vm->_stdout) << ("Type \"exit()\" to exit." "\n");
    }

    bool input(std::string line){
        CompileMode mode = REPL_MODE;
        if(need_more_lines){
            buffer += line;
            buffer += '\n';
            int n = buffer.size();
            if(n>=need_more_lines){
                for(int i=buffer.size()-need_more_lines; i<buffer.size(); i++){
                    // no enough lines
                    if(buffer[i] != '\n') return true;
                }
                need_more_lines = 0;
                line = buffer;
                buffer.clear();
                mode = EXEC_MODE;
            }else{
                return true;
            }
        }
        
        try{
            vm->exec(line, "<stdin>", mode);
        }catch(NeedMoreLines& ne){
            buffer += line;
            buffer += '\n';
            need_more_lines = ne.is_compiling_class ? 3 : 2;
            if (need_more_lines) return true;
        }
        return false;
    }
};

} // namespace pkpy

// generated on 2023-05-02 23:19:23
#include <map>
#include <string>

namespace pkpy{
    inline static std::map<std::string, const char*> kPythonLibs = {
        {"collections", "\x63\x6c\x61\x73\x73\x20\x5f\x4c\x69\x6e\x6b\x65\x64\x4c\x69\x73\x74\x4e\x6f\x64\x65\x3a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x69\x6e\x69\x74\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x70\x72\x65\x76\x2c\x20\x6e\x65\x78\x74\x2c\x20\x76\x61\x6c\x75\x65\x29\x20\x2d\x3e\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x70\x72\x65\x76\x20\x3d\x20\x70\x72\x65\x76\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x6e\x65\x78\x74\x20\x3d\x20\x6e\x65\x78\x74\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x76\x61\x6c\x75\x65\x20\x3d\x20\x76\x61\x6c\x75\x65\x0a\x0a\x63\x6c\x61\x73\x73\x20\x64\x65\x71\x75\x65\x3a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x69\x6e\x69\x74\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x3d\x4e\x6f\x6e\x65\x29\x20\x2d\x3e\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x68\x65\x61\x64\x20\x3d\x20\x5f\x4c\x69\x6e\x6b\x65\x64\x4c\x69\x73\x74\x4e\x6f\x64\x65\x28\x4e\x6f\x6e\x65\x2c\x20\x4e\x6f\x6e\x65\x2c\x20\x4e\x6f\x6e\x65\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x74\x61\x69\x6c\x20\x3d\x20\x5f\x4c\x69\x6e\x6b\x65\x64\x4c\x69\x73\x74\x4e\x6f\x64\x65\x28\x4e\x6f\x6e\x65\x2c\x20\x4e\x6f\x6e\x65\x2c\x20\x4e\x6f\x6e\x65\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x68\x65\x61\x64\x2e\x6e\x65\x78\x74\x20\x3d\x20\x73\x65\x6c\x66\x2e\x74\x61\x69\x6c\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x74\x61\x69\x6c\x2e\x70\x72\x65\x76\x20\x3d\x20\x73\x65\x6c\x66\x2e\x68\x65\x61\x64\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x73\x69\x7a\x65\x20\x3d\x20\x30\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x20\x69\x73\x20\x6e\x6f\x74\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x76\x61\x6c\x75\x65\x20\x69\x6e\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x61\x70\x70\x65\x6e\x64\x28\x76\x61\x6c\x75\x65\x29\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x61\x70\x70\x65\x6e\x64\x28\x73\x65\x6c\x66\x2c\x20\x76\x61\x6c\x75\x65\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6e\x6f\x64\x65\x20\x3d\x20\x5f\x4c\x69\x6e\x6b\x65\x64\x4c\x69\x73\x74\x4e\x6f\x64\x65\x28\x73\x65\x6c\x66\x2e\x74\x61\x69\x6c\x2e\x70\x72\x65\x76\x2c\x20\x73\x65\x6c\x66\x2e\x74\x61\x69\x6c\x2c\x20\x76\x61\x6c\x75\x65\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x74\x61\x69\x6c\x2e\x70\x72\x65\x76\x2e\x6e\x65\x78\x74\x20\x3d\x20\x6e\x6f\x64\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x74\x61\x69\x6c\x2e\x70\x72\x65\x76\x20\x3d\x20\x6e\x6f\x64\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x73\x69\x7a\x65\x20\x2b\x3d\x20\x31\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x61\x70\x70\x65\x6e\x64\x6c\x65\x66\x74\x28\x73\x65\x6c\x66\x2c\x20\x76\x61\x6c\x75\x65\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6e\x6f\x64\x65\x20\x3d\x20\x5f\x4c\x69\x6e\x6b\x65\x64\x4c\x69\x73\x74\x4e\x6f\x64\x65\x28\x73\x65\x6c\x66\x2e\x68\x65\x61\x64\x2c\x20\x73\x65\x6c\x66\x2e\x68\x65\x61\x64\x2e\x6e\x65\x78\x74\x2c\x20\x76\x61\x6c\x75\x65\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x68\x65\x61\x64\x2e\x6e\x65\x78\x74\x2e\x70\x72\x65\x76\x20\x3d\x20\x6e\x6f\x64\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x68\x65\x61\x64\x2e\x6e\x65\x78\x74\x20\x3d\x20\x6e\x6f\x64\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x73\x69\x7a\x65\x20\x2b\x3d\x20\x31\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x70\x6f\x70\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x61\x73\x73\x65\x72\x74\x20\x73\x65\x6c\x66\x2e\x73\x69\x7a\x65\x20\x3e\x20\x30\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6e\x6f\x64\x65\x20\x3d\x20\x73\x65\x6c\x66\x2e\x74\x61\x69\x6c\x2e\x70\x72\x65\x76\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6e\x6f\x64\x65\x2e\x70\x72\x65\x76\x2e\x6e\x65\x78\x74\x20\x3d\x20\x73\x65\x6c\x66\x2e\x74\x61\x69\x6c\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x74\x61\x69\x6c\x2e\x70\x72\x65\x76\x20\x3d\x20\x6e\x6f\x64\x65\x2e\x70\x72\x65\x76\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x73\x69\x7a\x65\x20\x2d\x3d\x20\x31\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6e\x6f\x64\x65\x2e\x76\x61\x6c\x75\x65\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x70\x6f\x70\x6c\x65\x66\x74\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x61\x73\x73\x65\x72\x74\x20\x73\x65\x6c\x66\x2e\x73\x69\x7a\x65\x20\x3e\x20\x30\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6e\x6f\x64\x65\x20\x3d\x20\x73\x65\x6c\x66\x2e\x68\x65\x61\x64\x2e\x6e\x65\x78\x74\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6e\x6f\x64\x65\x2e\x6e\x65\x78\x74\x2e\x70\x72\x65\x76\x20\x3d\x20\x73\x65\x6c\x66\x2e\x68\x65\x61\x64\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x68\x65\x61\x64\x2e\x6e\x65\x78\x74\x20\x3d\x20\x6e\x6f\x64\x65\x2e\x6e\x65\x78\x74\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x73\x69\x7a\x65\x20\x2d\x3d\x20\x31\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6e\x6f\x64\x65\x2e\x76\x61\x6c\x75\x65\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x63\x6f\x70\x79\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6e\x65\x77\x5f\x6c\x69\x73\x74\x20\x3d\x20\x64\x65\x71\x75\x65\x28\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x76\x61\x6c\x75\x65\x20\x69\x6e\x20\x73\x65\x6c\x66\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x6e\x65\x77\x5f\x6c\x69\x73\x74\x2e\x61\x70\x70\x65\x6e\x64\x28\x76\x61\x6c\x75\x65\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6e\x65\x77\x5f\x6c\x69\x73\x74\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x6c\x65\x6e\x5f\x5f\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x73\x69\x7a\x65\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x69\x74\x65\x72\x5f\x5f\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6e\x6f\x64\x65\x20\x3d\x20\x73\x65\x6c\x66\x2e\x68\x65\x61\x64\x2e\x6e\x65\x78\x74\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x6e\x6f\x64\x65\x20\x69\x73\x20\x6e\x6f\x74\x20\x73\x65\x6c\x66\x2e\x74\x61\x69\x6c\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x79\x69\x65\x6c\x64\x20\x6e\x6f\x64\x65\x2e\x76\x61\x6c\x75\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x6e\x6f\x64\x65\x20\x3d\x20\x6e\x6f\x64\x65\x2e\x6e\x65\x78\x74\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x72\x65\x70\x72\x5f\x5f\x28\x73\x65\x6c\x66\x29\x20\x2d\x3e\x20\x73\x74\x72\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x66\x22\x64\x65\x71\x75\x65\x28\x7b\x6c\x69\x73\x74\x28\x73\x65\x6c\x66\x29\x7d\x29\x22\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x65\x71\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x5f\x5f\x6f\x3a\x20\x6f\x62\x6a\x65\x63\x74\x29\x20\x2d\x3e\x20\x62\x6f\x6f\x6c\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6e\x6f\x74\x20\x69\x73\x69\x6e\x73\x74\x61\x6e\x63\x65\x28\x5f\x5f\x6f\x2c\x20\x64\x65\x71\x75\x65\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6c\x65\x6e\x28\x73\x65\x6c\x66\x29\x20\x21\x3d\x20\x6c\x65\x6e\x28\x5f\x5f\x6f\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x74\x31\x2c\x20\x74\x32\x20\x3d\x20\x73\x65\x6c\x66\x2e\x68\x65\x61\x64\x2e\x6e\x65\x78\x74\x2c\x20\x5f\x5f\x6f\x2e\x68\x65\x61\x64\x2e\x6e\x65\x78\x74\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x74\x31\x20\x69\x73\x20\x6e\x6f\x74\x20\x73\x65\x6c\x66\x2e\x74\x61\x69\x6c\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x74\x31\x2e\x76\x61\x6c\x75\x65\x20\x21\x3d\x20\x74\x32\x2e\x76\x61\x6c\x75\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x74\x31\x2c\x20\x74\x32\x20\x3d\x20\x74\x31\x2e\x6e\x65\x78\x74\x2c\x20\x74\x32\x2e\x6e\x65\x78\x74\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x54\x72\x75\x65\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x6e\x65\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x5f\x5f\x6f\x3a\x20\x6f\x62\x6a\x65\x63\x74\x29\x20\x2d\x3e\x20\x62\x6f\x6f\x6c\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6e\x6f\x74\x20\x73\x65\x6c\x66\x20\x3d\x3d\x20\x5f\x5f\x6f\x0a\x0a\x0a\x64\x65\x66\x20\x43\x6f\x75\x6e\x74\x65\x72\x28\x69\x74\x65\x72\x61\x62\x6c\x65\x29\x3a\x0a\x20\x20\x20\x20\x61\x20\x3d\x20\x7b\x7d\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x78\x20\x69\x6e\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x78\x20\x69\x6e\x20\x61\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x61\x5b\x78\x5d\x20\x2b\x3d\x20\x31\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x65\x6c\x73\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x61\x5b\x78\x5d\x20\x3d\x20\x31\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x61"},
        {"requests", "\x63\x6c\x61\x73\x73\x20\x52\x65\x73\x70\x6f\x6e\x73\x65\x3a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x69\x6e\x69\x74\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x73\x74\x61\x74\x75\x73\x5f\x63\x6f\x64\x65\x2c\x20\x72\x65\x61\x73\x6f\x6e\x2c\x20\x63\x6f\x6e\x74\x65\x6e\x74\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x73\x74\x61\x74\x75\x73\x5f\x63\x6f\x64\x65\x20\x3d\x20\x73\x74\x61\x74\x75\x73\x5f\x63\x6f\x64\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x72\x65\x61\x73\x6f\x6e\x20\x3d\x20\x72\x65\x61\x73\x6f\x6e\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x63\x6f\x6e\x74\x65\x6e\x74\x20\x3d\x20\x63\x6f\x6e\x74\x65\x6e\x74\x0a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x61\x73\x73\x65\x72\x74\x20\x74\x79\x70\x65\x28\x73\x65\x6c\x66\x2e\x73\x74\x61\x74\x75\x73\x5f\x63\x6f\x64\x65\x29\x20\x69\x73\x20\x69\x6e\x74\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x61\x73\x73\x65\x72\x74\x20\x74\x79\x70\x65\x28\x73\x65\x6c\x66\x2e\x72\x65\x61\x73\x6f\x6e\x29\x20\x69\x73\x20\x73\x74\x72\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x61\x73\x73\x65\x72\x74\x20\x74\x79\x70\x65\x28\x73\x65\x6c\x66\x2e\x63\x6f\x6e\x74\x65\x6e\x74\x29\x20\x69\x73\x20\x62\x79\x74\x65\x73\x0a\x0a\x20\x20\x20\x20\x40\x70\x72\x6f\x70\x65\x72\x74\x79\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x74\x65\x78\x74\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x63\x6f\x6e\x74\x65\x6e\x74\x2e\x64\x65\x63\x6f\x64\x65\x28\x29\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x72\x65\x70\x72\x5f\x5f\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x63\x6f\x64\x65\x20\x3d\x20\x73\x65\x6c\x66\x2e\x73\x74\x61\x74\x75\x73\x5f\x63\x6f\x64\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x66\x27\x3c\x52\x65\x73\x70\x6f\x6e\x73\x65\x20\x5b\x7b\x63\x6f\x64\x65\x7d\x5d\x3e\x27\x0a\x0a\x64\x65\x66\x20\x5f\x70\x61\x72\x73\x65\x5f\x68\x28\x68\x65\x61\x64\x65\x72\x73\x29\x3a\x0a\x20\x20\x20\x20\x69\x66\x20\x68\x65\x61\x64\x65\x72\x73\x20\x69\x73\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x5b\x5d\x0a\x20\x20\x20\x20\x69\x66\x20\x74\x79\x70\x65\x28\x68\x65\x61\x64\x65\x72\x73\x29\x20\x69\x73\x20\x64\x69\x63\x74\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6c\x69\x73\x74\x28\x68\x65\x61\x64\x65\x72\x73\x2e\x69\x74\x65\x6d\x73\x28\x29\x29\x0a\x20\x20\x20\x20\x72\x61\x69\x73\x65\x20\x56\x61\x6c\x75\x65\x45\x72\x72\x6f\x72\x28\x27\x68\x65\x61\x64\x65\x72\x73\x20\x6d\x75\x73\x74\x20\x62\x65\x20\x64\x69\x63\x74\x20\x6f\x72\x20\x4e\x6f\x6e\x65\x27\x29\x0a\x0a\x64\x65\x66\x20\x67\x65\x74\x28\x75\x72\x6c\x2c\x20\x68\x65\x61\x64\x65\x72\x73\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x68\x65\x61\x64\x65\x72\x73\x20\x3d\x20\x5f\x70\x61\x72\x73\x65\x5f\x68\x28\x68\x65\x61\x64\x65\x72\x73\x29\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x5f\x72\x65\x71\x75\x65\x73\x74\x28\x27\x47\x45\x54\x27\x2c\x20\x75\x72\x6c\x2c\x20\x68\x65\x61\x64\x65\x72\x73\x2c\x20\x4e\x6f\x6e\x65\x29\x0a\x0a\x64\x65\x66\x20\x70\x6f\x73\x74\x28\x75\x72\x6c\x2c\x20\x64\x61\x74\x61\x3a\x20\x62\x79\x74\x65\x73\x2c\x20\x68\x65\x61\x64\x65\x72\x73\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x68\x65\x61\x64\x65\x72\x73\x20\x3d\x20\x5f\x70\x61\x72\x73\x65\x5f\x68\x28\x68\x65\x61\x64\x65\x72\x73\x29\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x5f\x72\x65\x71\x75\x65\x73\x74\x28\x27\x50\x4f\x53\x54\x27\x2c\x20\x75\x72\x6c\x2c\x20\x68\x65\x61\x64\x65\x72\x73\x2c\x20\x64\x61\x74\x61\x29\x0a\x0a\x64\x65\x66\x20\x70\x75\x74\x28\x75\x72\x6c\x2c\x20\x64\x61\x74\x61\x3a\x20\x62\x79\x74\x65\x73\x2c\x20\x68\x65\x61\x64\x65\x72\x73\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x68\x65\x61\x64\x65\x72\x73\x20\x3d\x20\x5f\x70\x61\x72\x73\x65\x5f\x68\x28\x68\x65\x61\x64\x65\x72\x73\x29\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x5f\x72\x65\x71\x75\x65\x73\x74\x28\x27\x50\x55\x54\x27\x2c\x20\x75\x72\x6c\x2c\x20\x68\x65\x61\x64\x65\x72\x73\x2c\x20\x64\x61\x74\x61\x29\x0a\x0a\x64\x65\x66\x20\x64\x65\x6c\x65\x74\x65\x28\x75\x72\x6c\x2c\x20\x68\x65\x61\x64\x65\x72\x73\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x68\x65\x61\x64\x65\x72\x73\x20\x3d\x20\x5f\x70\x61\x72\x73\x65\x5f\x68\x28\x68\x65\x61\x64\x65\x72\x73\x29\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x5f\x72\x65\x71\x75\x65\x73\x74\x28\x27\x44\x45\x4c\x45\x54\x45\x27\x2c\x20\x75\x72\x6c\x2c\x20\x68\x65\x61\x64\x65\x72\x73\x2c\x20\x4e\x6f\x6e\x65\x29"},
        {"this", "\x70\x72\x69\x6e\x74\x28\x22\x22\x22\x54\x68\x65\x20\x5a\x65\x6e\x20\x6f\x66\x20\x50\x79\x74\x68\x6f\x6e\x2c\x20\x62\x79\x20\x54\x69\x6d\x20\x50\x65\x74\x65\x72\x73\x0a\x0a\x42\x65\x61\x75\x74\x69\x66\x75\x6c\x20\x69\x73\x20\x62\x65\x74\x74\x65\x72\x20\x74\x68\x61\x6e\x20\x75\x67\x6c\x79\x2e\x0a\x45\x78\x70\x6c\x69\x63\x69\x74\x20\x69\x73\x20\x62\x65\x74\x74\x65\x72\x20\x74\x68\x61\x6e\x20\x69\x6d\x70\x6c\x69\x63\x69\x74\x2e\x0a\x53\x69\x6d\x70\x6c\x65\x20\x69\x73\x20\x62\x65\x74\x74\x65\x72\x20\x74\x68\x61\x6e\x20\x63\x6f\x6d\x70\x6c\x65\x78\x2e\x0a\x43\x6f\x6d\x70\x6c\x65\x78\x20\x69\x73\x20\x62\x65\x74\x74\x65\x72\x20\x74\x68\x61\x6e\x20\x63\x6f\x6d\x70\x6c\x69\x63\x61\x74\x65\x64\x2e\x0a\x46\x6c\x61\x74\x20\x69\x73\x20\x62\x65\x74\x74\x65\x72\x20\x74\x68\x61\x6e\x20\x6e\x65\x73\x74\x65\x64\x2e\x0a\x53\x70\x61\x72\x73\x65\x20\x69\x73\x20\x62\x65\x74\x74\x65\x72\x20\x74\x68\x61\x6e\x20\x64\x65\x6e\x73\x65\x2e\x0a\x52\x65\x61\x64\x61\x62\x69\x6c\x69\x74\x79\x20\x63\x6f\x75\x6e\x74\x73\x2e\x0a\x53\x70\x65\x63\x69\x61\x6c\x20\x63\x61\x73\x65\x73\x20\x61\x72\x65\x6e\x27\x74\x20\x73\x70\x65\x63\x69\x61\x6c\x20\x65\x6e\x6f\x75\x67\x68\x20\x74\x6f\x20\x62\x72\x65\x61\x6b\x20\x74\x68\x65\x20\x72\x75\x6c\x65\x73\x2e\x0a\x41\x6c\x74\x68\x6f\x75\x67\x68\x20\x70\x72\x61\x63\x74\x69\x63\x61\x6c\x69\x74\x79\x20\x62\x65\x61\x74\x73\x20\x70\x75\x72\x69\x74\x79\x2e\x0a\x45\x72\x72\x6f\x72\x73\x20\x73\x68\x6f\x75\x6c\x64\x20\x6e\x65\x76\x65\x72\x20\x70\x61\x73\x73\x20\x73\x69\x6c\x65\x6e\x74\x6c\x79\x2e\x0a\x55\x6e\x6c\x65\x73\x73\x20\x65\x78\x70\x6c\x69\x63\x69\x74\x6c\x79\x20\x73\x69\x6c\x65\x6e\x63\x65\x64\x2e\x0a\x49\x6e\x20\x74\x68\x65\x20\x66\x61\x63\x65\x20\x6f\x66\x20\x61\x6d\x62\x69\x67\x75\x69\x74\x79\x2c\x20\x72\x65\x66\x75\x73\x65\x20\x74\x68\x65\x20\x74\x65\x6d\x70\x74\x61\x74\x69\x6f\x6e\x20\x74\x6f\x20\x67\x75\x65\x73\x73\x2e\x0a\x54\x68\x65\x72\x65\x20\x73\x68\x6f\x75\x6c\x64\x20\x62\x65\x20\x6f\x6e\x65\x2d\x2d\x20\x61\x6e\x64\x20\x70\x72\x65\x66\x65\x72\x61\x62\x6c\x79\x20\x6f\x6e\x6c\x79\x20\x6f\x6e\x65\x20\x2d\x2d\x6f\x62\x76\x69\x6f\x75\x73\x20\x77\x61\x79\x20\x74\x6f\x20\x64\x6f\x20\x69\x74\x2e\x0a\x41\x6c\x74\x68\x6f\x75\x67\x68\x20\x74\x68\x61\x74\x20\x77\x61\x79\x20\x6d\x61\x79\x20\x6e\x6f\x74\x20\x62\x65\x20\x6f\x62\x76\x69\x6f\x75\x73\x20\x61\x74\x20\x66\x69\x72\x73\x74\x20\x75\x6e\x6c\x65\x73\x73\x20\x79\x6f\x75\x27\x72\x65\x20\x44\x75\x74\x63\x68\x2e\x0a\x4e\x6f\x77\x20\x69\x73\x20\x62\x65\x74\x74\x65\x72\x20\x74\x68\x61\x6e\x20\x6e\x65\x76\x65\x72\x2e\x0a\x41\x6c\x74\x68\x6f\x75\x67\x68\x20\x6e\x65\x76\x65\x72\x20\x69\x73\x20\x6f\x66\x74\x65\x6e\x20\x62\x65\x74\x74\x65\x72\x20\x74\x68\x61\x6e\x20\x2a\x72\x69\x67\x68\x74\x2a\x20\x6e\x6f\x77\x2e\x0a\x49\x66\x20\x74\x68\x65\x20\x69\x6d\x70\x6c\x65\x6d\x65\x6e\x74\x61\x74\x69\x6f\x6e\x20\x69\x73\x20\x68\x61\x72\x64\x20\x74\x6f\x20\x65\x78\x70\x6c\x61\x69\x6e\x2c\x20\x69\x74\x27\x73\x20\x61\x20\x62\x61\x64\x20\x69\x64\x65\x61\x2e\x0a\x49\x66\x20\x74\x68\x65\x20\x69\x6d\x70\x6c\x65\x6d\x65\x6e\x74\x61\x74\x69\x6f\x6e\x20\x69\x73\x20\x65\x61\x73\x79\x20\x74\x6f\x20\x65\x78\x70\x6c\x61\x69\x6e\x2c\x20\x69\x74\x20\x6d\x61\x79\x20\x62\x65\x20\x61\x20\x67\x6f\x6f\x64\x20\x69\x64\x65\x61\x2e\x0a\x4e\x61\x6d\x65\x73\x70\x61\x63\x65\x73\x20\x61\x72\x65\x20\x6f\x6e\x65\x20\x68\x6f\x6e\x6b\x69\x6e\x67\x20\x67\x72\x65\x61\x74\x20\x69\x64\x65\x61\x20\x2d\x2d\x20\x6c\x65\x74\x27\x73\x20\x64\x6f\x20\x6d\x6f\x72\x65\x20\x6f\x66\x20\x74\x68\x6f\x73\x65\x21\x22\x22\x22\x29"},
        {"heapq", "\x23\x20\x48\x65\x61\x70\x20\x71\x75\x65\x75\x65\x20\x61\x6c\x67\x6f\x72\x69\x74\x68\x6d\x20\x28\x61\x2e\x6b\x2e\x61\x2e\x20\x70\x72\x69\x6f\x72\x69\x74\x79\x20\x71\x75\x65\x75\x65\x29\x0a\x64\x65\x66\x20\x68\x65\x61\x70\x70\x75\x73\x68\x28\x68\x65\x61\x70\x2c\x20\x69\x74\x65\x6d\x29\x3a\x0a\x20\x20\x20\x20\x22\x22\x22\x50\x75\x73\x68\x20\x69\x74\x65\x6d\x20\x6f\x6e\x74\x6f\x20\x68\x65\x61\x70\x2c\x20\x6d\x61\x69\x6e\x74\x61\x69\x6e\x69\x6e\x67\x20\x74\x68\x65\x20\x68\x65\x61\x70\x20\x69\x6e\x76\x61\x72\x69\x61\x6e\x74\x2e\x22\x22\x22\x0a\x20\x20\x20\x20\x68\x65\x61\x70\x2e\x61\x70\x70\x65\x6e\x64\x28\x69\x74\x65\x6d\x29\x0a\x20\x20\x20\x20\x5f\x73\x69\x66\x74\x64\x6f\x77\x6e\x28\x68\x65\x61\x70\x2c\x20\x30\x2c\x20\x6c\x65\x6e\x28\x68\x65\x61\x70\x29\x2d\x31\x29\x0a\x0a\x64\x65\x66\x20\x68\x65\x61\x70\x70\x6f\x70\x28\x68\x65\x61\x70\x29\x3a\x0a\x20\x20\x20\x20\x22\x22\x22\x50\x6f\x70\x20\x74\x68\x65\x20\x73\x6d\x61\x6c\x6c\x65\x73\x74\x20\x69\x74\x65\x6d\x20\x6f\x66\x66\x20\x74\x68\x65\x20\x68\x65\x61\x70\x2c\x20\x6d\x61\x69\x6e\x74\x61\x69\x6e\x69\x6e\x67\x20\x74\x68\x65\x20\x68\x65\x61\x70\x20\x69\x6e\x76\x61\x72\x69\x61\x6e\x74\x2e\x22\x22\x22\x0a\x20\x20\x20\x20\x6c\x61\x73\x74\x65\x6c\x74\x20\x3d\x20\x68\x65\x61\x70\x2e\x70\x6f\x70\x28\x29\x20\x20\x20\x20\x23\x20\x72\x61\x69\x73\x65\x73\x20\x61\x70\x70\x72\x6f\x70\x72\x69\x61\x74\x65\x20\x49\x6e\x64\x65\x78\x45\x72\x72\x6f\x72\x20\x69\x66\x20\x68\x65\x61\x70\x20\x69\x73\x20\x65\x6d\x70\x74\x79\x0a\x20\x20\x20\x20\x69\x66\x20\x68\x65\x61\x70\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x69\x74\x65\x6d\x20\x3d\x20\x68\x65\x61\x70\x5b\x30\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x68\x65\x61\x70\x5b\x30\x5d\x20\x3d\x20\x6c\x61\x73\x74\x65\x6c\x74\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x5f\x73\x69\x66\x74\x75\x70\x28\x68\x65\x61\x70\x2c\x20\x30\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x72\x65\x74\x75\x72\x6e\x69\x74\x65\x6d\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6c\x61\x73\x74\x65\x6c\x74\x0a\x0a\x64\x65\x66\x20\x68\x65\x61\x70\x72\x65\x70\x6c\x61\x63\x65\x28\x68\x65\x61\x70\x2c\x20\x69\x74\x65\x6d\x29\x3a\x0a\x20\x20\x20\x20\x22\x22\x22\x50\x6f\x70\x20\x61\x6e\x64\x20\x72\x65\x74\x75\x72\x6e\x20\x74\x68\x65\x20\x63\x75\x72\x72\x65\x6e\x74\x20\x73\x6d\x61\x6c\x6c\x65\x73\x74\x20\x76\x61\x6c\x75\x65\x2c\x20\x61\x6e\x64\x20\x61\x64\x64\x20\x74\x68\x65\x20\x6e\x65\x77\x20\x69\x74\x65\x6d\x2e\x0a\x0a\x20\x20\x20\x20\x54\x68\x69\x73\x20\x69\x73\x20\x6d\x6f\x72\x65\x20\x65\x66\x66\x69\x63\x69\x65\x6e\x74\x20\x74\x68\x61\x6e\x20\x68\x65\x61\x70\x70\x6f\x70\x28\x29\x20\x66\x6f\x6c\x6c\x6f\x77\x65\x64\x20\x62\x79\x20\x68\x65\x61\x70\x70\x75\x73\x68\x28\x29\x2c\x20\x61\x6e\x64\x20\x63\x61\x6e\x20\x62\x65\x0a\x20\x20\x20\x20\x6d\x6f\x72\x65\x20\x61\x70\x70\x72\x6f\x70\x72\x69\x61\x74\x65\x20\x77\x68\x65\x6e\x20\x75\x73\x69\x6e\x67\x20\x61\x20\x66\x69\x78\x65\x64\x2d\x73\x69\x7a\x65\x20\x68\x65\x61\x70\x2e\x20\x20\x4e\x6f\x74\x65\x20\x74\x68\x61\x74\x20\x74\x68\x65\x20\x76\x61\x6c\x75\x65\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x65\x64\x20\x6d\x61\x79\x20\x62\x65\x20\x6c\x61\x72\x67\x65\x72\x20\x74\x68\x61\x6e\x20\x69\x74\x65\x6d\x21\x20\x20\x54\x68\x61\x74\x20\x63\x6f\x6e\x73\x74\x72\x61\x69\x6e\x73\x20\x72\x65\x61\x73\x6f\x6e\x61\x62\x6c\x65\x20\x75\x73\x65\x73\x20\x6f\x66\x0a\x20\x20\x20\x20\x74\x68\x69\x73\x20\x72\x6f\x75\x74\x69\x6e\x65\x20\x75\x6e\x6c\x65\x73\x73\x20\x77\x72\x69\x74\x74\x65\x6e\x20\x61\x73\x20\x70\x61\x72\x74\x20\x6f\x66\x20\x61\x20\x63\x6f\x6e\x64\x69\x74\x69\x6f\x6e\x61\x6c\x20\x72\x65\x70\x6c\x61\x63\x65\x6d\x65\x6e\x74\x3a\x0a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x69\x74\x65\x6d\x20\x3e\x20\x68\x65\x61\x70\x5b\x30\x5d\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x74\x65\x6d\x20\x3d\x20\x68\x65\x61\x70\x72\x65\x70\x6c\x61\x63\x65\x28\x68\x65\x61\x70\x2c\x20\x69\x74\x65\x6d\x29\x0a\x20\x20\x20\x20\x22\x22\x22\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x69\x74\x65\x6d\x20\x3d\x20\x68\x65\x61\x70\x5b\x30\x5d\x20\x20\x20\x20\x23\x20\x72\x61\x69\x73\x65\x73\x20\x61\x70\x70\x72\x6f\x70\x72\x69\x61\x74\x65\x20\x49\x6e\x64\x65\x78\x45\x72\x72\x6f\x72\x20\x69\x66\x20\x68\x65\x61\x70\x20\x69\x73\x20\x65\x6d\x70\x74\x79\x0a\x20\x20\x20\x20\x68\x65\x61\x70\x5b\x30\x5d\x20\x3d\x20\x69\x74\x65\x6d\x0a\x20\x20\x20\x20\x5f\x73\x69\x66\x74\x75\x70\x28\x68\x65\x61\x70\x2c\x20\x30\x29\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x72\x65\x74\x75\x72\x6e\x69\x74\x65\x6d\x0a\x0a\x64\x65\x66\x20\x68\x65\x61\x70\x70\x75\x73\x68\x70\x6f\x70\x28\x68\x65\x61\x70\x2c\x20\x69\x74\x65\x6d\x29\x3a\x0a\x20\x20\x20\x20\x22\x22\x22\x46\x61\x73\x74\x20\x76\x65\x72\x73\x69\x6f\x6e\x20\x6f\x66\x20\x61\x20\x68\x65\x61\x70\x70\x75\x73\x68\x20\x66\x6f\x6c\x6c\x6f\x77\x65\x64\x20\x62\x79\x20\x61\x20\x68\x65\x61\x70\x70\x6f\x70\x2e\x22\x22\x22\x0a\x20\x20\x20\x20\x69\x66\x20\x68\x65\x61\x70\x20\x61\x6e\x64\x20\x68\x65\x61\x70\x5b\x30\x5d\x20\x3c\x20\x69\x74\x65\x6d\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x74\x65\x6d\x2c\x20\x68\x65\x61\x70\x5b\x30\x5d\x20\x3d\x20\x68\x65\x61\x70\x5b\x30\x5d\x2c\x20\x69\x74\x65\x6d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x5f\x73\x69\x66\x74\x75\x70\x28\x68\x65\x61\x70\x2c\x20\x30\x29\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x69\x74\x65\x6d\x0a\x0a\x64\x65\x66\x20\x68\x65\x61\x70\x69\x66\x79\x28\x78\x29\x3a\x0a\x20\x20\x20\x20\x22\x22\x22\x54\x72\x61\x6e\x73\x66\x6f\x72\x6d\x20\x6c\x69\x73\x74\x20\x69\x6e\x74\x6f\x20\x61\x20\x68\x65\x61\x70\x2c\x20\x69\x6e\x2d\x70\x6c\x61\x63\x65\x2c\x20\x69\x6e\x20\x4f\x28\x6c\x65\x6e\x28\x78\x29\x29\x20\x74\x69\x6d\x65\x2e\x22\x22\x22\x0a\x20\x20\x20\x20\x6e\x20\x3d\x20\x6c\x65\x6e\x28\x78\x29\x0a\x20\x20\x20\x20\x23\x20\x54\x72\x61\x6e\x73\x66\x6f\x72\x6d\x20\x62\x6f\x74\x74\x6f\x6d\x2d\x75\x70\x2e\x20\x20\x54\x68\x65\x20\x6c\x61\x72\x67\x65\x73\x74\x20\x69\x6e\x64\x65\x78\x20\x74\x68\x65\x72\x65\x27\x73\x20\x61\x6e\x79\x20\x70\x6f\x69\x6e\x74\x20\x74\x6f\x20\x6c\x6f\x6f\x6b\x69\x6e\x67\x20\x61\x74\x0a\x20\x20\x20\x20\x23\x20\x69\x73\x20\x74\x68\x65\x20\x6c\x61\x72\x67\x65\x73\x74\x20\x77\x69\x74\x68\x20\x61\x20\x63\x68\x69\x6c\x64\x20\x69\x6e\x64\x65\x78\x20\x69\x6e\x2d\x72\x61\x6e\x67\x65\x2c\x20\x73\x6f\x20\x6d\x75\x73\x74\x20\x68\x61\x76\x65\x20\x32\x2a\x69\x20\x2b\x20\x31\x20\x3c\x20\x6e\x2c\x0a\x20\x20\x20\x20\x23\x20\x6f\x72\x20\x69\x20\x3c\x20\x28\x6e\x2d\x31\x29\x2f\x32\x2e\x20\x20\x49\x66\x20\x6e\x20\x69\x73\x20\x65\x76\x65\x6e\x20\x3d\x20\x32\x2a\x6a\x2c\x20\x74\x68\x69\x73\x20\x69\x73\x20\x28\x32\x2a\x6a\x2d\x31\x29\x2f\x32\x20\x3d\x20\x6a\x2d\x31\x2f\x32\x20\x73\x6f\x0a\x20\x20\x20\x20\x23\x20\x6a\x2d\x31\x20\x69\x73\x20\x74\x68\x65\x20\x6c\x61\x72\x67\x65\x73\x74\x2c\x20\x77\x68\x69\x63\x68\x20\x69\x73\x20\x6e\x2f\x2f\x32\x20\x2d\x20\x31\x2e\x20\x20\x49\x66\x20\x6e\x20\x69\x73\x20\x6f\x64\x64\x20\x3d\x20\x32\x2a\x6a\x2b\x31\x2c\x20\x74\x68\x69\x73\x20\x69\x73\x0a\x20\x20\x20\x20\x23\x20\x28\x32\x2a\x6a\x2b\x31\x2d\x31\x29\x2f\x32\x20\x3d\x20\x6a\x20\x73\x6f\x20\x6a\x2d\x31\x20\x69\x73\x20\x74\x68\x65\x20\x6c\x61\x72\x67\x65\x73\x74\x2c\x20\x61\x6e\x64\x20\x74\x68\x61\x74\x27\x73\x20\x61\x67\x61\x69\x6e\x20\x6e\x2f\x2f\x32\x2d\x31\x2e\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x72\x65\x76\x65\x72\x73\x65\x64\x28\x72\x61\x6e\x67\x65\x28\x6e\x2f\x2f\x32\x29\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x5f\x73\x69\x66\x74\x75\x70\x28\x78\x2c\x20\x69\x29\x0a\x0a\x23\x20\x27\x68\x65\x61\x70\x27\x20\x69\x73\x20\x61\x20\x68\x65\x61\x70\x20\x61\x74\x20\x61\x6c\x6c\x20\x69\x6e\x64\x69\x63\x65\x73\x20\x3e\x3d\x20\x73\x74\x61\x72\x74\x70\x6f\x73\x2c\x20\x65\x78\x63\x65\x70\x74\x20\x70\x6f\x73\x73\x69\x62\x6c\x79\x20\x66\x6f\x72\x20\x70\x6f\x73\x2e\x20\x20\x70\x6f\x73\x0a\x23\x20\x69\x73\x20\x74\x68\x65\x20\x69\x6e\x64\x65\x78\x20\x6f\x66\x20\x61\x20\x6c\x65\x61\x66\x20\x77\x69\x74\x68\x20\x61\x20\x70\x6f\x73\x73\x69\x62\x6c\x79\x20\x6f\x75\x74\x2d\x6f\x66\x2d\x6f\x72\x64\x65\x72\x20\x76\x61\x6c\x75\x65\x2e\x20\x20\x52\x65\x73\x74\x6f\x72\x65\x20\x74\x68\x65\x0a\x23\x20\x68\x65\x61\x70\x20\x69\x6e\x76\x61\x72\x69\x61\x6e\x74\x2e\x0a\x64\x65\x66\x20\x5f\x73\x69\x66\x74\x64\x6f\x77\x6e\x28\x68\x65\x61\x70\x2c\x20\x73\x74\x61\x72\x74\x70\x6f\x73\x2c\x20\x70\x6f\x73\x29\x3a\x0a\x20\x20\x20\x20\x6e\x65\x77\x69\x74\x65\x6d\x20\x3d\x20\x68\x65\x61\x70\x5b\x70\x6f\x73\x5d\x0a\x20\x20\x20\x20\x23\x20\x46\x6f\x6c\x6c\x6f\x77\x20\x74\x68\x65\x20\x70\x61\x74\x68\x20\x74\x6f\x20\x74\x68\x65\x20\x72\x6f\x6f\x74\x2c\x20\x6d\x6f\x76\x69\x6e\x67\x20\x70\x61\x72\x65\x6e\x74\x73\x20\x64\x6f\x77\x6e\x20\x75\x6e\x74\x69\x6c\x20\x66\x69\x6e\x64\x69\x6e\x67\x20\x61\x20\x70\x6c\x61\x63\x65\x0a\x20\x20\x20\x20\x23\x20\x6e\x65\x77\x69\x74\x65\x6d\x20\x66\x69\x74\x73\x2e\x0a\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x70\x6f\x73\x20\x3e\x20\x73\x74\x61\x72\x74\x70\x6f\x73\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x70\x61\x72\x65\x6e\x74\x70\x6f\x73\x20\x3d\x20\x28\x70\x6f\x73\x20\x2d\x20\x31\x29\x20\x3e\x3e\x20\x31\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x70\x61\x72\x65\x6e\x74\x20\x3d\x20\x68\x65\x61\x70\x5b\x70\x61\x72\x65\x6e\x74\x70\x6f\x73\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6e\x65\x77\x69\x74\x65\x6d\x20\x3c\x20\x70\x61\x72\x65\x6e\x74\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x68\x65\x61\x70\x5b\x70\x6f\x73\x5d\x20\x3d\x20\x70\x61\x72\x65\x6e\x74\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x70\x6f\x73\x20\x3d\x20\x70\x61\x72\x65\x6e\x74\x70\x6f\x73\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x63\x6f\x6e\x74\x69\x6e\x75\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x62\x72\x65\x61\x6b\x0a\x20\x20\x20\x20\x68\x65\x61\x70\x5b\x70\x6f\x73\x5d\x20\x3d\x20\x6e\x65\x77\x69\x74\x65\x6d\x0a\x0a\x64\x65\x66\x20\x5f\x73\x69\x66\x74\x75\x70\x28\x68\x65\x61\x70\x2c\x20\x70\x6f\x73\x29\x3a\x0a\x20\x20\x20\x20\x65\x6e\x64\x70\x6f\x73\x20\x3d\x20\x6c\x65\x6e\x28\x68\x65\x61\x70\x29\x0a\x20\x20\x20\x20\x73\x74\x61\x72\x74\x70\x6f\x73\x20\x3d\x20\x70\x6f\x73\x0a\x20\x20\x20\x20\x6e\x65\x77\x69\x74\x65\x6d\x20\x3d\x20\x68\x65\x61\x70\x5b\x70\x6f\x73\x5d\x0a\x20\x20\x20\x20\x23\x20\x42\x75\x62\x62\x6c\x65\x20\x75\x70\x20\x74\x68\x65\x20\x73\x6d\x61\x6c\x6c\x65\x72\x20\x63\x68\x69\x6c\x64\x20\x75\x6e\x74\x69\x6c\x20\x68\x69\x74\x74\x69\x6e\x67\x20\x61\x20\x6c\x65\x61\x66\x2e\x0a\x20\x20\x20\x20\x63\x68\x69\x6c\x64\x70\x6f\x73\x20\x3d\x20\x32\x2a\x70\x6f\x73\x20\x2b\x20\x31\x20\x20\x20\x20\x23\x20\x6c\x65\x66\x74\x6d\x6f\x73\x74\x20\x63\x68\x69\x6c\x64\x20\x70\x6f\x73\x69\x74\x69\x6f\x6e\x0a\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x63\x68\x69\x6c\x64\x70\x6f\x73\x20\x3c\x20\x65\x6e\x64\x70\x6f\x73\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x23\x20\x53\x65\x74\x20\x63\x68\x69\x6c\x64\x70\x6f\x73\x20\x74\x6f\x20\x69\x6e\x64\x65\x78\x20\x6f\x66\x20\x73\x6d\x61\x6c\x6c\x65\x72\x20\x63\x68\x69\x6c\x64\x2e\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x69\x67\x68\x74\x70\x6f\x73\x20\x3d\x20\x63\x68\x69\x6c\x64\x70\x6f\x73\x20\x2b\x20\x31\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x72\x69\x67\x68\x74\x70\x6f\x73\x20\x3c\x20\x65\x6e\x64\x70\x6f\x73\x20\x61\x6e\x64\x20\x6e\x6f\x74\x20\x68\x65\x61\x70\x5b\x63\x68\x69\x6c\x64\x70\x6f\x73\x5d\x20\x3c\x20\x68\x65\x61\x70\x5b\x72\x69\x67\x68\x74\x70\x6f\x73\x5d\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x63\x68\x69\x6c\x64\x70\x6f\x73\x20\x3d\x20\x72\x69\x67\x68\x74\x70\x6f\x73\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x23\x20\x4d\x6f\x76\x65\x20\x74\x68\x65\x20\x73\x6d\x61\x6c\x6c\x65\x72\x20\x63\x68\x69\x6c\x64\x20\x75\x70\x2e\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x68\x65\x61\x70\x5b\x70\x6f\x73\x5d\x20\x3d\x20\x68\x65\x61\x70\x5b\x63\x68\x69\x6c\x64\x70\x6f\x73\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x70\x6f\x73\x20\x3d\x20\x63\x68\x69\x6c\x64\x70\x6f\x73\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x63\x68\x69\x6c\x64\x70\x6f\x73\x20\x3d\x20\x32\x2a\x70\x6f\x73\x20\x2b\x20\x31\x0a\x20\x20\x20\x20\x23\x20\x54\x68\x65\x20\x6c\x65\x61\x66\x20\x61\x74\x20\x70\x6f\x73\x20\x69\x73\x20\x65\x6d\x70\x74\x79\x20\x6e\x6f\x77\x2e\x20\x20\x50\x75\x74\x20\x6e\x65\x77\x69\x74\x65\x6d\x20\x74\x68\x65\x72\x65\x2c\x20\x61\x6e\x64\x20\x62\x75\x62\x62\x6c\x65\x20\x69\x74\x20\x75\x70\x0a\x20\x20\x20\x20\x23\x20\x74\x6f\x20\x69\x74\x73\x20\x66\x69\x6e\x61\x6c\x20\x72\x65\x73\x74\x69\x6e\x67\x20\x70\x6c\x61\x63\x65\x20\x28\x62\x79\x20\x73\x69\x66\x74\x69\x6e\x67\x20\x69\x74\x73\x20\x70\x61\x72\x65\x6e\x74\x73\x20\x64\x6f\x77\x6e\x29\x2e\x0a\x20\x20\x20\x20\x68\x65\x61\x70\x5b\x70\x6f\x73\x5d\x20\x3d\x20\x6e\x65\x77\x69\x74\x65\x6d\x0a\x20\x20\x20\x20\x5f\x73\x69\x66\x74\x64\x6f\x77\x6e\x28\x68\x65\x61\x70\x2c\x20\x73\x74\x61\x72\x74\x70\x6f\x73\x2c\x20\x70\x6f\x73\x29"},
        {"bisect", "\x22\x22\x22\x42\x69\x73\x65\x63\x74\x69\x6f\x6e\x20\x61\x6c\x67\x6f\x72\x69\x74\x68\x6d\x73\x2e\x22\x22\x22\x0a\x0a\x64\x65\x66\x20\x69\x6e\x73\x6f\x72\x74\x5f\x72\x69\x67\x68\x74\x28\x61\x2c\x20\x78\x2c\x20\x6c\x6f\x3d\x30\x2c\x20\x68\x69\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x22\x22\x22\x49\x6e\x73\x65\x72\x74\x20\x69\x74\x65\x6d\x20\x78\x20\x69\x6e\x20\x6c\x69\x73\x74\x20\x61\x2c\x20\x61\x6e\x64\x20\x6b\x65\x65\x70\x20\x69\x74\x20\x73\x6f\x72\x74\x65\x64\x20\x61\x73\x73\x75\x6d\x69\x6e\x67\x20\x61\x20\x69\x73\x20\x73\x6f\x72\x74\x65\x64\x2e\x0a\x0a\x20\x20\x20\x20\x49\x66\x20\x78\x20\x69\x73\x20\x61\x6c\x72\x65\x61\x64\x79\x20\x69\x6e\x20\x61\x2c\x20\x69\x6e\x73\x65\x72\x74\x20\x69\x74\x20\x74\x6f\x20\x74\x68\x65\x20\x72\x69\x67\x68\x74\x20\x6f\x66\x20\x74\x68\x65\x20\x72\x69\x67\x68\x74\x6d\x6f\x73\x74\x20\x78\x2e\x0a\x0a\x20\x20\x20\x20\x4f\x70\x74\x69\x6f\x6e\x61\x6c\x20\x61\x72\x67\x73\x20\x6c\x6f\x20\x28\x64\x65\x66\x61\x75\x6c\x74\x20\x30\x29\x20\x61\x6e\x64\x20\x68\x69\x20\x28\x64\x65\x66\x61\x75\x6c\x74\x20\x6c\x65\x6e\x28\x61\x29\x29\x20\x62\x6f\x75\x6e\x64\x20\x74\x68\x65\x0a\x20\x20\x20\x20\x73\x6c\x69\x63\x65\x20\x6f\x66\x20\x61\x20\x74\x6f\x20\x62\x65\x20\x73\x65\x61\x72\x63\x68\x65\x64\x2e\x0a\x20\x20\x20\x20\x22\x22\x22\x0a\x0a\x20\x20\x20\x20\x6c\x6f\x20\x3d\x20\x62\x69\x73\x65\x63\x74\x5f\x72\x69\x67\x68\x74\x28\x61\x2c\x20\x78\x2c\x20\x6c\x6f\x2c\x20\x68\x69\x29\x0a\x20\x20\x20\x20\x61\x2e\x69\x6e\x73\x65\x72\x74\x28\x6c\x6f\x2c\x20\x78\x29\x0a\x0a\x64\x65\x66\x20\x62\x69\x73\x65\x63\x74\x5f\x72\x69\x67\x68\x74\x28\x61\x2c\x20\x78\x2c\x20\x6c\x6f\x3d\x30\x2c\x20\x68\x69\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x22\x22\x22\x52\x65\x74\x75\x72\x6e\x20\x74\x68\x65\x20\x69\x6e\x64\x65\x78\x20\x77\x68\x65\x72\x65\x20\x74\x6f\x20\x69\x6e\x73\x65\x72\x74\x20\x69\x74\x65\x6d\x20\x78\x20\x69\x6e\x20\x6c\x69\x73\x74\x20\x61\x2c\x20\x61\x73\x73\x75\x6d\x69\x6e\x67\x20\x61\x20\x69\x73\x20\x73\x6f\x72\x74\x65\x64\x2e\x0a\x0a\x20\x20\x20\x20\x54\x68\x65\x20\x72\x65\x74\x75\x72\x6e\x20\x76\x61\x6c\x75\x65\x20\x69\x20\x69\x73\x20\x73\x75\x63\x68\x20\x74\x68\x61\x74\x20\x61\x6c\x6c\x20\x65\x20\x69\x6e\x20\x61\x5b\x3a\x69\x5d\x20\x68\x61\x76\x65\x20\x65\x20\x3c\x3d\x20\x78\x2c\x20\x61\x6e\x64\x20\x61\x6c\x6c\x20\x65\x20\x69\x6e\x0a\x20\x20\x20\x20\x61\x5b\x69\x3a\x5d\x20\x68\x61\x76\x65\x20\x65\x20\x3e\x20\x78\x2e\x20\x20\x53\x6f\x20\x69\x66\x20\x78\x20\x61\x6c\x72\x65\x61\x64\x79\x20\x61\x70\x70\x65\x61\x72\x73\x20\x69\x6e\x20\x74\x68\x65\x20\x6c\x69\x73\x74\x2c\x20\x61\x2e\x69\x6e\x73\x65\x72\x74\x28\x78\x29\x20\x77\x69\x6c\x6c\x0a\x20\x20\x20\x20\x69\x6e\x73\x65\x72\x74\x20\x6a\x75\x73\x74\x20\x61\x66\x74\x65\x72\x20\x74\x68\x65\x20\x72\x69\x67\x68\x74\x6d\x6f\x73\x74\x20\x78\x20\x61\x6c\x72\x65\x61\x64\x79\x20\x74\x68\x65\x72\x65\x2e\x0a\x0a\x20\x20\x20\x20\x4f\x70\x74\x69\x6f\x6e\x61\x6c\x20\x61\x72\x67\x73\x20\x6c\x6f\x20\x28\x64\x65\x66\x61\x75\x6c\x74\x20\x30\x29\x20\x61\x6e\x64\x20\x68\x69\x20\x28\x64\x65\x66\x61\x75\x6c\x74\x20\x6c\x65\x6e\x28\x61\x29\x29\x20\x62\x6f\x75\x6e\x64\x20\x74\x68\x65\x0a\x20\x20\x20\x20\x73\x6c\x69\x63\x65\x20\x6f\x66\x20\x61\x20\x74\x6f\x20\x62\x65\x20\x73\x65\x61\x72\x63\x68\x65\x64\x2e\x0a\x20\x20\x20\x20\x22\x22\x22\x0a\x0a\x20\x20\x20\x20\x69\x66\x20\x6c\x6f\x20\x3c\x20\x30\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x61\x69\x73\x65\x20\x56\x61\x6c\x75\x65\x45\x72\x72\x6f\x72\x28\x27\x6c\x6f\x20\x6d\x75\x73\x74\x20\x62\x65\x20\x6e\x6f\x6e\x2d\x6e\x65\x67\x61\x74\x69\x76\x65\x27\x29\x0a\x20\x20\x20\x20\x69\x66\x20\x68\x69\x20\x69\x73\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x68\x69\x20\x3d\x20\x6c\x65\x6e\x28\x61\x29\x0a\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x6c\x6f\x20\x3c\x20\x68\x69\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6d\x69\x64\x20\x3d\x20\x28\x6c\x6f\x2b\x68\x69\x29\x2f\x2f\x32\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x78\x20\x3c\x20\x61\x5b\x6d\x69\x64\x5d\x3a\x20\x68\x69\x20\x3d\x20\x6d\x69\x64\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x65\x6c\x73\x65\x3a\x20\x6c\x6f\x20\x3d\x20\x6d\x69\x64\x2b\x31\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6c\x6f\x0a\x0a\x64\x65\x66\x20\x69\x6e\x73\x6f\x72\x74\x5f\x6c\x65\x66\x74\x28\x61\x2c\x20\x78\x2c\x20\x6c\x6f\x3d\x30\x2c\x20\x68\x69\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x22\x22\x22\x49\x6e\x73\x65\x72\x74\x20\x69\x74\x65\x6d\x20\x78\x20\x69\x6e\x20\x6c\x69\x73\x74\x20\x61\x2c\x20\x61\x6e\x64\x20\x6b\x65\x65\x70\x20\x69\x74\x20\x73\x6f\x72\x74\x65\x64\x20\x61\x73\x73\x75\x6d\x69\x6e\x67\x20\x61\x20\x69\x73\x20\x73\x6f\x72\x74\x65\x64\x2e\x0a\x0a\x20\x20\x20\x20\x49\x66\x20\x78\x20\x69\x73\x20\x61\x6c\x72\x65\x61\x64\x79\x20\x69\x6e\x20\x61\x2c\x20\x69\x6e\x73\x65\x72\x74\x20\x69\x74\x20\x74\x6f\x20\x74\x68\x65\x20\x6c\x65\x66\x74\x20\x6f\x66\x20\x74\x68\x65\x20\x6c\x65\x66\x74\x6d\x6f\x73\x74\x20\x78\x2e\x0a\x0a\x20\x20\x20\x20\x4f\x70\x74\x69\x6f\x6e\x61\x6c\x20\x61\x72\x67\x73\x20\x6c\x6f\x20\x28\x64\x65\x66\x61\x75\x6c\x74\x20\x30\x29\x20\x61\x6e\x64\x20\x68\x69\x20\x28\x64\x65\x66\x61\x75\x6c\x74\x20\x6c\x65\x6e\x28\x61\x29\x29\x20\x62\x6f\x75\x6e\x64\x20\x74\x68\x65\x0a\x20\x20\x20\x20\x73\x6c\x69\x63\x65\x20\x6f\x66\x20\x61\x20\x74\x6f\x20\x62\x65\x20\x73\x65\x61\x72\x63\x68\x65\x64\x2e\x0a\x20\x20\x20\x20\x22\x22\x22\x0a\x0a\x20\x20\x20\x20\x6c\x6f\x20\x3d\x20\x62\x69\x73\x65\x63\x74\x5f\x6c\x65\x66\x74\x28\x61\x2c\x20\x78\x2c\x20\x6c\x6f\x2c\x20\x68\x69\x29\x0a\x20\x20\x20\x20\x61\x2e\x69\x6e\x73\x65\x72\x74\x28\x6c\x6f\x2c\x20\x78\x29\x0a\x0a\x0a\x64\x65\x66\x20\x62\x69\x73\x65\x63\x74\x5f\x6c\x65\x66\x74\x28\x61\x2c\x20\x78\x2c\x20\x6c\x6f\x3d\x30\x2c\x20\x68\x69\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x22\x22\x22\x52\x65\x74\x75\x72\x6e\x20\x74\x68\x65\x20\x69\x6e\x64\x65\x78\x20\x77\x68\x65\x72\x65\x20\x74\x6f\x20\x69\x6e\x73\x65\x72\x74\x20\x69\x74\x65\x6d\x20\x78\x20\x69\x6e\x20\x6c\x69\x73\x74\x20\x61\x2c\x20\x61\x73\x73\x75\x6d\x69\x6e\x67\x20\x61\x20\x69\x73\x20\x73\x6f\x72\x74\x65\x64\x2e\x0a\x0a\x20\x20\x20\x20\x54\x68\x65\x20\x72\x65\x74\x75\x72\x6e\x20\x76\x61\x6c\x75\x65\x20\x69\x20\x69\x73\x20\x73\x75\x63\x68\x20\x74\x68\x61\x74\x20\x61\x6c\x6c\x20\x65\x20\x69\x6e\x20\x61\x5b\x3a\x69\x5d\x20\x68\x61\x76\x65\x20\x65\x20\x3c\x20\x78\x2c\x20\x61\x6e\x64\x20\x61\x6c\x6c\x20\x65\x20\x69\x6e\x0a\x20\x20\x20\x20\x61\x5b\x69\x3a\x5d\x20\x68\x61\x76\x65\x20\x65\x20\x3e\x3d\x20\x78\x2e\x20\x20\x53\x6f\x20\x69\x66\x20\x78\x20\x61\x6c\x72\x65\x61\x64\x79\x20\x61\x70\x70\x65\x61\x72\x73\x20\x69\x6e\x20\x74\x68\x65\x20\x6c\x69\x73\x74\x2c\x20\x61\x2e\x69\x6e\x73\x65\x72\x74\x28\x78\x29\x20\x77\x69\x6c\x6c\x0a\x20\x20\x20\x20\x69\x6e\x73\x65\x72\x74\x20\x6a\x75\x73\x74\x20\x62\x65\x66\x6f\x72\x65\x20\x74\x68\x65\x20\x6c\x65\x66\x74\x6d\x6f\x73\x74\x20\x78\x20\x61\x6c\x72\x65\x61\x64\x79\x20\x74\x68\x65\x72\x65\x2e\x0a\x0a\x20\x20\x20\x20\x4f\x70\x74\x69\x6f\x6e\x61\x6c\x20\x61\x72\x67\x73\x20\x6c\x6f\x20\x28\x64\x65\x66\x61\x75\x6c\x74\x20\x30\x29\x20\x61\x6e\x64\x20\x68\x69\x20\x28\x64\x65\x66\x61\x75\x6c\x74\x20\x6c\x65\x6e\x28\x61\x29\x29\x20\x62\x6f\x75\x6e\x64\x20\x74\x68\x65\x0a\x20\x20\x20\x20\x73\x6c\x69\x63\x65\x20\x6f\x66\x20\x61\x20\x74\x6f\x20\x62\x65\x20\x73\x65\x61\x72\x63\x68\x65\x64\x2e\x0a\x20\x20\x20\x20\x22\x22\x22\x0a\x0a\x20\x20\x20\x20\x69\x66\x20\x6c\x6f\x20\x3c\x20\x30\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x61\x69\x73\x65\x20\x56\x61\x6c\x75\x65\x45\x72\x72\x6f\x72\x28\x27\x6c\x6f\x20\x6d\x75\x73\x74\x20\x62\x65\x20\x6e\x6f\x6e\x2d\x6e\x65\x67\x61\x74\x69\x76\x65\x27\x29\x0a\x20\x20\x20\x20\x69\x66\x20\x68\x69\x20\x69\x73\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x68\x69\x20\x3d\x20\x6c\x65\x6e\x28\x61\x29\x0a\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x6c\x6f\x20\x3c\x20\x68\x69\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6d\x69\x64\x20\x3d\x20\x28\x6c\x6f\x2b\x68\x69\x29\x2f\x2f\x32\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x61\x5b\x6d\x69\x64\x5d\x20\x3c\x20\x78\x3a\x20\x6c\x6f\x20\x3d\x20\x6d\x69\x64\x2b\x31\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x65\x6c\x73\x65\x3a\x20\x68\x69\x20\x3d\x20\x6d\x69\x64\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6c\x6f\x0a\x0a\x23\x20\x43\x72\x65\x61\x74\x65\x20\x61\x6c\x69\x61\x73\x65\x73\x0a\x62\x69\x73\x65\x63\x74\x20\x3d\x20\x62\x69\x73\x65\x63\x74\x5f\x72\x69\x67\x68\x74\x0a\x69\x6e\x73\x6f\x72\x74\x20\x3d\x20\x69\x6e\x73\x6f\x72\x74\x5f\x72\x69\x67\x68\x74\x0a"},
        {"random", "\x5f\x69\x6e\x73\x74\x20\x3d\x20\x52\x61\x6e\x64\x6f\x6d\x28\x29\x0a\x0a\x73\x65\x65\x64\x20\x3d\x20\x5f\x69\x6e\x73\x74\x2e\x73\x65\x65\x64\x0a\x72\x61\x6e\x64\x6f\x6d\x20\x3d\x20\x5f\x69\x6e\x73\x74\x2e\x72\x61\x6e\x64\x6f\x6d\x0a\x75\x6e\x69\x66\x6f\x72\x6d\x20\x3d\x20\x5f\x69\x6e\x73\x74\x2e\x75\x6e\x69\x66\x6f\x72\x6d\x0a\x72\x61\x6e\x64\x69\x6e\x74\x20\x3d\x20\x5f\x69\x6e\x73\x74\x2e\x72\x61\x6e\x64\x69\x6e\x74\x0a\x0a\x64\x65\x66\x20\x73\x68\x75\x66\x66\x6c\x65\x28\x4c\x29\x3a\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x72\x61\x6e\x67\x65\x28\x6c\x65\x6e\x28\x4c\x29\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6a\x20\x3d\x20\x72\x61\x6e\x64\x69\x6e\x74\x28\x69\x2c\x20\x6c\x65\x6e\x28\x4c\x29\x20\x2d\x20\x31\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x4c\x5b\x69\x5d\x2c\x20\x4c\x5b\x6a\x5d\x20\x3d\x20\x4c\x5b\x6a\x5d\x2c\x20\x4c\x5b\x69\x5d\x0a\x0a\x64\x65\x66\x20\x63\x68\x6f\x69\x63\x65\x28\x4c\x29\x3a\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x4c\x5b\x72\x61\x6e\x64\x69\x6e\x74\x28\x30\x2c\x20\x6c\x65\x6e\x28\x4c\x29\x20\x2d\x20\x31\x29\x5d"},
        {"functools", "\x64\x65\x66\x20\x63\x61\x63\x68\x65\x28\x66\x29\x3a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x77\x72\x61\x70\x70\x65\x72\x28\x2a\x61\x72\x67\x73\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6e\x6f\x74\x20\x68\x61\x73\x61\x74\x74\x72\x28\x66\x2c\x20\x27\x63\x61\x63\x68\x65\x27\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x66\x2e\x63\x61\x63\x68\x65\x20\x3d\x20\x7b\x7d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6b\x65\x79\x20\x3d\x20\x61\x72\x67\x73\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6b\x65\x79\x20\x6e\x6f\x74\x20\x69\x6e\x20\x66\x2e\x63\x61\x63\x68\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x66\x2e\x63\x61\x63\x68\x65\x5b\x6b\x65\x79\x5d\x20\x3d\x20\x66\x28\x2a\x61\x72\x67\x73\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x66\x2e\x63\x61\x63\x68\x65\x5b\x6b\x65\x79\x5d\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x77\x72\x61\x70\x70\x65\x72"},
        {"_dict", "\x63\x6c\x61\x73\x73\x20\x64\x69\x63\x74\x3a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x69\x6e\x69\x74\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6d\x61\x70\x70\x69\x6e\x67\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x63\x61\x70\x61\x63\x69\x74\x79\x20\x3d\x20\x31\x36\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x61\x20\x3d\x20\x5b\x4e\x6f\x6e\x65\x5d\x20\x2a\x20\x73\x65\x6c\x66\x2e\x5f\x63\x61\x70\x61\x63\x69\x74\x79\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x6c\x65\x6e\x20\x3d\x20\x30\x0a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6d\x61\x70\x70\x69\x6e\x67\x20\x69\x73\x20\x6e\x6f\x74\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x6b\x2c\x76\x20\x69\x6e\x20\x6d\x61\x70\x70\x69\x6e\x67\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x5b\x6b\x5d\x20\x3d\x20\x76\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x6c\x65\x6e\x5f\x5f\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x6c\x65\x6e\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x70\x72\x6f\x62\x65\x28\x73\x65\x6c\x66\x2c\x20\x6b\x65\x79\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x20\x3d\x20\x68\x61\x73\x68\x28\x6b\x65\x79\x29\x20\x25\x20\x73\x65\x6c\x66\x2e\x5f\x63\x61\x70\x61\x63\x69\x74\x79\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x73\x65\x6c\x66\x2e\x5f\x61\x5b\x69\x5d\x20\x69\x73\x20\x6e\x6f\x74\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x73\x65\x6c\x66\x2e\x5f\x61\x5b\x69\x5d\x5b\x30\x5d\x20\x3d\x3d\x20\x6b\x65\x79\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x54\x72\x75\x65\x2c\x20\x69\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x20\x3d\x20\x28\x69\x20\x2b\x20\x31\x29\x20\x25\x20\x73\x65\x6c\x66\x2e\x5f\x63\x61\x70\x61\x63\x69\x74\x79\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x2c\x20\x69\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x67\x65\x74\x69\x74\x65\x6d\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6b\x65\x79\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6f\x6b\x2c\x20\x69\x20\x3d\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x70\x72\x6f\x62\x65\x28\x6b\x65\x79\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6e\x6f\x74\x20\x6f\x6b\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x61\x69\x73\x65\x20\x4b\x65\x79\x45\x72\x72\x6f\x72\x28\x72\x65\x70\x72\x28\x6b\x65\x79\x29\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x61\x5b\x69\x5d\x5b\x31\x5d\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x63\x6f\x6e\x74\x61\x69\x6e\x73\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6b\x65\x79\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6f\x6b\x2c\x20\x69\x20\x3d\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x70\x72\x6f\x62\x65\x28\x6b\x65\x79\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6f\x6b\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x73\x65\x74\x69\x74\x65\x6d\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6b\x65\x79\x2c\x20\x76\x61\x6c\x75\x65\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6f\x6b\x2c\x20\x69\x20\x3d\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x70\x72\x6f\x62\x65\x28\x6b\x65\x79\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6f\x6b\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x61\x5b\x69\x5d\x5b\x31\x5d\x20\x3d\x20\x76\x61\x6c\x75\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x65\x6c\x73\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x61\x5b\x69\x5d\x20\x3d\x20\x5b\x6b\x65\x79\x2c\x20\x76\x61\x6c\x75\x65\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x6c\x65\x6e\x20\x2b\x3d\x20\x31\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x73\x65\x6c\x66\x2e\x5f\x6c\x65\x6e\x20\x3e\x20\x73\x65\x6c\x66\x2e\x5f\x63\x61\x70\x61\x63\x69\x74\x79\x20\x2a\x20\x30\x2e\x36\x37\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x63\x61\x70\x61\x63\x69\x74\x79\x20\x2a\x3d\x20\x32\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x72\x65\x68\x61\x73\x68\x28\x29\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x64\x65\x6c\x69\x74\x65\x6d\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6b\x65\x79\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6f\x6b\x2c\x20\x69\x20\x3d\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x70\x72\x6f\x62\x65\x28\x6b\x65\x79\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6e\x6f\x74\x20\x6f\x6b\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x61\x69\x73\x65\x20\x4b\x65\x79\x45\x72\x72\x6f\x72\x28\x72\x65\x70\x72\x28\x6b\x65\x79\x29\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x61\x5b\x69\x5d\x20\x3d\x20\x4e\x6f\x6e\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x6c\x65\x6e\x20\x2d\x3d\x20\x31\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x72\x65\x68\x61\x73\x68\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6f\x6c\x64\x5f\x61\x20\x3d\x20\x73\x65\x6c\x66\x2e\x5f\x61\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x61\x20\x3d\x20\x5b\x4e\x6f\x6e\x65\x5d\x20\x2a\x20\x73\x65\x6c\x66\x2e\x5f\x63\x61\x70\x61\x63\x69\x74\x79\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x6c\x65\x6e\x20\x3d\x20\x30\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x6b\x76\x20\x69\x6e\x20\x6f\x6c\x64\x5f\x61\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6b\x76\x20\x69\x73\x20\x6e\x6f\x74\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x5b\x6b\x76\x5b\x30\x5d\x5d\x20\x3d\x20\x6b\x76\x5b\x31\x5d\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x67\x65\x74\x28\x73\x65\x6c\x66\x2c\x20\x6b\x65\x79\x2c\x20\x64\x65\x66\x61\x75\x6c\x74\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6f\x6b\x2c\x20\x69\x20\x3d\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x70\x72\x6f\x62\x65\x28\x6b\x65\x79\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6f\x6b\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x61\x5b\x69\x5d\x5b\x31\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x64\x65\x66\x61\x75\x6c\x74\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x6b\x65\x79\x73\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x6b\x76\x20\x69\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x61\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6b\x76\x20\x69\x73\x20\x6e\x6f\x74\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x79\x69\x65\x6c\x64\x20\x6b\x76\x5b\x30\x5d\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x76\x61\x6c\x75\x65\x73\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x6b\x76\x20\x69\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x61\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6b\x76\x20\x69\x73\x20\x6e\x6f\x74\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x79\x69\x65\x6c\x64\x20\x6b\x76\x5b\x31\x5d\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x69\x74\x65\x6d\x73\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x6b\x76\x20\x69\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x61\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6b\x76\x20\x69\x73\x20\x6e\x6f\x74\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x79\x69\x65\x6c\x64\x20\x6b\x76\x5b\x30\x5d\x2c\x20\x6b\x76\x5b\x31\x5d\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x63\x6c\x65\x61\x72\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x61\x20\x3d\x20\x5b\x4e\x6f\x6e\x65\x5d\x20\x2a\x20\x73\x65\x6c\x66\x2e\x5f\x63\x61\x70\x61\x63\x69\x74\x79\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x6c\x65\x6e\x20\x3d\x20\x30\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x75\x70\x64\x61\x74\x65\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x6b\x2c\x20\x76\x20\x69\x6e\x20\x6f\x74\x68\x65\x72\x2e\x69\x74\x65\x6d\x73\x28\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x5b\x6b\x5d\x20\x3d\x20\x76\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x63\x6f\x70\x79\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x64\x20\x3d\x20\x64\x69\x63\x74\x28\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x6b\x76\x20\x69\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x61\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6b\x76\x20\x69\x73\x20\x6e\x6f\x74\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x64\x5b\x6b\x76\x5b\x30\x5d\x5d\x20\x3d\x20\x6b\x76\x5b\x31\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x64\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x72\x65\x70\x72\x5f\x5f\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x61\x20\x3d\x20\x5b\x72\x65\x70\x72\x28\x6b\x29\x2b\x27\x3a\x20\x27\x2b\x72\x65\x70\x72\x28\x76\x29\x20\x66\x6f\x72\x20\x6b\x2c\x76\x20\x69\x6e\x20\x73\x65\x6c\x66\x2e\x69\x74\x65\x6d\x73\x28\x29\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x27\x7b\x27\x2b\x20\x27\x2c\x20\x27\x2e\x6a\x6f\x69\x6e\x28\x61\x29\x20\x2b\x20\x27\x7d\x27\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x6a\x73\x6f\x6e\x5f\x5f\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x61\x20\x3d\x20\x5b\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x6b\x2c\x76\x20\x69\x6e\x20\x73\x65\x6c\x66\x2e\x69\x74\x65\x6d\x73\x28\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x74\x79\x70\x65\x28\x6b\x29\x20\x69\x73\x20\x6e\x6f\x74\x20\x73\x74\x72\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x61\x69\x73\x65\x20\x54\x79\x70\x65\x45\x72\x72\x6f\x72\x28\x27\x6a\x73\x6f\x6e\x20\x6b\x65\x79\x73\x20\x6d\x75\x73\x74\x20\x62\x65\x20\x73\x74\x72\x69\x6e\x67\x73\x2c\x20\x67\x6f\x74\x20\x27\x20\x2b\x20\x72\x65\x70\x72\x28\x6b\x29\x20\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x61\x2e\x61\x70\x70\x65\x6e\x64\x28\x6b\x2e\x5f\x5f\x6a\x73\x6f\x6e\x5f\x5f\x28\x29\x2b\x27\x3a\x20\x27\x2b\x76\x2e\x5f\x5f\x6a\x73\x6f\x6e\x5f\x5f\x28\x29\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x27\x7b\x27\x2b\x20\x27\x2c\x20\x27\x2e\x6a\x6f\x69\x6e\x28\x61\x29\x20\x2b\x20\x27\x7d\x27\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x65\x71\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x5f\x5f\x6f\x3a\x20\x6f\x62\x6a\x65\x63\x74\x29\x20\x2d\x3e\x20\x62\x6f\x6f\x6c\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x74\x79\x70\x65\x28\x5f\x5f\x6f\x29\x20\x69\x73\x20\x6e\x6f\x74\x20\x64\x69\x63\x74\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6c\x65\x6e\x28\x73\x65\x6c\x66\x29\x20\x21\x3d\x20\x6c\x65\x6e\x28\x5f\x5f\x6f\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x6b\x20\x69\x6e\x20\x73\x65\x6c\x66\x2e\x6b\x65\x79\x73\x28\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6b\x20\x6e\x6f\x74\x20\x69\x6e\x20\x5f\x5f\x6f\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x73\x65\x6c\x66\x5b\x6b\x5d\x20\x21\x3d\x20\x5f\x5f\x6f\x5b\x6b\x5d\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x54\x72\x75\x65\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x6e\x65\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x5f\x5f\x6f\x3a\x20\x6f\x62\x6a\x65\x63\x74\x29\x20\x2d\x3e\x20\x62\x6f\x6f\x6c\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6e\x6f\x74\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x65\x71\x5f\x5f\x28\x5f\x5f\x6f\x29"},
        {"builtins", "\x64\x65\x66\x20\x70\x72\x69\x6e\x74\x28\x2a\x61\x72\x67\x73\x2c\x20\x73\x65\x70\x3d\x27\x20\x27\x2c\x20\x65\x6e\x64\x3d\x27\x5c\x6e\x27\x29\x3a\x0a\x20\x20\x20\x20\x73\x20\x3d\x20\x73\x65\x70\x2e\x6a\x6f\x69\x6e\x28\x5b\x73\x74\x72\x28\x69\x29\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x61\x72\x67\x73\x5d\x29\x0a\x20\x20\x20\x20\x5f\x5f\x73\x79\x73\x5f\x73\x74\x64\x6f\x75\x74\x5f\x77\x72\x69\x74\x65\x28\x73\x20\x2b\x20\x65\x6e\x64\x29\x0a\x0a\x64\x65\x66\x20\x72\x6f\x75\x6e\x64\x28\x78\x2c\x20\x6e\x64\x69\x67\x69\x74\x73\x3d\x30\x29\x3a\x0a\x20\x20\x20\x20\x61\x73\x73\x65\x72\x74\x20\x6e\x64\x69\x67\x69\x74\x73\x20\x3e\x3d\x20\x30\x0a\x20\x20\x20\x20\x69\x66\x20\x6e\x64\x69\x67\x69\x74\x73\x20\x3d\x3d\x20\x30\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x69\x6e\x74\x28\x78\x20\x2b\x20\x30\x2e\x35\x29\x20\x69\x66\x20\x78\x20\x3e\x3d\x20\x30\x20\x65\x6c\x73\x65\x20\x69\x6e\x74\x28\x78\x20\x2d\x20\x30\x2e\x35\x29\x0a\x20\x20\x20\x20\x69\x66\x20\x78\x20\x3e\x3d\x20\x30\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x69\x6e\x74\x28\x78\x20\x2a\x20\x31\x30\x2a\x2a\x6e\x64\x69\x67\x69\x74\x73\x20\x2b\x20\x30\x2e\x35\x29\x20\x2f\x20\x31\x30\x2a\x2a\x6e\x64\x69\x67\x69\x74\x73\x0a\x20\x20\x20\x20\x65\x6c\x73\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x69\x6e\x74\x28\x78\x20\x2a\x20\x31\x30\x2a\x2a\x6e\x64\x69\x67\x69\x74\x73\x20\x2d\x20\x30\x2e\x35\x29\x20\x2f\x20\x31\x30\x2a\x2a\x6e\x64\x69\x67\x69\x74\x73\x0a\x0a\x64\x65\x66\x20\x61\x62\x73\x28\x78\x29\x3a\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x2d\x78\x20\x69\x66\x20\x78\x20\x3c\x20\x30\x20\x65\x6c\x73\x65\x20\x78\x0a\x0a\x64\x65\x66\x20\x6d\x61\x78\x28\x61\x2c\x20\x62\x29\x3a\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x61\x20\x69\x66\x20\x61\x20\x3e\x20\x62\x20\x65\x6c\x73\x65\x20\x62\x0a\x0a\x64\x65\x66\x20\x6d\x69\x6e\x28\x61\x2c\x20\x62\x29\x3a\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x61\x20\x69\x66\x20\x61\x20\x3c\x20\x62\x20\x65\x6c\x73\x65\x20\x62\x0a\x0a\x64\x65\x66\x20\x61\x6c\x6c\x28\x69\x74\x65\x72\x61\x62\x6c\x65\x29\x3a\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6e\x6f\x74\x20\x69\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x54\x72\x75\x65\x0a\x0a\x64\x65\x66\x20\x61\x6e\x79\x28\x69\x74\x65\x72\x61\x62\x6c\x65\x29\x3a\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x69\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x54\x72\x75\x65\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x0a\x64\x65\x66\x20\x65\x6e\x75\x6d\x65\x72\x61\x74\x65\x28\x69\x74\x65\x72\x61\x62\x6c\x65\x2c\x20\x73\x74\x61\x72\x74\x3d\x30\x29\x3a\x0a\x20\x20\x20\x20\x6e\x20\x3d\x20\x73\x74\x61\x72\x74\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x65\x6c\x65\x6d\x20\x69\x6e\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x79\x69\x65\x6c\x64\x20\x6e\x2c\x20\x65\x6c\x65\x6d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6e\x20\x2b\x3d\x20\x31\x0a\x0a\x64\x65\x66\x20\x73\x75\x6d\x28\x69\x74\x65\x72\x61\x62\x6c\x65\x29\x3a\x0a\x20\x20\x20\x20\x72\x65\x73\x20\x3d\x20\x30\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x73\x20\x2b\x3d\x20\x69\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x72\x65\x73\x0a\x0a\x64\x65\x66\x20\x6d\x61\x70\x28\x66\x2c\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x29\x3a\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x79\x69\x65\x6c\x64\x20\x66\x28\x69\x29\x0a\x0a\x64\x65\x66\x20\x66\x69\x6c\x74\x65\x72\x28\x66\x2c\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x29\x3a\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x66\x28\x69\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x79\x69\x65\x6c\x64\x20\x69\x0a\x0a\x64\x65\x66\x20\x7a\x69\x70\x28\x61\x2c\x20\x62\x29\x3a\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x72\x61\x6e\x67\x65\x28\x6d\x69\x6e\x28\x6c\x65\x6e\x28\x61\x29\x2c\x20\x6c\x65\x6e\x28\x62\x29\x29\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x79\x69\x65\x6c\x64\x20\x28\x61\x5b\x69\x5d\x2c\x20\x62\x5b\x69\x5d\x29\x0a\x0a\x64\x65\x66\x20\x72\x65\x76\x65\x72\x73\x65\x64\x28\x69\x74\x65\x72\x61\x62\x6c\x65\x29\x3a\x0a\x20\x20\x20\x20\x61\x20\x3d\x20\x6c\x69\x73\x74\x28\x69\x74\x65\x72\x61\x62\x6c\x65\x29\x0a\x20\x20\x20\x20\x61\x2e\x72\x65\x76\x65\x72\x73\x65\x28\x29\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x61\x0a\x0a\x64\x65\x66\x20\x73\x6f\x72\x74\x65\x64\x28\x69\x74\x65\x72\x61\x62\x6c\x65\x2c\x20\x72\x65\x76\x65\x72\x73\x65\x3d\x46\x61\x6c\x73\x65\x29\x3a\x0a\x20\x20\x20\x20\x61\x20\x3d\x20\x6c\x69\x73\x74\x28\x69\x74\x65\x72\x61\x62\x6c\x65\x29\x0a\x20\x20\x20\x20\x61\x2e\x73\x6f\x72\x74\x28\x72\x65\x76\x65\x72\x73\x65\x3d\x72\x65\x76\x65\x72\x73\x65\x29\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x61\x0a\x0a\x23\x23\x23\x23\x23\x20\x73\x74\x72\x20\x23\x23\x23\x23\x23\x0a\x0a\x73\x74\x72\x2e\x5f\x5f\x6d\x75\x6c\x5f\x5f\x20\x3d\x20\x6c\x61\x6d\x62\x64\x61\x20\x73\x65\x6c\x66\x2c\x20\x6e\x3a\x20\x27\x27\x2e\x6a\x6f\x69\x6e\x28\x5b\x73\x65\x6c\x66\x20\x66\x6f\x72\x20\x5f\x20\x69\x6e\x20\x72\x61\x6e\x67\x65\x28\x6e\x29\x5d\x29\x0a\x0a\x64\x65\x66\x20\x73\x74\x72\x40\x73\x70\x6c\x69\x74\x28\x73\x65\x6c\x66\x2c\x20\x73\x65\x70\x29\x3a\x0a\x20\x20\x20\x20\x69\x66\x20\x73\x65\x70\x20\x3d\x3d\x20\x22\x22\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6c\x69\x73\x74\x28\x73\x65\x6c\x66\x29\x0a\x20\x20\x20\x20\x72\x65\x73\x20\x3d\x20\x5b\x5d\x0a\x20\x20\x20\x20\x69\x20\x3d\x20\x30\x0a\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x69\x20\x3c\x20\x6c\x65\x6e\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x73\x65\x6c\x66\x5b\x69\x3a\x69\x2b\x6c\x65\x6e\x28\x73\x65\x70\x29\x5d\x20\x3d\x3d\x20\x73\x65\x70\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x73\x2e\x61\x70\x70\x65\x6e\x64\x28\x73\x65\x6c\x66\x5b\x3a\x69\x5d\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x20\x3d\x20\x73\x65\x6c\x66\x5b\x69\x2b\x6c\x65\x6e\x28\x73\x65\x70\x29\x3a\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x20\x3d\x20\x30\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x65\x6c\x73\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x20\x2b\x3d\x20\x31\x0a\x20\x20\x20\x20\x72\x65\x73\x2e\x61\x70\x70\x65\x6e\x64\x28\x73\x65\x6c\x66\x29\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x72\x65\x73\x0a\x0a\x64\x65\x66\x20\x73\x74\x72\x40\x66\x6f\x72\x6d\x61\x74\x28\x73\x65\x6c\x66\x2c\x20\x2a\x61\x72\x67\x73\x29\x3a\x0a\x20\x20\x20\x20\x69\x66\x20\x27\x7b\x7d\x27\x20\x69\x6e\x20\x73\x65\x6c\x66\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x72\x61\x6e\x67\x65\x28\x6c\x65\x6e\x28\x61\x72\x67\x73\x29\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x20\x3d\x20\x73\x65\x6c\x66\x2e\x72\x65\x70\x6c\x61\x63\x65\x28\x27\x7b\x7d\x27\x2c\x20\x73\x74\x72\x28\x61\x72\x67\x73\x5b\x69\x5d\x29\x2c\x20\x31\x29\x0a\x20\x20\x20\x20\x65\x6c\x73\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x72\x61\x6e\x67\x65\x28\x6c\x65\x6e\x28\x61\x72\x67\x73\x29\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x20\x3d\x20\x73\x65\x6c\x66\x2e\x72\x65\x70\x6c\x61\x63\x65\x28\x27\x7b\x27\x2b\x73\x74\x72\x28\x69\x29\x2b\x27\x7d\x27\x2c\x20\x73\x74\x72\x28\x61\x72\x67\x73\x5b\x69\x5d\x29\x29\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x0a\x0a\x64\x65\x66\x20\x73\x74\x72\x40\x73\x74\x72\x69\x70\x28\x73\x65\x6c\x66\x2c\x20\x63\x68\x61\x72\x73\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x63\x68\x61\x72\x73\x20\x3d\x20\x63\x68\x61\x72\x73\x20\x6f\x72\x20\x27\x20\x5c\x74\x5c\x6e\x5c\x72\x27\x0a\x20\x20\x20\x20\x69\x20\x3d\x20\x30\x0a\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x69\x20\x3c\x20\x6c\x65\x6e\x28\x73\x65\x6c\x66\x29\x20\x61\x6e\x64\x20\x73\x65\x6c\x66\x5b\x69\x5d\x20\x69\x6e\x20\x63\x68\x61\x72\x73\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x20\x2b\x3d\x20\x31\x0a\x20\x20\x20\x20\x6a\x20\x3d\x20\x6c\x65\x6e\x28\x73\x65\x6c\x66\x29\x20\x2d\x20\x31\x0a\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x6a\x20\x3e\x3d\x20\x30\x20\x61\x6e\x64\x20\x73\x65\x6c\x66\x5b\x6a\x5d\x20\x69\x6e\x20\x63\x68\x61\x72\x73\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6a\x20\x2d\x3d\x20\x31\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x5b\x69\x3a\x6a\x2b\x31\x5d\x0a\x0a\x23\x23\x23\x23\x23\x20\x6c\x69\x73\x74\x20\x23\x23\x23\x23\x23\x0a\x0a\x6c\x69\x73\x74\x2e\x5f\x5f\x6e\x65\x77\x5f\x5f\x20\x3d\x20\x6c\x61\x6d\x62\x64\x61\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x3a\x20\x5b\x78\x20\x66\x6f\x72\x20\x78\x20\x69\x6e\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x5d\x0a\x6c\x69\x73\x74\x2e\x5f\x5f\x72\x65\x70\x72\x5f\x5f\x20\x3d\x20\x6c\x61\x6d\x62\x64\x61\x20\x73\x65\x6c\x66\x3a\x20\x27\x5b\x27\x20\x2b\x20\x27\x2c\x20\x27\x2e\x6a\x6f\x69\x6e\x28\x5b\x72\x65\x70\x72\x28\x69\x29\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x73\x65\x6c\x66\x5d\x29\x20\x2b\x20\x27\x5d\x27\x0a\x74\x75\x70\x6c\x65\x2e\x5f\x5f\x72\x65\x70\x72\x5f\x5f\x20\x3d\x20\x6c\x61\x6d\x62\x64\x61\x20\x73\x65\x6c\x66\x3a\x20\x27\x28\x27\x20\x2b\x20\x27\x2c\x20\x27\x2e\x6a\x6f\x69\x6e\x28\x5b\x72\x65\x70\x72\x28\x69\x29\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x73\x65\x6c\x66\x5d\x29\x20\x2b\x20\x27\x29\x27\x0a\x6c\x69\x73\x74\x2e\x5f\x5f\x6a\x73\x6f\x6e\x5f\x5f\x20\x3d\x20\x6c\x61\x6d\x62\x64\x61\x20\x73\x65\x6c\x66\x3a\x20\x27\x5b\x27\x20\x2b\x20\x27\x2c\x20\x27\x2e\x6a\x6f\x69\x6e\x28\x5b\x69\x2e\x5f\x5f\x6a\x73\x6f\x6e\x5f\x5f\x28\x29\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x73\x65\x6c\x66\x5d\x29\x20\x2b\x20\x27\x5d\x27\x0a\x74\x75\x70\x6c\x65\x2e\x5f\x5f\x6a\x73\x6f\x6e\x5f\x5f\x20\x3d\x20\x6c\x61\x6d\x62\x64\x61\x20\x73\x65\x6c\x66\x3a\x20\x27\x5b\x27\x20\x2b\x20\x27\x2c\x20\x27\x2e\x6a\x6f\x69\x6e\x28\x5b\x69\x2e\x5f\x5f\x6a\x73\x6f\x6e\x5f\x5f\x28\x29\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x73\x65\x6c\x66\x5d\x29\x20\x2b\x20\x27\x5d\x27\x0a\x0a\x64\x65\x66\x20\x5f\x5f\x71\x73\x6f\x72\x74\x28\x61\x3a\x20\x6c\x69\x73\x74\x2c\x20\x4c\x3a\x20\x69\x6e\x74\x2c\x20\x52\x3a\x20\x69\x6e\x74\x29\x3a\x0a\x20\x20\x20\x20\x69\x66\x20\x4c\x20\x3e\x3d\x20\x52\x3a\x20\x72\x65\x74\x75\x72\x6e\x3b\x0a\x20\x20\x20\x20\x6d\x69\x64\x20\x3d\x20\x61\x5b\x28\x52\x2b\x4c\x29\x2f\x2f\x32\x5d\x3b\x0a\x20\x20\x20\x20\x69\x2c\x20\x6a\x20\x3d\x20\x4c\x2c\x20\x52\x0a\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x69\x3c\x3d\x6a\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x61\x5b\x69\x5d\x3c\x6d\x69\x64\x3a\x20\x69\x2b\x3d\x31\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x77\x68\x69\x6c\x65\x20\x61\x5b\x6a\x5d\x3e\x6d\x69\x64\x3a\x20\x6a\x2d\x3d\x31\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x69\x3c\x3d\x6a\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x61\x5b\x69\x5d\x2c\x20\x61\x5b\x6a\x5d\x20\x3d\x20\x61\x5b\x6a\x5d\x2c\x20\x61\x5b\x69\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x2b\x3d\x31\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x6a\x2d\x3d\x31\x0a\x20\x20\x20\x20\x5f\x5f\x71\x73\x6f\x72\x74\x28\x61\x2c\x20\x4c\x2c\x20\x6a\x29\x0a\x20\x20\x20\x20\x5f\x5f\x71\x73\x6f\x72\x74\x28\x61\x2c\x20\x69\x2c\x20\x52\x29\x0a\x0a\x64\x65\x66\x20\x6c\x69\x73\x74\x40\x73\x6f\x72\x74\x28\x73\x65\x6c\x66\x2c\x20\x72\x65\x76\x65\x72\x73\x65\x3d\x46\x61\x6c\x73\x65\x29\x3a\x0a\x20\x20\x20\x20\x5f\x5f\x71\x73\x6f\x72\x74\x28\x73\x65\x6c\x66\x2c\x20\x30\x2c\x20\x6c\x65\x6e\x28\x73\x65\x6c\x66\x29\x2d\x31\x29\x0a\x20\x20\x20\x20\x69\x66\x20\x72\x65\x76\x65\x72\x73\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x72\x65\x76\x65\x72\x73\x65\x28\x29\x0a\x0a\x64\x65\x66\x20\x6c\x69\x73\x74\x40\x72\x65\x6d\x6f\x76\x65\x28\x73\x65\x6c\x66\x2c\x20\x76\x61\x6c\x75\x65\x29\x3a\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x72\x61\x6e\x67\x65\x28\x6c\x65\x6e\x28\x73\x65\x6c\x66\x29\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x73\x65\x6c\x66\x5b\x69\x5d\x20\x3d\x3d\x20\x76\x61\x6c\x75\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x64\x65\x6c\x20\x73\x65\x6c\x66\x5b\x69\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x54\x72\x75\x65\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x0a\x64\x65\x66\x20\x6c\x69\x73\x74\x40\x69\x6e\x64\x65\x78\x28\x73\x65\x6c\x66\x2c\x20\x76\x61\x6c\x75\x65\x29\x3a\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x72\x61\x6e\x67\x65\x28\x6c\x65\x6e\x28\x73\x65\x6c\x66\x29\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x73\x65\x6c\x66\x5b\x69\x5d\x20\x3d\x3d\x20\x76\x61\x6c\x75\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x69\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x2d\x31\x0a\x0a\x64\x65\x66\x20\x6c\x69\x73\x74\x40\x70\x6f\x70\x28\x73\x65\x6c\x66\x2c\x20\x69\x3d\x2d\x31\x29\x3a\x0a\x20\x20\x20\x20\x72\x65\x73\x20\x3d\x20\x73\x65\x6c\x66\x5b\x69\x5d\x0a\x20\x20\x20\x20\x64\x65\x6c\x20\x73\x65\x6c\x66\x5b\x69\x5d\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x72\x65\x73\x0a\x0a\x64\x65\x66\x20\x6c\x69\x73\x74\x40\x5f\x5f\x65\x71\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x69\x66\x20\x74\x79\x70\x65\x28\x73\x65\x6c\x66\x29\x20\x69\x73\x20\x6e\x6f\x74\x20\x74\x79\x70\x65\x28\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x20\x20\x20\x20\x69\x66\x20\x6c\x65\x6e\x28\x73\x65\x6c\x66\x29\x20\x21\x3d\x20\x6c\x65\x6e\x28\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x72\x61\x6e\x67\x65\x28\x6c\x65\x6e\x28\x73\x65\x6c\x66\x29\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x73\x65\x6c\x66\x5b\x69\x5d\x20\x21\x3d\x20\x6f\x74\x68\x65\x72\x5b\x69\x5d\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x54\x72\x75\x65\x0a\x74\x75\x70\x6c\x65\x2e\x5f\x5f\x65\x71\x5f\x5f\x20\x3d\x20\x6c\x69\x73\x74\x2e\x5f\x5f\x65\x71\x5f\x5f\x0a\x6c\x69\x73\x74\x2e\x5f\x5f\x6e\x65\x5f\x5f\x20\x3d\x20\x6c\x61\x6d\x62\x64\x61\x20\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x3a\x20\x6e\x6f\x74\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x65\x71\x5f\x5f\x28\x6f\x74\x68\x65\x72\x29\x0a\x74\x75\x70\x6c\x65\x2e\x5f\x5f\x6e\x65\x5f\x5f\x20\x3d\x20\x6c\x61\x6d\x62\x64\x61\x20\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x3a\x20\x6e\x6f\x74\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x65\x71\x5f\x5f\x28\x6f\x74\x68\x65\x72\x29\x0a\x0a\x64\x65\x66\x20\x6c\x69\x73\x74\x40\x63\x6f\x75\x6e\x74\x28\x73\x65\x6c\x66\x2c\x20\x78\x29\x3a\x0a\x20\x20\x20\x20\x72\x65\x73\x20\x3d\x20\x30\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x73\x65\x6c\x66\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x69\x20\x3d\x3d\x20\x78\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x73\x20\x2b\x3d\x20\x31\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x72\x65\x73\x0a\x74\x75\x70\x6c\x65\x2e\x63\x6f\x75\x6e\x74\x20\x3d\x20\x6c\x69\x73\x74\x2e\x63\x6f\x75\x6e\x74\x0a\x0a\x64\x65\x66\x20\x6c\x69\x73\x74\x40\x5f\x5f\x63\x6f\x6e\x74\x61\x69\x6e\x73\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x69\x74\x65\x6d\x29\x3a\x0a\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x73\x65\x6c\x66\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x69\x20\x3d\x3d\x20\x69\x74\x65\x6d\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x54\x72\x75\x65\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x46\x61\x6c\x73\x65\x0a\x74\x75\x70\x6c\x65\x2e\x5f\x5f\x63\x6f\x6e\x74\x61\x69\x6e\x73\x5f\x5f\x20\x3d\x20\x6c\x69\x73\x74\x2e\x5f\x5f\x63\x6f\x6e\x74\x61\x69\x6e\x73\x5f\x5f\x0a\x0a\x0a\x63\x6c\x61\x73\x73\x20\x70\x72\x6f\x70\x65\x72\x74\x79\x3a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x69\x6e\x69\x74\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x66\x67\x65\x74\x2c\x20\x66\x73\x65\x74\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x66\x67\x65\x74\x20\x3d\x20\x66\x67\x65\x74\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x66\x73\x65\x74\x20\x3d\x20\x66\x73\x65\x74\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x68\x61\x73\x61\x74\x74\x72\x28\x66\x67\x65\x74\x2c\x20\x27\x5f\x5f\x64\x6f\x63\x5f\x5f\x27\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x64\x6f\x63\x5f\x5f\x20\x3d\x20\x66\x67\x65\x74\x2e\x5f\x5f\x64\x6f\x63\x5f\x5f\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x67\x65\x74\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6f\x62\x6a\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x66\x67\x65\x74\x28\x6f\x62\x6a\x29\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x73\x65\x74\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6f\x62\x6a\x2c\x20\x76\x61\x6c\x75\x65\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x73\x65\x6c\x66\x2e\x66\x73\x65\x74\x20\x69\x73\x20\x4e\x6f\x6e\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x61\x69\x73\x65\x20\x41\x74\x74\x72\x69\x62\x75\x74\x65\x45\x72\x72\x6f\x72\x28\x22\x72\x65\x61\x64\x6f\x6e\x6c\x79\x20\x70\x72\x6f\x70\x65\x72\x74\x79\x22\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x66\x73\x65\x74\x28\x6f\x62\x6a\x2c\x20\x76\x61\x6c\x75\x65\x29\x0a\x0a\x63\x6c\x61\x73\x73\x20\x73\x74\x61\x74\x69\x63\x6d\x65\x74\x68\x6f\x64\x3a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x69\x6e\x69\x74\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x66\x20\x3d\x20\x66\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x67\x65\x74\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6f\x62\x6a\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x66\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x63\x61\x6c\x6c\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x2a\x61\x72\x67\x73\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x66\x28\x2a\x61\x72\x67\x73\x29\x0a\x20\x20\x20\x20\x0a\x64\x65\x66\x20\x74\x79\x70\x65\x40\x5f\x5f\x72\x65\x70\x72\x5f\x5f\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x22\x3c\x63\x6c\x61\x73\x73\x20\x27\x22\x20\x2b\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x6e\x61\x6d\x65\x5f\x5f\x20\x2b\x20\x22\x27\x3e\x22\x0a\x0a\x64\x65\x66\x20\x68\x65\x6c\x70\x28\x6f\x62\x6a\x29\x3a\x0a\x20\x20\x20\x20\x69\x66\x20\x68\x61\x73\x61\x74\x74\x72\x28\x6f\x62\x6a\x2c\x20\x27\x5f\x5f\x66\x75\x6e\x63\x5f\x5f\x27\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x6f\x62\x6a\x20\x3d\x20\x6f\x62\x6a\x2e\x5f\x5f\x66\x75\x6e\x63\x5f\x5f\x0a\x20\x20\x20\x20\x69\x66\x20\x68\x61\x73\x61\x74\x74\x72\x28\x6f\x62\x6a\x2c\x20\x27\x5f\x5f\x64\x6f\x63\x5f\x5f\x27\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x70\x72\x69\x6e\x74\x28\x6f\x62\x6a\x2e\x5f\x5f\x64\x6f\x63\x5f\x5f\x29\x0a\x20\x20\x20\x20\x65\x6c\x73\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x70\x72\x69\x6e\x74\x28\x22\x4e\x6f\x20\x64\x6f\x63\x73\x74\x72\x69\x6e\x67\x20\x66\x6f\x75\x6e\x64\x22\x29\x0a"},
        {"_set", "\x63\x6c\x61\x73\x73\x20\x73\x65\x74\x3a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x69\x6e\x69\x74\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x3d\x4e\x6f\x6e\x65\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x20\x3d\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x20\x6f\x72\x20\x5b\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x61\x20\x3d\x20\x7b\x7d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x69\x74\x65\x6d\x20\x69\x6e\x20\x69\x74\x65\x72\x61\x62\x6c\x65\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x61\x64\x64\x28\x69\x74\x65\x6d\x29\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x61\x64\x64\x28\x73\x65\x6c\x66\x2c\x20\x65\x6c\x65\x6d\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x61\x5b\x65\x6c\x65\x6d\x5d\x20\x3d\x20\x4e\x6f\x6e\x65\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x64\x69\x73\x63\x61\x72\x64\x28\x73\x65\x6c\x66\x2c\x20\x65\x6c\x65\x6d\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x65\x6c\x65\x6d\x20\x69\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x61\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x64\x65\x6c\x20\x73\x65\x6c\x66\x2e\x5f\x61\x5b\x65\x6c\x65\x6d\x5d\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x72\x65\x6d\x6f\x76\x65\x28\x73\x65\x6c\x66\x2c\x20\x65\x6c\x65\x6d\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x64\x65\x6c\x20\x73\x65\x6c\x66\x2e\x5f\x61\x5b\x65\x6c\x65\x6d\x5d\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x63\x6c\x65\x61\x72\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x5f\x61\x2e\x63\x6c\x65\x61\x72\x28\x29\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x75\x70\x64\x61\x74\x65\x28\x73\x65\x6c\x66\x2c\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x65\x6c\x65\x6d\x20\x69\x6e\x20\x6f\x74\x68\x65\x72\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x73\x65\x6c\x66\x2e\x61\x64\x64\x28\x65\x6c\x65\x6d\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x6c\x65\x6e\x5f\x5f\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6c\x65\x6e\x28\x73\x65\x6c\x66\x2e\x5f\x61\x29\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x63\x6f\x70\x79\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x74\x28\x73\x65\x6c\x66\x2e\x5f\x61\x2e\x6b\x65\x79\x73\x28\x29\x29\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x61\x6e\x64\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x20\x3d\x20\x73\x65\x74\x28\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x65\x6c\x65\x6d\x20\x69\x6e\x20\x73\x65\x6c\x66\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x65\x6c\x65\x6d\x20\x69\x6e\x20\x6f\x74\x68\x65\x72\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x2e\x61\x64\x64\x28\x65\x6c\x65\x6d\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x72\x65\x74\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x6f\x72\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x20\x3d\x20\x73\x65\x6c\x66\x2e\x63\x6f\x70\x79\x28\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x65\x6c\x65\x6d\x20\x69\x6e\x20\x6f\x74\x68\x65\x72\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x2e\x61\x64\x64\x28\x65\x6c\x65\x6d\x29\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x72\x65\x74\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x73\x75\x62\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x20\x3d\x20\x73\x65\x74\x28\x29\x20\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x65\x6c\x65\x6d\x20\x69\x6e\x20\x73\x65\x6c\x66\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x65\x6c\x65\x6d\x20\x6e\x6f\x74\x20\x69\x6e\x20\x6f\x74\x68\x65\x72\x3a\x20\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x2e\x61\x64\x64\x28\x65\x6c\x65\x6d\x29\x20\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x72\x65\x74\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x78\x6f\x72\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x20\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x20\x3d\x20\x73\x65\x74\x28\x29\x20\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x65\x6c\x65\x6d\x20\x69\x6e\x20\x73\x65\x6c\x66\x3a\x20\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x65\x6c\x65\x6d\x20\x6e\x6f\x74\x20\x69\x6e\x20\x6f\x74\x68\x65\x72\x3a\x20\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x2e\x61\x64\x64\x28\x65\x6c\x65\x6d\x29\x20\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x66\x6f\x72\x20\x65\x6c\x65\x6d\x20\x69\x6e\x20\x6f\x74\x68\x65\x72\x3a\x20\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x65\x6c\x65\x6d\x20\x6e\x6f\x74\x20\x69\x6e\x20\x73\x65\x6c\x66\x3a\x20\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x2e\x61\x64\x64\x28\x65\x6c\x65\x6d\x29\x20\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x72\x65\x74\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x75\x6e\x69\x6f\x6e\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x20\x7c\x20\x6f\x74\x68\x65\x72\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x69\x6e\x74\x65\x72\x73\x65\x63\x74\x69\x6f\x6e\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x20\x26\x20\x6f\x74\x68\x65\x72\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x64\x69\x66\x66\x65\x72\x65\x6e\x63\x65\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x20\x2d\x20\x6f\x74\x68\x65\x72\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x73\x79\x6d\x6d\x65\x74\x72\x69\x63\x5f\x64\x69\x66\x66\x65\x72\x65\x6e\x63\x65\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x20\x20\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x20\x5e\x20\x6f\x74\x68\x65\x72\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x65\x71\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x78\x6f\x72\x5f\x5f\x28\x6f\x74\x68\x65\x72\x29\x2e\x5f\x5f\x6c\x65\x6e\x5f\x5f\x28\x29\x20\x3d\x3d\x20\x30\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x6e\x65\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x78\x6f\x72\x5f\x5f\x28\x6f\x74\x68\x65\x72\x29\x2e\x5f\x5f\x6c\x65\x6e\x5f\x5f\x28\x29\x20\x21\x3d\x20\x30\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x69\x73\x64\x69\x73\x6a\x6f\x69\x6e\x74\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x61\x6e\x64\x5f\x5f\x28\x6f\x74\x68\x65\x72\x29\x2e\x5f\x5f\x6c\x65\x6e\x5f\x5f\x28\x29\x20\x3d\x3d\x20\x30\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x69\x73\x73\x75\x62\x73\x65\x74\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x5f\x73\x75\x62\x5f\x5f\x28\x6f\x74\x68\x65\x72\x29\x2e\x5f\x5f\x6c\x65\x6e\x5f\x5f\x28\x29\x20\x3d\x3d\x20\x30\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x69\x73\x73\x75\x70\x65\x72\x73\x65\x74\x28\x73\x65\x6c\x66\x2c\x20\x6f\x74\x68\x65\x72\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x6f\x74\x68\x65\x72\x2e\x5f\x5f\x73\x75\x62\x5f\x5f\x28\x73\x65\x6c\x66\x29\x2e\x5f\x5f\x6c\x65\x6e\x5f\x5f\x28\x29\x20\x3d\x3d\x20\x30\x0a\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x63\x6f\x6e\x74\x61\x69\x6e\x73\x5f\x5f\x28\x73\x65\x6c\x66\x2c\x20\x65\x6c\x65\x6d\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x65\x6c\x65\x6d\x20\x69\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x61\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x72\x65\x70\x72\x5f\x5f\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x69\x66\x20\x6c\x65\x6e\x28\x73\x65\x6c\x66\x29\x20\x3d\x3d\x20\x30\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x27\x73\x65\x74\x28\x29\x27\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x27\x7b\x27\x2b\x20\x27\x2c\x20\x27\x2e\x6a\x6f\x69\x6e\x28\x5b\x72\x65\x70\x72\x28\x69\x29\x20\x66\x6f\x72\x20\x69\x20\x69\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x61\x2e\x6b\x65\x79\x73\x28\x29\x5d\x29\x20\x2b\x20\x27\x7d\x27\x0a\x20\x20\x20\x20\x0a\x20\x20\x20\x20\x64\x65\x66\x20\x5f\x5f\x69\x74\x65\x72\x5f\x5f\x28\x73\x65\x6c\x66\x29\x3a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x72\x65\x74\x75\x72\x6e\x20\x73\x65\x6c\x66\x2e\x5f\x61\x2e\x6b\x65\x79\x73\x28\x29"},

    };
}   // namespace pkpy



namespace pkpy{

class RangeIter final: public BaseIter {
    i64 current;
    Range r;    // copy by value, so we don't need to keep ref
public:
    RangeIter(VM* vm, PyObject* ref) : BaseIter(vm) {
        this->r = OBJ_GET(Range, ref);
        this->current = r.start;
    }

    bool _has_next(){
        return r.step > 0 ? current < r.stop : current > r.stop;
    }

    PyObject* next(){
        if(!_has_next()) return vm->StopIteration;
        current += r.step;
        return VAR(current-r.step);
    }
};

template <typename T>
class ArrayIter final: public BaseIter {
    PyObject* ref;
    T* array;
    int index;
public:
    ArrayIter(VM* vm, PyObject* ref) : BaseIter(vm), ref(ref) {
        array = &OBJ_GET(T, ref);
        index = 0;
    }

    PyObject* next() override{
        if(index >= array->size()) return vm->StopIteration;
        return array->operator[](index++);
    }

    void _gc_mark() const{
        OBJ_MARK(ref);
    }
};

class StringIter final: public BaseIter {
    PyObject* ref;
    int index;
public:
    StringIter(VM* vm, PyObject* ref) : BaseIter(vm), ref(ref), index(0) {}

    PyObject* next() override{
        // TODO: optimize this to use iterator
        // operator[] is O(n) complexity
        Str* str = &OBJ_GET(Str, ref);
        if(index == str->u8_length()) return vm->StopIteration;
        return VAR(str->u8_getitem(index++));
    }

    void _gc_mark() const{
        OBJ_MARK(ref);
    }
};

inline PyObject* Generator::next(){
    if(state == 2) return vm->StopIteration;
    // reset frame._sp_base
    frame._sp_base = frame._s->_sp;
    frame._locals.a = frame._s->_sp;
    // restore the context
    for(PyObject* obj: s_backup) frame._s->push(obj);
    s_backup.clear();
    vm->callstack.push(std::move(frame));
    PyObject* ret = vm->_run_top_frame();
    if(ret == PY_OP_YIELD){
        // backup the context
        frame = std::move(vm->callstack.top());
        PyObject* ret = frame._s->popx();
        for(PyObject* obj: frame.stack_view()) s_backup.push_back(obj);
        vm->_pop_frame();
        state = 1;
        if(ret == vm->StopIteration) state = 2;
        return ret;
    }else{
        state = 2;
        return vm->StopIteration;
    }
}

inline void Generator::_gc_mark() const{
    frame._gc_mark();
    for(PyObject* obj: s_backup) OBJ_MARK(obj);
}

} // namespace pkpy


namespace pkpy {

// https://github.com/zhicheng/base64/blob/master/base64.c

#define BASE64_PAD '='
#define BASE64DE_FIRST '+'
#define BASE64DE_LAST 'z'

/* BASE 64 encode table */
static const char base64en[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/',
};

/* ASCII order for BASE 64 decode, 255 in unused character */
static const unsigned char base64de[] = {
	/* nul, soh, stx, etx, eot, enq, ack, bel, */
	   255, 255, 255, 255, 255, 255, 255, 255,

	/*  bs,  ht,  nl,  vt,  np,  cr,  so,  si, */
	   255, 255, 255, 255, 255, 255, 255, 255,

	/* dle, dc1, dc2, dc3, dc4, nak, syn, etb, */
	   255, 255, 255, 255, 255, 255, 255, 255,

	/* can,  em, sub, esc,  fs,  gs,  rs,  us, */
	   255, 255, 255, 255, 255, 255, 255, 255,

	/*  sp, '!', '"', '#', '$', '%', '&', ''', */
	   255, 255, 255, 255, 255, 255, 255, 255,

	/* '(', ')', '*', '+', ',', '-', '.', '/', */
	   255, 255, 255,  62, 255, 255, 255,  63,

	/* '0', '1', '2', '3', '4', '5', '6', '7', */
	    52,  53,  54,  55,  56,  57,  58,  59,

	/* '8', '9', ':', ';', '<', '=', '>', '?', */
	    60,  61, 255, 255, 255, 255, 255, 255,

	/* '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', */
	   255,   0,   1,  2,   3,   4,   5,    6,

	/* 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', */
	     7,   8,   9,  10,  11,  12,  13,  14,

	/* 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', */
	    15,  16,  17,  18,  19,  20,  21,  22,

	/* 'X', 'Y', 'Z', '[', '\', ']', '^', '_', */
	    23,  24,  25, 255, 255, 255, 255, 255,

	/* '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', */
	   255,  26,  27,  28,  29,  30,  31,  32,

	/* 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', */
	    33,  34,  35,  36,  37,  38,  39,  40,

	/* 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', */
	    41,  42,  43,  44,  45,  46,  47,  48,

	/* 'x', 'y', 'z', '{', '|', '}', '~', del, */
	    49,  50,  51, 255, 255, 255, 255, 255
};

unsigned int
base64_encode(const unsigned char *in, unsigned int inlen, char *out)
{
	int s;
	unsigned int i;
	unsigned int j;
	unsigned char c;
	unsigned char l;

	s = 0;
	l = 0;
	for (i = j = 0; i < inlen; i++) {
		c = in[i];

		switch (s) {
		case 0:
			s = 1;
			out[j++] = base64en[(c >> 2) & 0x3F];
			break;
		case 1:
			s = 2;
			out[j++] = base64en[((l & 0x3) << 4) | ((c >> 4) & 0xF)];
			break;
		case 2:
			s = 0;
			out[j++] = base64en[((l & 0xF) << 2) | ((c >> 6) & 0x3)];
			out[j++] = base64en[c & 0x3F];
			break;
		}
		l = c;
	}

	switch (s) {
	case 1:
		out[j++] = base64en[(l & 0x3) << 4];
		out[j++] = BASE64_PAD;
		out[j++] = BASE64_PAD;
		break;
	case 2:
		out[j++] = base64en[(l & 0xF) << 2];
		out[j++] = BASE64_PAD;
		break;
	}

	out[j] = 0;

	return j;
}

unsigned int
base64_decode(const char *in, unsigned int inlen, unsigned char *out)
{
	unsigned int i;
	unsigned int j;
	unsigned char c;

	if (inlen & 0x3) {
		return 0;
	}

	for (i = j = 0; i < inlen; i++) {
		if (in[i] == BASE64_PAD) {
			break;
		}
		if (in[i] < BASE64DE_FIRST || in[i] > BASE64DE_LAST) {
			return 0;
		}

		c = base64de[(unsigned char)in[i]];
		if (c == 255) {
			return 0;
		}

		switch (i & 0x3) {
		case 0:
			out[j] = (c << 2) & 0xFF;
			break;
		case 1:
			out[j++] |= (c >> 4) & 0x3;
			out[j] = (c & 0xF) << 4; 
			break;
		case 2:
			out[j++] |= (c >> 2) & 0xF;
			out[j] = (c & 0x3) << 6;
			break;
		case 3:
			out[j++] |= c;
			break;
		}
	}

	return j;
}

void add_module_base64(VM* vm){
    PyObject* mod = vm->new_module("base64");

    // b64encode
    vm->bind_static_method<1>(mod, "b64encode", [](VM* vm, ArgsView args){
        Bytes& b = CAST(Bytes&, args[0]);
        std::vector<char> out(b.size() * 2);
        int size = base64_encode((const unsigned char*)b.data(), b.size(), out.data());
        out.resize(size);
        return VAR(Bytes(std::move(out)));
    });

    // b64decode
    vm->bind_static_method<1>(mod, "b64decode", [](VM* vm, ArgsView args){
        Bytes& b = CAST(Bytes&, args[0]);
        std::vector<char> out(b.size());
        int size = base64_decode(b.data(), b.size(), (unsigned char*)out.data());
        out.resize(size);
        return VAR(Bytes(std::move(out)));
    });
}

} // namespace pkpy


namespace pkpy {

#define PY_CLASS(T, mod, name)                  \
    static Type _type(VM* vm) {                 \
        static const StrName __x0(#mod);        \
        static const StrName __x1(#name);       \
        return OBJ_GET(Type, vm->_modules[__x0]->attr(__x1));               \
    }                                                                       \
    static PyObject* register_class(VM* vm, PyObject* mod) {                \
        PyObject* type = vm->new_type_object(mod, #name, vm->tp_object);    \
        if(OBJ_NAME(mod) != #mod) {                                         \
            auto msg = fmt("register_class() failed: ", OBJ_NAME(mod), " != ", #mod); \
            throw std::runtime_error(msg);                                  \
        }                                                                   \
        T::_register(vm, mod, type);                                        \
        type->attr()._try_perfect_rehash();                                 \
        return type;                                                        \
    }                                                                       

#define VAR_T(T, ...) vm->heap.gcnew<T>(T::_type(vm), T(__VA_ARGS__))


struct VoidP{
    PY_CLASS(VoidP, c, void_p)

    void* ptr;
    VoidP(void* ptr): ptr(ptr){}

    static void _register(VM* vm, PyObject* mod, PyObject* type){
        vm->bind_static_method<1>(type, "__new__", CPP_NOT_IMPLEMENTED());

        vm->bind_static_method<1>(type, "__repr__", [](VM* vm, ArgsView args){
            VoidP& self = CAST(VoidP&, args[0]);
            std::stringstream ss;
            ss << "<void* at " << self.ptr << ">";
            return VAR(ss.str());
        });
    }
};

inline void add_module_c(VM* vm){
    PyObject* mod = vm->new_module("c");
    VoidP::register_class(vm, mod);
}

inline PyObject* py_var(VM* vm, void* p){
    return VAR_T(VoidP, p);
}

inline PyObject* py_var(VM* vm, char* p){
    return VAR_T(VoidP, p);
}
/***********************************************/

template<typename T>
struct _pointer {
    static constexpr int level = 0;
    using baseT = T;
};

template<typename T>
struct _pointer<T*> {
    static constexpr int level = _pointer<T>::level + 1;
    using baseT = typename _pointer<T>::baseT;
};

template<typename T>
struct pointer {
    static constexpr int level = _pointer<std::decay_t<T>>::level;
    using baseT = typename _pointer<std::decay_t<T>>::baseT;
};

template<typename T>
T py_pointer_cast(VM* vm, PyObject* var){
    static_assert(std::is_pointer_v<T>);
    VoidP& p = CAST(VoidP&, var);
    return reinterpret_cast<T>(p.ptr);
}
}   // namespace pkpy


#if __has_include("httplib.h")
#include "httplib.h"

namespace pkpy {

inline void add_module_requests(VM* vm){
    static StrName m_requests("requests");
    static StrName m_Response("Response");
    PyObject* mod = vm->new_module(m_requests);
    CodeObject_ code = vm->compile(kPythonLibs["requests"], "requests.py", EXEC_MODE);
    vm->_exec(code, mod);

    vm->bind_func<4>(mod, "_request", [](VM* vm, ArgsView args){
        Str method = CAST(Str&, args[0]);
        Str url = CAST(Str&, args[1]);
        PyObject* headers = args[2];            // a dict object
        PyObject* body = args[3];               // a bytes object

        if(url.index("http://") != 0){
            vm->ValueError("url must start with http://");
        }

        for(char c: url){
            switch(c){
                case '.':
                case '-':
                case '_':
                case '~':
                case ':':
                case '/':
                break;
                default:
                    if(!isalnum(c)){
                        vm->ValueError(fmt("invalid character in url: '", c, "'"));
                    }
            }
        }

        int slash = url.index("/", 7);
        Str path = "/";
        if(slash != -1){
            path = url.substr(slash);
            url = url.substr(0, slash);
            if(path.empty()) path = "/";
        }

        httplib::Client client(url.str());

        httplib::Headers h;
        if(headers != vm->None){
            List list = CAST(List&, headers);
            for(auto& item : list){
                Tuple t = CAST(Tuple&, item);
                Str key = CAST(Str&, t[0]);
                Str value = CAST(Str&, t[1]);
                h.emplace(key.str(), value.str());
            }
        }

        auto _to_resp = [=](const httplib::Result& res){
            return vm->call(
                vm->_modules[m_requests]->attr(m_Response),
                VAR(res->status),
                VAR(res->reason),
                VAR(Bytes(res->body))
            );
        };

        if(method == "GET"){
            httplib::Result res = client.Get(path.str(), h);
            return _to_resp(res);
        }else if(method == "POST"){
            Bytes b = CAST(Bytes&, body);
            httplib::Result res = client.Post(path.str(), h, b.data(), b.size(), "application/octet-stream");
            return _to_resp(res);
        }else if(method == "PUT"){
            Bytes b = CAST(Bytes&, body);
            httplib::Result res = client.Put(path.str(), h, b.data(), b.size(), "application/octet-stream");
            return _to_resp(res);
        }else if(method == "DELETE"){
            httplib::Result res = client.Delete(path.str(), h);
            return _to_resp(res);
        }else{
            vm->ValueError("invalid method");
        }
        UNREACHABLE();
    });
}

}   // namespace pkpy

#else

inline void add_module_requests(void* vm){ }

#endif


#if PK_ENABLE_OS

#include <fstream>
#include <filesystem>

namespace pkpy{

inline int _ = set_read_file_cwd([](const Str& name){
    std::filesystem::path path(name.sv());
    bool exists = std::filesystem::exists(path);
    if(!exists) return Bytes();
    std::ifstream ifs(path, std::ios::binary);
    std::vector<char> buffer(std::istreambuf_iterator<char>(ifs), {});
    ifs.close();
    return Bytes(std::move(buffer));
});

struct FileIO {
    PY_CLASS(FileIO, io, FileIO)

    Str file;
    Str mode;
    std::fstream _fs;

    bool is_text() const { return mode != "rb" && mode != "wb" && mode != "ab"; }

    FileIO(VM* vm, Str file, Str mode): file(file), mode(mode) {
        std::ios_base::openmode extra = static_cast<std::ios_base::openmode>(0);
        if(mode == "rb" || mode == "wb" || mode == "ab"){
            extra |= std::ios::binary;
        }
        if(mode == "rt" || mode == "r" || mode == "rb"){
            _fs.open(file.str(), std::ios::in | extra);
        }else if(mode == "wt" || mode == "w" || mode == "wb"){
            _fs.open(file.str(), std::ios::out | extra);
        }else if(mode == "at" || mode == "a" || mode == "ab"){
            _fs.open(file.str(), std::ios::app | extra);
        }else{
            vm->ValueError("invalid mode");
        }
        if(!_fs.is_open()) vm->IOError(strerror(errno));
    }

    static void _register(VM* vm, PyObject* mod, PyObject* type){
        vm->bind_static_method<2>(type, "__new__", [](VM* vm, ArgsView args){
            return VAR_T(FileIO, 
                vm, CAST(Str, args[0]), CAST(Str, args[1])
            );
        });

        vm->bind_method<0>(type, "read", [](VM* vm, ArgsView args){
            FileIO& io = CAST(FileIO&, args[0]);
            std::vector<char> buffer;
            while(true){
                char c = io._fs.get();
                if(io._fs.eof()) break;
                buffer.push_back(c);
            }
            Bytes b(std::move(buffer));
            if(io.is_text()) return VAR(Str(b.str()));
            return VAR(std::move(b));
        });

        vm->bind_method<1>(type, "write", [](VM* vm, ArgsView args){
            FileIO& io = CAST(FileIO&, args[0]);
            if(io.is_text()) io._fs << CAST(Str&, args[1]);
            else{
                Bytes& buffer = CAST(Bytes&, args[1]);
                io._fs.write(buffer.data(), buffer.size());
            }
            return vm->None;
        });

        vm->bind_method<0>(type, "close", [](VM* vm, ArgsView args){
            FileIO& io = CAST(FileIO&, args[0]);
            io._fs.close();
            return vm->None;
        });

        vm->bind_method<0>(type, "__exit__", [](VM* vm, ArgsView args){
            FileIO& io = CAST(FileIO&, args[0]);
            io._fs.close();
            return vm->None;
        });

        vm->bind_method<0>(type, "__enter__", CPP_LAMBDA(vm->None));
    }
};

inline void add_module_io(VM* vm){
    PyObject* mod = vm->new_module("io");
    FileIO::register_class(vm, mod);
    vm->bind_builtin_func<2>("open", [](VM* vm, ArgsView args){
        static StrName m_io("io");
        static StrName m_FileIO("FileIO");
        return vm->call(vm->_modules[m_io]->attr(m_FileIO), args[0], args[1]);
    });
}

inline void add_module_os(VM* vm){
    PyObject* mod = vm->new_module("os");
    PyObject* path_obj = vm->heap.gcnew<DummyInstance>(vm->tp_object, {});
    mod->attr().set("path", path_obj);
    
    // Working directory is shared by all VMs!!
    vm->bind_func<0>(mod, "getcwd", [](VM* vm, ArgsView args){
        return VAR(std::filesystem::current_path().string());
    });

    vm->bind_func<1>(mod, "chdir", [](VM* vm, ArgsView args){
        std::filesystem::path path(CAST(Str&, args[0]).sv());
        std::filesystem::current_path(path);
        return vm->None;
    });

    vm->bind_func<1>(mod, "listdir", [](VM* vm, ArgsView args){
        std::filesystem::path path(CAST(Str&, args[0]).sv());
        std::filesystem::directory_iterator di;
        try{
            di = std::filesystem::directory_iterator(path);
        }catch(std::filesystem::filesystem_error& e){
            std::string msg = e.what();
            auto pos = msg.find_last_of(":");
            if(pos != std::string::npos) msg = msg.substr(pos + 1);
            vm->IOError(Str(msg).lstrip());
        }
        List ret;
        for(auto& p: di) ret.push_back(VAR(p.path().filename().string()));
        return VAR(ret);
    });

    vm->bind_func<1>(mod, "remove", [](VM* vm, ArgsView args){
        std::filesystem::path path(CAST(Str&, args[0]).sv());
        bool ok = std::filesystem::remove(path);
        if(!ok) vm->IOError("operation failed");
        return vm->None;
    });

    vm->bind_func<1>(mod, "mkdir", [](VM* vm, ArgsView args){
        std::filesystem::path path(CAST(Str&, args[0]).sv());
        bool ok = std::filesystem::create_directory(path);
        if(!ok) vm->IOError("operation failed");
        return vm->None;
    });

    vm->bind_func<1>(mod, "rmdir", [](VM* vm, ArgsView args){
        std::filesystem::path path(CAST(Str&, args[0]).sv());
        bool ok = std::filesystem::remove(path);
        if(!ok) vm->IOError("operation failed");
        return vm->None;
    });

    vm->bind_func<-1>(path_obj, "join", [](VM* vm, ArgsView args){
        std::filesystem::path path;
        for(int i=0; i<args.size(); i++){
            path /= CAST(Str&, args[i]).sv();
        }
        return VAR(path.string());
    });

    vm->bind_func<1>(path_obj, "exists", [](VM* vm, ArgsView args){
        std::filesystem::path path(CAST(Str&, args[0]).sv());
        bool exists = std::filesystem::exists(path);
        return VAR(exists);
    });

    vm->bind_func<1>(path_obj, "basename", [](VM* vm, ArgsView args){
        std::filesystem::path path(CAST(Str&, args[0]).sv());
        return VAR(path.filename().string());
    });
}

} // namespace pkpy


#else

namespace pkpy{
inline void add_module_io(void* vm){}
inline void add_module_os(void* vm){}
} // namespace pkpy

#endif


namespace pkpy {

inline CodeObject_ VM::compile(Str source, Str filename, CompileMode mode, bool unknown_global_scope) {
    Compiler compiler(this, source, filename, mode, unknown_global_scope);
    try{
        return compiler.compile();
    }catch(Exception& e){
#if DEBUG_FULL_EXCEPTION
        std::cerr << e.summary() << std::endl;
#endif
        _error(e);
        return nullptr;
    }
}

#define BIND_NUM_ARITH_OPT(name, op)                                                                    \
    _vm->_bind_methods<1>({"int","float"}, #name, [](VM* vm, ArgsView args){                               \
        if(is_both_int(args[0], args[1])){                                                              \
            return VAR(_CAST(i64, args[0]) op _CAST(i64, args[1]));                                     \
        }else{                                                                                          \
            return VAR(vm->num_to_float(args[0]) op vm->num_to_float(args[1]));                         \
        }                                                                                               \
    });

#define BIND_NUM_LOGICAL_OPT(name, op, is_eq)                                                           \
    _vm->_bind_methods<1>({"int","float"}, #name, [](VM* vm, ArgsView args){                               \
        if(is_both_int(args[0], args[1]))                                                               \
            return VAR(_CAST(i64, args[0]) op _CAST(i64, args[1]));                                     \
        if(!is_both_int_or_float(args[0], args[1])){                                                    \
            if constexpr(is_eq) return VAR(args[0] op args[1]);                                         \
            vm->TypeError("unsupported operand type(s) for " #op );                                     \
        }                                                                                               \
        return VAR(vm->num_to_float(args[0]) op vm->num_to_float(args[1]));                             \
    });
    

inline void init_builtins(VM* _vm) {
    BIND_NUM_ARITH_OPT(__add__, +)
    BIND_NUM_ARITH_OPT(__sub__, -)
    BIND_NUM_ARITH_OPT(__mul__, *)

    BIND_NUM_LOGICAL_OPT(__lt__, <, false)
    BIND_NUM_LOGICAL_OPT(__le__, <=, false)
    BIND_NUM_LOGICAL_OPT(__gt__, >, false)
    BIND_NUM_LOGICAL_OPT(__ge__, >=, false)
    BIND_NUM_LOGICAL_OPT(__eq__, ==, true)
    BIND_NUM_LOGICAL_OPT(__ne__, !=, true)

#undef BIND_NUM_ARITH_OPT
#undef BIND_NUM_LOGICAL_OPT

    _vm->bind_builtin_func<1>("__sys_stdout_write", [](VM* vm, ArgsView args) {
        (*vm->_stdout) << CAST(Str&, args[0]);
        return vm->None;
    });

    _vm->bind_builtin_func<2>("super", [](VM* vm, ArgsView args) {
        vm->check_type(args[0], vm->tp_type);
        Type type = OBJ_GET(Type, args[0]);
        if(!vm->isinstance(args[1], type)){
            Str _0 = obj_type_name(vm, OBJ_GET(Type, vm->_t(args[1])));
            Str _1 = obj_type_name(vm, type);
            vm->TypeError("super(): " + _0.escape() + " is not an instance of " + _1.escape());
        }
        Type base = vm->_all_types[type].base;
        return vm->heap.gcnew(vm->tp_super, Super(args[1], base));
    });

    _vm->bind_builtin_func<2>("isinstance", [](VM* vm, ArgsView args) {
        vm->check_type(args[1], vm->tp_type);
        Type type = OBJ_GET(Type, args[1]);
        return VAR(vm->isinstance(args[0], type));
    });

    _vm->bind_builtin_func<1>("id", [](VM* vm, ArgsView args) {
        PyObject* obj = args[0];
        if(is_tagged(obj)) return VAR((i64)0);
        return VAR(BITS(obj));
    });

    _vm->bind_builtin_func<2>("divmod", [](VM* vm, ArgsView args) {
        i64 lhs = CAST(i64, args[0]);
        i64 rhs = CAST(i64, args[1]);
        if(rhs == 0) vm->ZeroDivisionError();
        return VAR(Tuple({VAR(lhs/rhs), VAR(lhs%rhs)}));
    });

    _vm->bind_builtin_func<1>("eval", [](VM* vm, ArgsView args) {
        CodeObject_ code = vm->compile(CAST(Str&, args[0]), "<eval>", EVAL_MODE, true);
        FrameId frame = vm->top_frame();
        return vm->_exec(code.get(), frame->_module, frame->_callable, frame->_locals);
    });

    _vm->bind_builtin_func<1>("exec", [](VM* vm, ArgsView args) {
        CodeObject_ code = vm->compile(CAST(Str&, args[0]), "<exec>", EXEC_MODE, true);
        FrameId frame = vm->top_frame();
        vm->_exec(code.get(), frame->_module, frame->_callable, frame->_locals);
        return vm->None;
    });

    _vm->bind_builtin_func<-1>("exit", [](VM* vm, ArgsView args) {
        if(args.size() == 0) std::exit(0);
        else if(args.size() == 1) std::exit(CAST(int, args[0]));
        else vm->TypeError("exit() takes at most 1 argument");
        return vm->None;
    });

    _vm->bind_builtin_func<1>("repr", CPP_LAMBDA(vm->asRepr(args[0])));
    _vm->bind_builtin_func<1>("len", [](VM* vm, ArgsView args){
        return vm->call_method(args[0], __len__);
    });

    _vm->bind_builtin_func<1>("hash", [](VM* vm, ArgsView args){
        i64 value = vm->hash(args[0]);
        if(((value << 2) >> 2) != value) value >>= 2;
        return VAR(value);
    });

    _vm->bind_builtin_func<1>("chr", [](VM* vm, ArgsView args) {
        i64 i = CAST(i64, args[0]);
        if (i < 0 || i > 128) vm->ValueError("chr() arg not in range(128)");
        return VAR(std::string(1, (char)i));
    });

    _vm->bind_builtin_func<1>("ord", [](VM* vm, ArgsView args) {
        const Str& s = CAST(Str&, args[0]);
        if (s.length()!=1) vm->TypeError("ord() expected an ASCII character");
        return VAR((i64)(s[0]));
    });

    _vm->bind_builtin_func<2>("hasattr", [](VM* vm, ArgsView args) {
        return VAR(vm->getattr(args[0], CAST(Str&, args[1]), false) != nullptr);
    });

    _vm->bind_builtin_func<3>("setattr", [](VM* vm, ArgsView args) {
        vm->setattr(args[0], CAST(Str&, args[1]), args[2]);
        return vm->None;
    });

    _vm->bind_builtin_func<2>("getattr", [](VM* vm, ArgsView args) {
        const Str& name = CAST(Str&, args[1]);
        return vm->getattr(args[0], name);
    });

    _vm->bind_builtin_func<1>("hex", [](VM* vm, ArgsView args) {
        std::stringstream ss;
        ss << std::hex << CAST(i64, args[0]);
        return VAR("0x" + ss.str());
    });

    _vm->bind_builtin_func<1>("iter", [](VM* vm, ArgsView args) {
        return vm->asIter(args[0]);
    });

    _vm->bind_builtin_func<1>("next", [](VM* vm, ArgsView args) {
        return vm->PyIterNext(args[0]);
    });

    _vm->bind_builtin_func<1>("dir", [](VM* vm, ArgsView args) {
        std::set<StrName> names;
        if(args[0]->is_attr_valid()){
            std::vector<StrName> keys = args[0]->attr().keys();
            names.insert(keys.begin(), keys.end());
        }
        const NameDict& t_attr = vm->_t(args[0])->attr();
        std::vector<StrName> keys = t_attr.keys();
        names.insert(keys.begin(), keys.end());
        List ret;
        for (StrName name : names) ret.push_back(VAR(name.sv()));
        return VAR(std::move(ret));
    });

    _vm->bind_method<0>("object", "__repr__", [](VM* vm, ArgsView args) {
        PyObject* self = args[0];
        if(is_tagged(self)) self = nullptr;
        std::stringstream ss;
        ss << "<" << OBJ_NAME(vm->_t(self)) << " object at " << std::hex << self << ">";
        return VAR(ss.str());
    });

    _vm->bind_method<1>("object", "__eq__", CPP_LAMBDA(VAR(args[0] == args[1])));
    _vm->bind_method<1>("object", "__ne__", CPP_LAMBDA(VAR(args[0] != args[1])));

    _vm->bind_static_method<1>("type", "__new__", CPP_LAMBDA(vm->_t(args[0])));
    _vm->bind_static_method<-1>("range", "__new__", [](VM* vm, ArgsView args) {
        Range r;
        switch (args.size()) {
            case 1: r.stop = CAST(i64, args[0]); break;
            case 2: r.start = CAST(i64, args[0]); r.stop = CAST(i64, args[1]); break;
            case 3: r.start = CAST(i64, args[0]); r.stop = CAST(i64, args[1]); r.step = CAST(i64, args[2]); break;
            default: vm->TypeError("expected 1-3 arguments, but got " + std::to_string(args.size()));
        }
        return VAR(r);
    });

    _vm->bind_method<0>("range", "__iter__", CPP_LAMBDA(
        vm->PyIter(RangeIter(vm, args[0]))
    ));

    _vm->bind_method<0>("NoneType", "__repr__", CPP_LAMBDA(VAR("None")));
    _vm->bind_method<0>("NoneType", "__json__", CPP_LAMBDA(VAR("null")));

    _vm->_bind_methods<1>({"int", "float"}, "__truediv__", [](VM* vm, ArgsView args) {
        f64 rhs = vm->num_to_float(args[1]);
        if (rhs == 0) vm->ZeroDivisionError();
        return VAR(vm->num_to_float(args[0]) / rhs);
    });

    _vm->_bind_methods<1>({"int", "float"}, "__pow__", [](VM* vm, ArgsView args) {
        if(is_both_int(args[0], args[1])){
            i64 lhs = _CAST(i64, args[0]);
            i64 rhs = _CAST(i64, args[1]);
            bool flag = false;
            if(rhs < 0) {flag = true; rhs = -rhs;}
            i64 ret = 1;
            while(rhs){
                if(rhs & 1) ret *= lhs;
                lhs *= lhs;
                rhs >>= 1;
            }
            if(flag) return VAR((f64)(1.0 / ret));
            return VAR(ret);
        }else{
            return VAR((f64)std::pow(vm->num_to_float(args[0]), vm->num_to_float(args[1])));
        }
    });

    /************ PyInt ************/
    _vm->bind_static_method<1>("int", "__new__", [](VM* vm, ArgsView args) {
        if (is_type(args[0], vm->tp_int)) return args[0];
        if (is_type(args[0], vm->tp_float)) return VAR((i64)CAST(f64, args[0]));
        if (is_type(args[0], vm->tp_bool)) return VAR(_CAST(bool, args[0]) ? 1 : 0);
        if (is_type(args[0], vm->tp_str)) {
            const Str& s = CAST(Str&, args[0]);
            try{
                size_t parsed = 0;
                i64 val = Number::stoi(s.str(), &parsed, 10);
                if(parsed != s.length()) throw std::invalid_argument("<?>");
                return VAR(val);
            }catch(std::invalid_argument&){
                vm->ValueError("invalid literal for int(): " + s.escape());
            }
        }
        vm->TypeError("int() argument must be a int, float, bool or str");
        return vm->None;
    });

    _vm->bind_method<1>("int", "__floordiv__", [](VM* vm, ArgsView args) {
        i64 rhs = CAST(i64, args[1]);
        if(rhs == 0) vm->ZeroDivisionError();
        return VAR(CAST(i64, args[0]) / rhs);
    });

    _vm->bind_method<1>("int", "__mod__", [](VM* vm, ArgsView args) {
        i64 rhs = CAST(i64, args[1]);
        if(rhs == 0) vm->ZeroDivisionError();
        return VAR(CAST(i64, args[0]) % rhs);
    });

    _vm->bind_method<0>("int", "__repr__", CPP_LAMBDA(VAR(std::to_string(CAST(i64, args[0])))));
    _vm->bind_method<0>("int", "__json__", CPP_LAMBDA(VAR(std::to_string(CAST(i64, args[0])))));

#define INT_BITWISE_OP(name,op) \
    _vm->bind_method<1>("int", #name, CPP_LAMBDA(VAR(CAST(i64, args[0]) op CAST(i64, args[1]))));

    INT_BITWISE_OP(__lshift__, <<)
    INT_BITWISE_OP(__rshift__, >>)
    INT_BITWISE_OP(__and__, &)
    INT_BITWISE_OP(__or__, |)
    INT_BITWISE_OP(__xor__, ^)

#undef INT_BITWISE_OP

    /************ PyFloat ************/
    _vm->bind_static_method<1>("float", "__new__", [](VM* vm, ArgsView args) {
        if (is_type(args[0], vm->tp_int)) return VAR((f64)CAST(i64, args[0]));
        if (is_type(args[0], vm->tp_float)) return args[0];
        if (is_type(args[0], vm->tp_bool)) return VAR(_CAST(bool, args[0]) ? 1.0 : 0.0);
        if (is_type(args[0], vm->tp_str)) {
            const Str& s = CAST(Str&, args[0]);
            if(s == "inf") return VAR(INFINITY);
            if(s == "-inf") return VAR(-INFINITY);
            try{
                f64 val = Number::stof(s.str());
                return VAR(val);
            }catch(std::invalid_argument&){
                vm->ValueError("invalid literal for float(): '" + s + "'");
            }
        }
        vm->TypeError("float() argument must be a int, float, bool or str");
        return vm->None;
    });

    _vm->bind_method<0>("float", "__repr__", [](VM* vm, ArgsView args) {
        f64 val = CAST(f64, args[0]);
        if(std::isinf(val) || std::isnan(val)) return VAR(std::to_string(val));
        std::stringstream ss;
        ss << std::setprecision(std::numeric_limits<f64>::max_digits10-1-2) << val;
        std::string s = ss.str();
        if(std::all_of(s.begin()+1, s.end(), isdigit)) s += ".0";
        return VAR(s);
    });

    _vm->bind_method<0>("float", "__json__", [](VM* vm, ArgsView args) {
        f64 val = CAST(f64, args[0]);
        if(std::isinf(val) || std::isnan(val)) vm->ValueError("cannot jsonify 'nan' or 'inf'");
        return VAR(std::to_string(val));
    });

    /************ PyString ************/
    _vm->bind_static_method<1>("str", "__new__", CPP_LAMBDA(vm->asStr(args[0])));

    _vm->bind_method<1>("str", "__add__", [](VM* vm, ArgsView args) {
        const Str& lhs = CAST(Str&, args[0]);
        const Str& rhs = CAST(Str&, args[1]);
        return VAR(lhs + rhs);
    });

    _vm->bind_method<0>("str", "__len__", [](VM* vm, ArgsView args) {
        const Str& self = CAST(Str&, args[0]);
        return VAR(self.u8_length());
    });

    _vm->bind_method<1>("str", "__contains__", [](VM* vm, ArgsView args) {
        const Str& self = CAST(Str&, args[0]);
        const Str& other = CAST(Str&, args[1]);
        return VAR(self.index(other) != -1);
    });

    _vm->bind_method<0>("str", "__str__", CPP_LAMBDA(args[0]));
    _vm->bind_method<0>("str", "__iter__", CPP_LAMBDA(vm->PyIter(StringIter(vm, args[0]))));

    _vm->bind_method<0>("str", "__repr__", [](VM* vm, ArgsView args) {
        const Str& self = CAST(Str&, args[0]);
        return VAR(self.escape());
    });

    _vm->bind_method<0>("str", "__json__", [](VM* vm, ArgsView args) {
        const Str& self = CAST(Str&, args[0]);
        return VAR(self.escape(false));
    });

    _vm->bind_method<1>("str", "__eq__", [](VM* vm, ArgsView args) {
        const Str& self = CAST(Str&, args[0]);
        if(!is_type(args[1], vm->tp_str)) return VAR(false);
        return VAR(self == CAST(Str&, args[1]));
    });

    _vm->bind_method<1>("str", "__ne__", [](VM* vm, ArgsView args) {
        const Str& self = CAST(Str&, args[0]);
        if(!is_type(args[1], vm->tp_str)) return VAR(true);
        return VAR(self != CAST(Str&, args[1]));
    });

    _vm->bind_method<1>("str", "__getitem__", [](VM* vm, ArgsView args) {
        const Str& self (CAST(Str&, args[0]));

        if(is_type(args[1], vm->tp_slice)){
            const Slice& s = _CAST(Slice&, args[1]);
            int start, stop, step;
            vm->parse_int_slice(s, self.u8_length(), start, stop, step);
            return VAR(self.u8_slice(start, stop, step));
        }

        int index = CAST(int, args[1]);
        index = vm->normalized_index(index, self.u8_length());
        return VAR(self.u8_getitem(index));
    });

    _vm->bind_method<1>("str", "__gt__", [](VM* vm, ArgsView args) {
        const Str& self (CAST(Str&, args[0]));
        const Str& obj (CAST(Str&, args[1]));
        return VAR(self > obj);
    });

    _vm->bind_method<1>("str", "__lt__", [](VM* vm, ArgsView args) {
        const Str& self (CAST(Str&, args[0]));
        const Str& obj (CAST(Str&, args[1]));
        return VAR(self < obj);
    });

    _vm->bind_method<-1>("str", "replace", [](VM* vm, ArgsView args) {
        if(args.size() != 1+2 && args.size() != 1+3) vm->TypeError("replace() takes 2 or 3 arguments");
        const Str& self = CAST(Str&, args[0]);
        const Str& old = CAST(Str&, args[1]);
        const Str& new_ = CAST(Str&, args[2]);
        int count = args.size()==1+3 ? CAST(int, args[3]) : -1;
        return VAR(self.replace(old, new_, count));
    });

    _vm->bind_method<1>("str", "index", [](VM* vm, ArgsView args) {
        const Str& self = CAST(Str&, args[0]);
        const Str& sub = CAST(Str&, args[1]);
        int index = self.index(sub);
        if(index == -1) vm->ValueError("substring not found");
        return VAR(index);
    });

    _vm->bind_method<1>("str", "startswith", [](VM* vm, ArgsView args) {
        const Str& self = CAST(Str&, args[0]);
        const Str& prefix = CAST(Str&, args[1]);
        return VAR(self.index(prefix) == 0);
    });

    _vm->bind_method<1>("str", "endswith", [](VM* vm, ArgsView args) {
        const Str& self = CAST(Str&, args[0]);
        const Str& suffix = CAST(Str&, args[1]);
        int offset = self.length() - suffix.length();
        if(offset < 0) return vm->False;
        bool ok = memcmp(self.data+offset, suffix.data, suffix.length()) == 0;
        return VAR(ok);
    });

    _vm->bind_method<0>("str", "encode", [](VM* vm, ArgsView args) {
        const Str& self = CAST(Str&, args[0]);
        std::vector<char> buffer(self.length());
        memcpy(buffer.data(), self.data, self.length());
        return VAR(Bytes(std::move(buffer)));
    });

    _vm->bind_method<1>("str", "join", [](VM* vm, ArgsView args) {
        const Str& self = CAST(Str&, args[0]);
        FastStrStream ss;
        PyObject* obj = vm->asList(args[1]);
        const List& list = CAST(List&, obj);
        for (int i = 0; i < list.size(); ++i) {
            if (i > 0) ss << self;
            ss << CAST(Str&, list[i]);
        }
        return VAR(ss.str());
    });

    /************ PyList ************/
    _vm->bind_method<1>("list", "append", [](VM* vm, ArgsView args) {
        List& self = CAST(List&, args[0]);
        self.push_back(args[1]);
        return vm->None;
    });

    _vm->bind_method<1>("list", "extend", [](VM* vm, ArgsView args) {
        List& self = CAST(List&, args[0]);
        PyObject* obj = vm->asList(args[1]);
        const List& list = CAST(List&, obj);
        self.extend(list);
        return vm->None;
    });

    _vm->bind_method<0>("list", "reverse", [](VM* vm, ArgsView args) {
        List& self = CAST(List&, args[0]);
        std::reverse(self.begin(), self.end());
        return vm->None;
    });

    _vm->bind_method<1>("list", "__mul__", [](VM* vm, ArgsView args) {
        const List& self = CAST(List&, args[0]);
        int n = CAST(int, args[1]);
        List result;
        result.reserve(self.size() * n);
        for(int i = 0; i < n; i++) result.extend(self);
        return VAR(std::move(result));
    });

    _vm->bind_method<2>("list", "insert", [](VM* vm, ArgsView args) {
        List& self = CAST(List&, args[0]);
        int index = CAST(int, args[1]);
        if(index < 0) index += self.size();
        if(index < 0) index = 0;
        if(index > self.size()) index = self.size();
        self.insert(index, args[2]);
        return vm->None;
    });

    _vm->bind_method<0>("list", "clear", [](VM* vm, ArgsView args) {
        CAST(List&, args[0]).clear();
        return vm->None;
    });

    _vm->bind_method<0>("list", "copy", CPP_LAMBDA(VAR(CAST(List, args[0]))));

    _vm->bind_method<1>("list", "__add__", [](VM* vm, ArgsView args) {
        const List& self = CAST(List&, args[0]);
        const List& other = CAST(List&, args[1]);
        List new_list(self);    // copy construct
        new_list.extend(other);
        return VAR(std::move(new_list));
    });

    _vm->bind_method<0>("list", "__len__", [](VM* vm, ArgsView args) {
        const List& self = CAST(List&, args[0]);
        return VAR(self.size());
    });

    _vm->bind_method<0>("list", "__iter__", [](VM* vm, ArgsView args) {
        return vm->PyIter(ArrayIter<List>(vm, args[0]));
    });

    _vm->bind_method<1>("list", "__getitem__", [](VM* vm, ArgsView args) {
        const List& self = CAST(List&, args[0]);

        if(is_type(args[1], vm->tp_slice)){
            const Slice& s = _CAST(Slice&, args[1]);
            int start, stop, step;
            vm->parse_int_slice(s, self.size(), start, stop, step);
            List new_list;
            for(int i=start; step>0?i<stop:i>stop; i+=step) new_list.push_back(self[i]);
            return VAR(std::move(new_list));
        }

        int index = CAST(int, args[1]);
        index = vm->normalized_index(index, self.size());
        return self[index];
    });

    _vm->bind_method<2>("list", "__setitem__", [](VM* vm, ArgsView args) {
        List& self = CAST(List&, args[0]);
        int index = CAST(int, args[1]);
        index = vm->normalized_index(index, self.size());
        self[index] = args[2];
        return vm->None;
    });

    _vm->bind_method<1>("list", "__delitem__", [](VM* vm, ArgsView args) {
        List& self = CAST(List&, args[0]);
        int index = CAST(int, args[1]);
        index = vm->normalized_index(index, self.size());
        self.erase(index);
        return vm->None;
    });

    /************ PyTuple ************/
    _vm->bind_static_method<1>("tuple", "__new__", [](VM* vm, ArgsView args) {
        List list = CAST(List, vm->asList(args[0]));
        return VAR(Tuple(std::move(list)));
    });

    _vm->bind_method<0>("tuple", "__iter__", [](VM* vm, ArgsView args) {
        return vm->PyIter(ArrayIter<Tuple>(vm, args[0]));
    });

    _vm->bind_method<1>("tuple", "__getitem__", [](VM* vm, ArgsView args) {
        const Tuple& self = CAST(Tuple&, args[0]);

        if(is_type(args[1], vm->tp_slice)){
            const Slice& s = _CAST(Slice&, args[1]);
            int start, stop, step;
            vm->parse_int_slice(s, self.size(), start, stop, step);
            List new_list;
            for(int i=start; step>0?i<stop:i>stop; i+=step) new_list.push_back(self[i]);
            return VAR(Tuple(std::move(new_list)));
        }

        int index = CAST(int, args[1]);
        index = vm->normalized_index(index, self.size());
        return self[index];
    });

    _vm->bind_method<0>("tuple", "__len__", [](VM* vm, ArgsView args) {
        const Tuple& self = CAST(Tuple&, args[0]);
        return VAR(self.size());
    });

    /************ bool ************/
    _vm->bind_static_method<1>("bool", "__new__", CPP_LAMBDA(VAR(vm->asBool(args[0]))));

    _vm->bind_method<0>("bool", "__repr__", [](VM* vm, ArgsView args) {
        bool val = CAST(bool, args[0]);
        return VAR(val ? "True" : "False");
    });

    _vm->bind_method<0>("bool", "__json__", [](VM* vm, ArgsView args) {
        bool val = CAST(bool, args[0]);
        return VAR(val ? "true" : "false");
    });

    _vm->bind_method<1>("bool", "__xor__", [](VM* vm, ArgsView args) {
        bool self = CAST(bool, args[0]);
        bool other = CAST(bool, args[1]);
        return VAR(self ^ other);
    });

    _vm->bind_method<0>("ellipsis", "__repr__", CPP_LAMBDA(VAR("Ellipsis")));

    /************ bytes ************/
    _vm->bind_static_method<1>("bytes", "__new__", [](VM* vm, ArgsView args){
        List& list = CAST(List&, args[0]);
        std::vector<char> buffer(list.size());
        for(int i=0; i<list.size(); i++){
            i64 b = CAST(i64, list[i]);
            if(b<0 || b>255) vm->ValueError("byte must be in range[0, 256)");
            buffer[i] = (char)b;
        }
        return VAR(Bytes(std::move(buffer)));
    });

    _vm->bind_method<1>("bytes", "__getitem__", [](VM* vm, ArgsView args) {
        const Bytes& self = CAST(Bytes&, args[0]);
        int index = CAST(int, args[1]);
        index = vm->normalized_index(index, self.size());
        return VAR(self[index]);
    });

    _vm->bind_method<0>("bytes", "__repr__", [](VM* vm, ArgsView args) {
        const Bytes& self = CAST(Bytes&, args[0]);
        std::stringstream ss;
        ss << "b'";
        for(int i=0; i<self.size(); i++){
            ss << "\\x" << std::hex << std::setw(2) << std::setfill('0') << self[i];
        }
        ss << "'";
        return VAR(ss.str());
    });

    _vm->bind_method<0>("bytes", "__len__", [](VM* vm, ArgsView args) {
        const Bytes& self = CAST(Bytes&, args[0]);
        return VAR(self.size());
    });

    _vm->bind_method<0>("bytes", "decode", [](VM* vm, ArgsView args) {
        const Bytes& self = CAST(Bytes&, args[0]);
        // TODO: check encoding is utf-8
        return VAR(Str(self.str()));
    });

    _vm->bind_method<1>("bytes", "__eq__", [](VM* vm, ArgsView args) {
        const Bytes& self = CAST(Bytes&, args[0]);
        if(!is_type(args[1], vm->tp_bytes)) return VAR(false);
        const Bytes& other = CAST(Bytes&, args[1]);
        return VAR(self == other);
    });

    _vm->bind_method<1>("bytes", "__ne__", [](VM* vm, ArgsView args) {
        const Bytes& self = CAST(Bytes&, args[0]);
        if(!is_type(args[1], vm->tp_bytes)) return VAR(true);
        const Bytes& other = CAST(Bytes&, args[1]);
        return VAR(self != other);
    });

    /************ slice ************/
    _vm->bind_static_method<3>("slice", "__new__", [](VM* vm, ArgsView args) {
        return VAR(Slice(args[0], args[1], args[2]));
    });

    _vm->bind_method<0>("slice", "__repr__", [](VM* vm, ArgsView args) {
        const Slice& self = CAST(Slice&, args[0]);
        std::stringstream ss;
        ss << "slice(";
        ss << CAST(Str, vm->asRepr(self.start)) << ", ";
        ss << CAST(Str, vm->asRepr(self.stop)) << ", ";
        ss << CAST(Str, vm->asRepr(self.step)) << ")";
        return VAR(ss.str());
    });

    /************ MappingProxy ************/
    _vm->bind_method<0>("mappingproxy", "keys", [](VM* vm, ArgsView args) {
        MappingProxy& self = CAST(MappingProxy&, args[0]);
        List keys;
        for(StrName name : self.attr().keys()) keys.push_back(VAR(name.sv()));
        return VAR(std::move(keys));
    });

    _vm->bind_method<0>("mappingproxy", "values", [](VM* vm, ArgsView args) {
        MappingProxy& self = CAST(MappingProxy&, args[0]);
        List values;
        for(auto& item : self.attr().items()) values.push_back(item.second);
        return VAR(std::move(values));
    });

    _vm->bind_method<0>("mappingproxy", "items", [](VM* vm, ArgsView args) {
        MappingProxy& self = CAST(MappingProxy&, args[0]);
        List items;
        for(auto& item : self.attr().items()){
            PyObject* t = VAR(Tuple({VAR(item.first.sv()), item.second}));
            items.push_back(std::move(t));
        }
        return VAR(std::move(items));
    });

    _vm->bind_method<0>("mappingproxy", "__len__", [](VM* vm, ArgsView args) {
        MappingProxy& self = CAST(MappingProxy&, args[0]);
        return VAR(self.attr().size());
    });

    _vm->bind_method<1>("mappingproxy", "__getitem__", [](VM* vm, ArgsView args) {
        MappingProxy& self = CAST(MappingProxy&, args[0]);
        StrName key = CAST(Str&, args[1]);
        PyObject* ret = self.attr().try_get(key);
        if(ret == nullptr) vm->AttributeError(key.sv());
        return ret;
    });

    _vm->bind_method<0>("mappingproxy", "__repr__", [](VM* vm, ArgsView args) {
        MappingProxy& self = CAST(MappingProxy&, args[0]);
        std::stringstream ss;
        ss << "mappingproxy({";
        bool first = true;
        for(auto& item : self.attr().items()){
            if(!first) ss << ", ";
            first = false;
            ss << item.first.escape() << ": " << CAST(Str, vm->asRepr(item.second));
        }
        ss << "})";
        return VAR(ss.str());
    });

    _vm->bind_method<1>("mappingproxy", "__contains__", [](VM* vm, ArgsView args) {
        MappingProxy& self = CAST(MappingProxy&, args[0]);
        StrName key = CAST(Str&, args[1]);
        return VAR(self.attr().contains(key));
    });
}

#ifdef _WIN32
#define __EXPORT __declspec(dllexport) inline
#elif __APPLE__
#define __EXPORT __attribute__((visibility("default"))) __attribute__((used)) inline
#elif __EMSCRIPTEN__
#include <emscripten.h>
#define __EXPORT EMSCRIPTEN_KEEPALIVE inline
#else
#define __EXPORT inline
#endif

inline void add_module_time(VM* vm){
    PyObject* mod = vm->new_module("time");
    vm->bind_func<0>(mod, "time", [](VM* vm, ArgsView args) {
        auto now = std::chrono::high_resolution_clock::now();
        return VAR(std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count() / 1000000.0);
    });
}

inline void add_module_sys(VM* vm){
    PyObject* mod = vm->new_module("sys");
    vm->setattr(mod, "version", VAR(PK_VERSION));
}

inline void add_module_json(VM* vm){
    PyObject* mod = vm->new_module("json");
    vm->bind_func<1>(mod, "loads", [](VM* vm, ArgsView args) {
        const Str& expr = CAST(Str&, args[0]);
        CodeObject_ code = vm->compile(expr, "<json>", JSON_MODE);
        return vm->_exec(code, vm->top_frame()->_module);
    });

    vm->bind_func<1>(mod, "dumps", CPP_LAMBDA(vm->call_method(args[0], __json__)));
}

inline void add_module_math(VM* vm){
    PyObject* mod = vm->new_module("math");
    mod->attr().set("pi", VAR(3.1415926535897932384));
    mod->attr().set("e" , VAR(2.7182818284590452354));

    vm->bind_func<1>(mod, "log", CPP_LAMBDA(VAR(std::log(vm->num_to_float(args[0])))));
    vm->bind_func<1>(mod, "log10", CPP_LAMBDA(VAR(std::log10(vm->num_to_float(args[0])))));
    vm->bind_func<1>(mod, "log2", CPP_LAMBDA(VAR(std::log2(vm->num_to_float(args[0])))));
    vm->bind_func<1>(mod, "sin", CPP_LAMBDA(VAR(std::sin(vm->num_to_float(args[0])))));
    vm->bind_func<1>(mod, "cos", CPP_LAMBDA(VAR(std::cos(vm->num_to_float(args[0])))));
    vm->bind_func<1>(mod, "tan", CPP_LAMBDA(VAR(std::tan(vm->num_to_float(args[0])))));
    vm->bind_func<1>(mod, "isnan", CPP_LAMBDA(VAR(std::isnan(vm->num_to_float(args[0])))));
    vm->bind_func<1>(mod, "isinf", CPP_LAMBDA(VAR(std::isinf(vm->num_to_float(args[0])))));
    vm->bind_func<1>(mod, "fabs", CPP_LAMBDA(VAR(std::fabs(vm->num_to_float(args[0])))));
    vm->bind_func<1>(mod, "floor", CPP_LAMBDA(VAR((i64)std::floor(vm->num_to_float(args[0])))));
    vm->bind_func<1>(mod, "ceil", CPP_LAMBDA(VAR((i64)std::ceil(vm->num_to_float(args[0])))));
    vm->bind_func<1>(mod, "sqrt", CPP_LAMBDA(VAR(std::sqrt(vm->num_to_float(args[0])))));
    vm->bind_func<2>(mod, "gcd", [](VM* vm, ArgsView args) {
        i64 a = CAST(i64, args[0]);
        i64 b = CAST(i64, args[1]);
        if(a < 0) a = -a;
        if(b < 0) b = -b;
        while(b != 0){
            i64 t = b;
            b = a % b;
            a = t;
        }
        return VAR(a);
    });
}

inline void add_module_dis(VM* vm){
    PyObject* mod = vm->new_module("dis");
    vm->bind_func<1>(mod, "dis", [](VM* vm, ArgsView args) {
        PyObject* f = args[0];
        if(is_type(f, vm->tp_bound_method)) f = CAST(BoundMethod, args[0]).func;
        CodeObject_ code = CAST(Function&, f).decl->code;
        (*vm->_stdout) << vm->disassemble(code);
        return vm->None;
    });
}

struct ReMatch {
    PY_CLASS(ReMatch, re, Match)

    i64 start;
    i64 end;
    std::cmatch m;
    ReMatch(i64 start, i64 end, std::cmatch m) : start(start), end(end), m(m) {}

    static void _register(VM* vm, PyObject* mod, PyObject* type){
        vm->bind_method<-1>(type, "__init__", CPP_NOT_IMPLEMENTED());
        vm->bind_method<0>(type, "start", CPP_LAMBDA(VAR(CAST(ReMatch&, args[0]).start)));
        vm->bind_method<0>(type, "end", CPP_LAMBDA(VAR(CAST(ReMatch&, args[0]).end)));

        vm->bind_method<0>(type, "span", [](VM* vm, ArgsView args) {
            auto& self = CAST(ReMatch&, args[0]);
            return VAR(Tuple({VAR(self.start), VAR(self.end)}));
        });

        vm->bind_method<1>(type, "group", [](VM* vm, ArgsView args) {
            auto& self = CAST(ReMatch&, args[0]);
            int index = CAST(int, args[1]);
            index = vm->normalized_index(index, self.m.size());
            return VAR(self.m[index].str());
        });
    }
};

inline PyObject* _regex_search(const Str& pattern, const Str& string, bool from_start, VM* vm){
    std::regex re(pattern.begin(), pattern.end());
    std::cmatch m;
    if(std::regex_search(string.begin(), string.end(), m, re)){
        if(from_start && m.position() != 0) return vm->None;
        i64 start = string._byte_index_to_unicode(m.position());
        i64 end = string._byte_index_to_unicode(m.position() + m.length());
        return VAR_T(ReMatch, start, end, m);
    }
    return vm->None;
};

inline void add_module_re(VM* vm){
    PyObject* mod = vm->new_module("re");
    ReMatch::register_class(vm, mod);

    vm->bind_func<2>(mod, "match", [](VM* vm, ArgsView args) {
        const Str& pattern = CAST(Str&, args[0]);
        const Str& string = CAST(Str&, args[1]);
        return _regex_search(pattern, string, true, vm);
    });

    vm->bind_func<2>(mod, "search", [](VM* vm, ArgsView args) {
        const Str& pattern = CAST(Str&, args[0]);
        const Str& string = CAST(Str&, args[1]);
        return _regex_search(pattern, string, false, vm);
    });

    vm->bind_func<3>(mod, "sub", [](VM* vm, ArgsView args) {
        const Str& pattern = CAST(Str&, args[0]);
        const Str& repl = CAST(Str&, args[1]);
        const Str& string = CAST(Str&, args[2]);
        std::regex re(pattern.begin(), pattern.end());
        return VAR(std::regex_replace(string.str(), re, repl.str()));
    });

    vm->bind_func<2>(mod, "split", [](VM* vm, ArgsView args) {
        const Str& pattern = CAST(Str&, args[0]);
        const Str& string = CAST(Str&, args[1]);
        std::regex re(pattern.begin(), pattern.end());
        std::cregex_token_iterator it(string.begin(), string.end(), re, -1);
        std::cregex_token_iterator end;
        List vec;
        for(; it != end; ++it){
            vec.push_back(VAR(it->str()));
        }
        return VAR(vec);
    });
}

struct Random{
    PY_CLASS(Random, random, Random)
    std::mt19937 gen;

    Random(){
        gen.seed(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }

    static void _register(VM* vm, PyObject* mod, PyObject* type){
        vm->bind_static_method<0>(type, "__new__", CPP_LAMBDA(VAR_T(Random)));

        vm->bind_method<1>(type, "seed", [](VM* vm, ArgsView args) {
            Random& self = CAST(Random&, args[0]);
            self.gen.seed(CAST(i64, args[1]));
            return vm->None;
        });

        vm->bind_method<2>(type, "randint", [](VM* vm, ArgsView args) {
            Random& self = CAST(Random&, args[0]);
            i64 a = CAST(i64, args[1]);
            i64 b = CAST(i64, args[2]);
            std::uniform_int_distribution<i64> dis(a, b);
            return VAR(dis(self.gen));
        });

        vm->bind_method<0>(type, "random", [](VM* vm, ArgsView args) {
            Random& self = CAST(Random&, args[0]);
            std::uniform_real_distribution<f64> dis(0.0, 1.0);
            return VAR(dis(self.gen));
        });

        vm->bind_method<2>(type, "uniform", [](VM* vm, ArgsView args) {
            Random& self = CAST(Random&, args[0]);
            f64 a = CAST(f64, args[1]);
            f64 b = CAST(f64, args[2]);
            std::uniform_real_distribution<f64> dis(a, b);
            return VAR(dis(self.gen));
        });
    }
};

inline void add_module_random(VM* vm){
    PyObject* mod = vm->new_module("random");
    Random::register_class(vm, mod);
    CodeObject_ code = vm->compile(kPythonLibs["random"], "random.py", EXEC_MODE);
    vm->_exec(code, mod);
}

inline void add_module_gc(VM* vm){
    PyObject* mod = vm->new_module("gc");
    vm->bind_func<0>(mod, "collect", CPP_LAMBDA(VAR(vm->heap.collect())));
}

inline void VM::post_init(){
    init_builtins(this);
#if !DEBUG_NO_BUILTIN_MODULES
    add_module_sys(this);
    add_module_time(this);
    add_module_json(this);
    add_module_math(this);
    add_module_re(this);
    add_module_dis(this);
    add_module_c(this);
    add_module_gc(this);
    add_module_random(this);
    add_module_base64(this);

    for(const char* name: {"this", "functools", "collections", "heapq", "bisect"}){
        _lazy_modules[name] = kPythonLibs[name];
    }

    CodeObject_ code = compile(kPythonLibs["builtins"], "<builtins>", EXEC_MODE);
    this->_exec(code, this->builtins);
    code = compile(kPythonLibs["_dict"], "<dict>", EXEC_MODE);
    this->_exec(code, this->builtins);
    code = compile(kPythonLibs["_set"], "<set>", EXEC_MODE);
    this->_exec(code, this->builtins);

    // property is defined in builtins.py so we need to add it after builtins is loaded
    _t(tp_object)->attr().set(__class__, property(CPP_LAMBDA(vm->_t(args[0]))));
    _t(tp_type)->attr().set(__base__, property([](VM* vm, ArgsView args){
        const PyTypeInfo& info = vm->_all_types[OBJ_GET(Type, args[0])];
        return info.base.index == -1 ? vm->None : vm->_all_types[info.base].obj;
    }));
    _t(tp_type)->attr().set(__name__, property([](VM* vm, ArgsView args){
        const PyTypeInfo& info = vm->_all_types[OBJ_GET(Type, args[0])];
        return VAR(info.name);
    }));

    _t(tp_bound_method)->attr().set("__self__", property([](VM* vm, ArgsView args){
        return CAST(BoundMethod&, args[0]).self;
    }));
    _t(tp_bound_method)->attr().set("__func__", property([](VM* vm, ArgsView args){
        return CAST(BoundMethod&, args[0]).func;
    }));

    _t(tp_slice)->attr().set("start", property([](VM* vm, ArgsView args){
        return CAST(Slice&, args[0]).start;
    }));
    _t(tp_slice)->attr().set("stop", property([](VM* vm, ArgsView args){
        return CAST(Slice&, args[0]).stop;
    }));
    _t(tp_slice)->attr().set("step", property([](VM* vm, ArgsView args){
        return CAST(Slice&, args[0]).step;
    }));
    _t(tp_object)->attr().set("__dict__", property([](VM* vm, ArgsView args){
        if(is_tagged(args[0]) || !args[0]->is_attr_valid()){
            vm->AttributeError("__dict__");
        }
        return VAR(MappingProxy(args[0]));
    }));

    if(enable_os){
        add_module_io(this);
        add_module_os(this);
        add_module_requests(this);
    }
#endif
}

}   // namespace pkpy

/*************************GLOBAL NAMESPACE*************************/
static std::map<void*, void(*)(void*)> _pk_deleter_map;

extern "C" {
    __EXPORT
    void pkpy_delete(void* p){
        auto it = _pk_deleter_map.find(p);
        if(it != _pk_deleter_map.end()){
            it->second(p);
        }else{
            free(p);
        }
    }

    __EXPORT
    void pkpy_vm_exec(pkpy::VM* vm, const char* source){
        vm->exec(source, "main.py", pkpy::EXEC_MODE);
    }

    __EXPORT
    char* pkpy_vm_get_global(pkpy::VM* vm, const char* name){
        pkpy::PyObject* val = vm->_main->attr().try_get(name);
        if(val == nullptr) return nullptr;
        try{
            pkpy::Str repr = pkpy::CAST(pkpy::Str&, vm->asRepr(val));
            return repr.c_str_dup();
        }catch(...){
            return nullptr;
        }
    }

    __EXPORT
    char* pkpy_vm_eval(pkpy::VM* vm, const char* source){
        pkpy::PyObject* ret = vm->exec(source, "<eval>", pkpy::EVAL_MODE);
        if(ret == nullptr) return nullptr;
        try{
            pkpy::Str repr = pkpy::CAST(pkpy::Str&, vm->asRepr(ret));
            return repr.c_str_dup();
        }catch(...){
            return nullptr;
        }
    }

    __EXPORT
    pkpy::REPL* pkpy_new_repl(pkpy::VM* vm){
        pkpy::REPL* p = new pkpy::REPL(vm);
        _pk_deleter_map[p] = [](void* p){ delete (pkpy::REPL*)p; };
        return p;
    }

    __EXPORT
    bool pkpy_repl_input(pkpy::REPL* r, const char* line){
        return r->input(line);
    }

    __EXPORT
    void pkpy_vm_add_module(pkpy::VM* vm, const char* name, const char* source){
        vm->_lazy_modules[name] = source;
    }

    __EXPORT
    pkpy::VM* pkpy_new_vm(bool use_stdio=true, bool enable_os=true){
        pkpy::VM* p = new pkpy::VM(use_stdio, enable_os);
        _pk_deleter_map[p] = [](void* p){ delete (pkpy::VM*)p; };
        return p;
    }

    __EXPORT
    char* pkpy_vm_read_output(pkpy::VM* vm){
        std::string json = vm->read_output();
        return strdup(json.c_str());
    }
}

#endif // POCKETPY_H
