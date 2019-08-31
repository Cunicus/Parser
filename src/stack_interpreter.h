
IndexedRawMemory byte_code;      //optcode instructions
IndexedRawMemory stack;          //operation_stack
IndexedRawMemory function_stack; //classical stack

namespace Opt { enum : byte {
    Constant, RelativeAddress, GlobalAddress,
    AddI, MulI, SubI, DivI, ModI, NegateI,
    AddF, MulF, SubF, DivF, NegateF,
    Equal, Nonequal, Greater, Less, GreaterEqual, LessEqual, 
    EqualF, NonequalF, GreaterF, LessF, GreaterEqualF, LessEqualF, 
    IntToFloat, FloatToInt, IntToInt,
    Load, Save,
    Ignore,
    Allocate, Free, Print,
    Jump, JumpZero,
    Grow, Call, Return, Halt
};}

#define  o(op, type) int s = stack.pop<type>(); stack.push<type>(stack.pop<type>() op s);
#define oe(op, type) int s = stack.pop<type>(); stack.push<int >(stack.pop<type>() op s);

#define bini(opt, op) bcase opt: switch(byte_code.next<byte>()) { \
    bcase 1: { o(op, byte ) }   bcase 2: { o(op, short) } \
    bcase 4: { o(op, int  ) }   bcase 8: { o(op, int64) } \
}

#define binie(opt, op) bcase opt: switch(byte_code.next<byte>()) { \
    bcase 1: { oe(op, byte ) }   bcase 2: { oe(op, short) } \
    bcase 4: { oe(op, int  ) }   bcase 8: { oe(op, int64) } \
}

#define binf(opt, op)  bcase opt: {  o(op, float) }
#define binfe(opt, op) bcase opt: { oe(op, float) }

//TODO replace comparison operators with compare operator - 0 + and multiple jumps jg jl je

int run() {
    if(print_debug_information) {
        std::cout << '\n';
        for(int i = 0; i < byte_code.index; ++i) std::cout << (int)*byte_code.at(i) << ' ';
        std::cout << '\n';
    }
    
    //reserve memory for the stacks
    stack.memory.         resize(1000); //need runtime check for overflow
    function_stack.memory.resize(1000); //need runtime check for overflow
    
    //copy string literals at the start of the memory
    byte* copy_to = function_stack.memory.data();
    for(String s : string_literals) {
        memcpy(copy_to, s.start, s.size());
        copy_to += s.size(); *copy_to++ = '\0';
    }

    int64 bp = 0; byte_code.index = 0;

    function_stack.grow_index(global_scope->stack_size);

    using namespace Opt;
    while(1) { 
    
    if(print_debug_information) {
        std::cout << (int)byte_code.rel_as<byte>(0) << '\n'; 
        
        std::cout << "Stack:\n";
        for(int i = 0; i < stack.index; ++i) {
            std::cout << (int)stack.as<byte>(i) << ' ';
        } std::cout << '\n';

        std::cout << "Memory::\n";
        for(int i = 0; i < function_stack.index; ++i) {
            std::cout << (int)function_stack.as<byte>(i) << ' ';
        } std::cout << "\n\n";
    }

    switch(byte_code.next<byte>()) {
        bcase Constant: { 
            int size = byte_code.next<byte>();
            stack.push(byte_code.at(byte_code.index), size);
            byte_code.index += size;
        }
        
        bcase RelativeAddress: 
            stack.push(function_stack.at(bp) + byte_code.next<int64>());
        
        bcase GlobalAddress:
            stack.push(function_stack.at(byte_code.next<int64>()));
        
        bini(AddI, +) bini(MulI, *) bini(SubI, -) bini(DivI, /) bini(ModI, %) 
        binie(Equal, ==) binie(Nonequal, !=) binie(Greater, >) binie(Less, <) binie(GreaterEqual, >=) binie(LessEqual, <=)
        binf(AddF, +) binf(MulF, *) binf(SubF, -) binf(DivF, /)
        binfe(EqualF, ==) binfe(NonequalF, !=) binfe(GreaterF, >) binfe(LessF, <) binfe(GreaterEqualF, >=) binfe(LessEqualF, <=)

        //TODO add support for all integers
        bcase NegateI:
            switch(byte_code.next<byte>()) {
                bcase 1: stack.push(-stack.pop<byte>()); bcase 2: stack.push(-stack.pop<int16>());
                bcase 4: stack.push(-stack.pop<int>());  bcase 8: stack.push(-stack.pop<int64>());
            }
        
        bcase NegateF: 
            stack.push<float>(-stack.pop<float>());

        bcase Halt: 
            return stack.pop<int>();
        
        bcase Grow: 
            function_stack.push(bp); 
            bp = function_stack.index; 
            function_stack.grow_index(stack.pop<int>());
        
        bcase Return: 
            function_stack.index = bp;
            bp = function_stack.pop<int64>(); 
            byte_code.index = function_stack.pop<int64>();

        bcase Load:
            stack.push((byte*)stack.pop<int64>(), byte_code.next<byte>());
        
        bcase Save: {
            void* data = (void*)stack.pop<int64>();
            stack.pop ((byte*)data, byte_code.next<byte>());
        } 

        bcase Call: 
            function_stack.push(byte_code.index);
        case Jump: 
            byte_code.index = stack.pop<int64>();

        bcase JumpZero:
            if(stack.pop<int>() == 0) byte_code.index = stack.pop<int64>();
            else                      stack.pop<int64>();
        
        bcase IntToFloat: 
            stack.push<float>(stack.pop<int>());
        
        bcase FloatToInt:
            stack.push<int>(stack.pop<float>());
        
        bcase IntToInt: {

            int64 value;
            switch(byte_code.next<byte>()) {
                bcase 1: value = stack.pop<byte>(); bcase 2: value = stack.pop<int16>();
                bcase 4: value = stack.pop<int>();  bcase 8: value = stack.pop<int64>();
            }
            
            switch(byte_code.next<byte>()) {
                bcase 1: stack.push((byte)value);   bcase 2: stack.push((short)value);
                bcase 4: stack.push((int)value);    bcase 8: stack.push((int64)value);
            }
        }
 
        bcase Allocate: 
            stack.push((int64)malloc(stack.pop<int>()));
 
        bcase Free: 
            free((void*)stack.pop<int64>());
 
        bcase Print:
            puts((char*)stack.pop<int64>());
 
        bcase Ignore:
            stack.index -= byte_code.next<byte>();
 
    }}
}
