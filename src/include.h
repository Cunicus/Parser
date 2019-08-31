#pragma once

//everything we may need from the standard library
#include <vector>
#include <array>
#include <iostream>
#include <bitset>
#include <string>
#include <memory>
#include <sstream>
#include <fstream>
#include <array>
#include <iostream>
#include <stdio.h>
#include <stdarg.h>

//most often used things from std
using std::string;
using std::vector;
using std::cout;
using std::array;

//short forms of for loops used most commonly
#define ifor(count) for(int i = 0; i < count; ++i) //count i
#define rfor(container) for(auto& it : container)  //reference it
#define cfor(container) for(auto it : container)   //copy it

//short form for switch statements
#define bcase break; case

//type aliases
using byte   = std::uint8_t;
using int8   = std::int8_t ;
using int16  = std::int16_t;
using int32  = std::int32_t;
using int64  = std::int64_t;

using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

//custom container with buckets storing if they are occupied or not
//this gives us an easy to use container with constant address objects 
template<typename T, int32 N>
struct ConstAddress {

    std::bitset<N> used = 0;
    union { char prevent_data_init; array<T, N> data; };
    ConstAddress<T, N>* next = nullptr;

    ConstAddress()  : prevent_data_init(0) {}
    ~ConstAddress() { delete next; }

    T* alloc() {
        ifor(N) if(!used[i]) { used[i] = true; new (&data[i]) T; return &data[i];}
        if(next == nullptr) next = new ConstAddress;
        return next->alloc();
    }

    void free(T* ptr) {
        ifor(N) if(&data[i] == ptr) { used[i] = false; data[i].~T(); return; }
        if(next) next->free(ptr);
    }

    template<typename T, int32 N>
    struct ConstAddressIterator {
        ConstAddress* bucket; int32 index;
        ConstAddressIterator operator++() {
            retry: ++index;
            if(index >= N) {
                bucket = bucket->next;
                if(bucket == nullptr)  { invalidate(); return *this; }
                index = -1; ++(*this);
            }
            if(bucket->used[index]) return *this;
            goto retry; //@todo while instead? readability uncommon but probably better
        }

        ConstAddressIterator operator++(int) { ConstAddressIterator<T, N> tmp = *this; ++(*this); return tmp; }

        T& operator*()  const { return   bucket->data[index]; }
        T* operator->() const { return &(bucket->data[index]); }

        bool operator==(const ConstAddressIterator<T, N>& other) const { return bucket == other.bucket && index == other.index; }
        bool operator!=(const ConstAddressIterator<T, N>& other) const { return !(*this == other); }

        void invalidate() { bucket = nullptr; index = 0; }
    };
    
    //because the first bit can be unused, we need to find the first valid one with ++Iterator
    ConstAddressIterator<T, N> begin() { return ++ConstAddressIterator<T, N>{this, -1}; }
    ConstAddressIterator<T, N> end() { return {nullptr, 0}; }

};

//just raw memory, that can be used as a stack or byte "stream"
struct IndexedRawMemory {
    vector<byte> memory;
    int64 index = 0;

    void grow(int size) {
        if(index + size > memory.size()) memory.resize(index + size);
    }
    void grow_index(int size) {
        grow(size); index += size;
    }

    template<typename T> void push(T t) {
        grow(sizeof(T));
        (T&)memory[index] = t;
        index += sizeof(T);
    }

    void push(byte* data, int32 size) {
        grow(size);
        memcpy(&memory[index], data, size);
        index += size;
    }

    void pop(byte* data, int32 size) {
        index -= size;
        memcpy(data, &memory[index], size);
    }

    template<typename T> T pop() {
        index -= sizeof(T);
        return (T&)memory[index];
    }

    template<typename T> T next() {
        T t = (T&)memory[index];
        index += sizeof(T);
        return t;
    }

    byte* at(int index) { return &memory[index]; }

    template<typename T> T& as(int offset) { return (T&)memory[offset]; }
    template<typename T> T& rel_as(int rel_index) { return as<T>(index + rel_index); }
};


//print error with location and exit the program
[[ noreturn ]] void fatal_error_impl(const string& file, int line_number, const char* fmt, ...) {
    std::cout << "Fatal Error in file '" << file << "' at line " << std::to_string(line_number) << ": ";
    va_list args; va_start(args, fmt);
    vprintf(fmt, args); va_end(args);
     exit(-1);
}


#define fatal_error(message, ...) fatal_error_impl(__FILE__, __LINE__, message, __VA_ARGS__);
#define print_line (std::cerr << "File: " << __FILE__ << " Line: " << __LINE__ << '\n') 

#define assert _ASSERT

//load file contents into a string
string load_file(const string& file_path) {
    std::stringstream sstream; std::ifstream file(file_path);
    if(!file.fail() && (sstream << file.rdbuf())) return sstream.str();
    fatal_error("Failed to load file: %s", file_path.c_str());
}

//note return not used often -> shoud probably be removed, but might still be better than .push_back
template<typename T> auto add_element(vector<T>& vec, T t) {
    vec.push_back(std::move(t)); return vec.size() - 1;
}

//type list to store index accessible types
template <class... Args> struct type_list {
   template <std::size_t N> using type = typename std::tuple_element<N, std::tuple<Args...>>::type;
};

//Unify static and dynamic arrays into a single name
template<typename T, int N> struct arr_imp { typedef std::array<T, N> type; };
template<typename T> struct arr_imp<T, 0> { typedef std::vector<T> type; };
template<typename T, int N = 0> using Array = typename arr_imp<T, N>::type;

//execute an expression at global scope, also not used at the moment, execution order probably unrelyable
#define exec(expr) namespace {  int _ = (expr, 0); }

// std::ostream& operator<<(std::ostream& stream, std::string_view view) { stream.write(view.data(), view.size()); return stream; }
