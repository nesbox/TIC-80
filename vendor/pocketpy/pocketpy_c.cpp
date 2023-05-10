#include "pocketpy.h"
#include "pocketpy_c.h"

using namespace pkpy;

#define PKPY_STACK_SIZE 32

#define SAFEGUARD_OPEN try { \

#define SAFEGUARD_CLOSE \
    } catch(std::exception& e) { \
        std::cerr << "ERROR: a std::exception " \
        << "this probably means pocketpy itself has a bug!\n" \
        << e.what() << "\n"; \
        exit(2); \
    } catch(...) { \
        std::cerr << "ERROR: a unknown exception was thrown from " << __func__ \
        << "\nthis probably means pocketpy itself has a bug!\n"; \
        exit(2); \
    }


#define ERRHANDLER_OPEN SAFEGUARD_OPEN \
    try { \
    if (vm->c_data->size() > 0 && vm->c_data->top() == nullptr) \
        return false; \

#define ERRHANDLER_CLOSE \
    } catch( Exception e ) { \
        vm->c_data->push(py_var(vm, e)); \
        vm->c_data->push(NULL); \
        return false; \
    } \
    SAFEGUARD_CLOSE \



class CVM : public VM {
    public :

    ValueStackImpl<PKPY_STACK_SIZE>* c_data;
    CVM(bool enable_os=true) : VM(enable_os) {
        c_data = new ValueStackImpl<PKPY_STACK_SIZE>();
    }

    ~CVM() {
        c_data->clear();
        delete c_data;
    }
};


//for now I will unpack a tuple automatically, we may not want to handle
//it this way, not sure
//it is more lua like, but maybe not python like
static void unpack_return(CVM* vm, PyObject* ret) {
    if (is_type(ret, vm->tp_tuple)) {
        Tuple& t = py_cast<Tuple&>(vm, ret);
        for (int i = 0; i < t.size(); i++) 
            vm->c_data->push(t[i]);
    } else if (ret == vm->None) {
        //do nothing here
        //having to pop the stack after every call that returns none is annoying
        //lua does not do this
        //
        //so for now we will not push none on the stack when it is the sole thing returned
        //if this becomes a problem we can change it
        //
        //you can still check if it returned none by comparing stack size before
        //and after if you have to
    } else 
        vm->c_data->push(ret);

}


bool pkpy_clear_error(pkpy_vm* vm_handle, char** message) {
    CVM* vm = (CVM*) vm_handle;
    SAFEGUARD_OPEN

        if (vm->c_data->size() == 0 || vm->c_data->top() != nullptr) 
            return false;

        vm->c_data->pop();
        Exception& e = py_cast<Exception&>(vm, vm->c_data->top());
        if (message != nullptr) 
            *message = e.summary().c_str_dup();
        else
            std::cerr << "ERROR: " << e.summary() << "\n";

        vm->c_data->clear();
        vm->callstack.clear();
        vm->s_data.clear(); 
        return true;

    SAFEGUARD_CLOSE
}

void gc_marker_ex(CVM* vm) {
    for(PyObject* obj: *vm->c_data) if(obj!=nullptr) OBJ_MARK(obj);
}


static void noop_output_handler(VM* vm, const Str& str) {
    (void) vm;
    (void) str;
}

pkpy_vm* pkpy_vm_create(bool use_stdio, bool enable_os) {

    CVM* vm = new CVM(enable_os);
    vm->c_data = new ValueStackImpl<PKPY_STACK_SIZE>();
    vm->_gc_marker_ex = (void (*)(VM*)) gc_marker_ex;

    if (!use_stdio) {
        vm->_stdout = noop_output_handler;
        vm->_stderr = noop_output_handler;
    }

    return (pkpy_vm*) vm;
}

bool pkpy_vm_run(pkpy_vm* vm_handle, const char* source) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    CodeObject_ code = vm->compile(source, "<c-bound>", EXEC_MODE);
    PyObject* result = vm->_exec(code, vm->_main);

    //unpack_return(w, result);
    //NOTE: it seems like vm->_exec should return whatever the last command it
    //ran returned but instead it seems to pretty much always return None
    //so I guess uncomment this line if that every changes

    return true;
    ERRHANDLER_CLOSE
}

void pkpy_vm_destroy(pkpy_vm* vm_handle) {
    CVM* vm = (CVM*) vm_handle;
    delete vm;
}

static void propagate_if_errored(CVM* vm, ValueStackImpl<PKPY_STACK_SIZE>* stored_stack) {
    try {
        if (vm->c_data->size() == 0 || vm->c_data->top() != nullptr) 
            return;

        vm->c_data->pop();
        Exception& e = py_cast<Exception&>(vm, vm->c_data->top());
        vm->c_data->pop();

        vm->c_data = stored_stack;

        throw e;
    } catch(Exception& e) {
        throw;
    } catch(...) {
        std::cerr << "ERROR: a non pocketpy exeception was thrown " 
            << "this probably means pocketpy itself has a bug!\n"; 
        exit(2); 
    }
}

PyObject* c_function_wrapper(VM* vm, ArgsView args) {
    LuaStyleFuncC f = py_cast<NativeFunc&>(vm, args[-2])._lua_f;
    CVM* cvm = (CVM*) vm;

    //setup c stack
    ValueStackImpl<PKPY_STACK_SIZE> local_stack;

    for (int i = 0; i < args.size(); i++)
        local_stack.push(args[i]);
    
    ValueStackImpl<PKPY_STACK_SIZE>* stored_stack = cvm->c_data;
    cvm->c_data = &local_stack;

    int retc = f(cvm);

    propagate_if_errored(cvm, stored_stack);
    cvm->c_data = stored_stack;

    PyObject* ret = cvm->None;

    if (retc == 1) 
        ret = local_stack.top();
    else if (retc > 1) {
        Tuple t(retc);

        for (int i = 0; i < retc; i++)  {
            int stack_index = (local_stack.size() - retc) + i;
            t[i] = local_stack.begin()[stack_index];
        }

        ret = py_var(cvm, t);
    }

    return ret;
}

bool pkpy_push_function(pkpy_vm* vm_handle, pkpy_function f) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    //TODO right now we just treat all c bound functions a varargs functions
    //do we want to change that?
    NativeFunc nf = NativeFunc(c_function_wrapper, -1, 0);
    nf._lua_f = (LuaStyleFuncC) f;

    vm->c_data->push(py_var(vm, nf));

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_push_int(pkpy_vm* vm_handle, int value) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    vm->c_data->push(py_var(vm, value));

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_push_float(pkpy_vm* vm_handle, double value) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN
    vm->c_data->push(py_var(vm, value));

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_push_bool(pkpy_vm* vm_handle, bool value) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN
    vm->c_data->push(py_var(vm, value));

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_push_string(pkpy_vm* vm_handle, const char* value) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN
    vm->c_data->push(py_var(vm, value));

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_push_stringn(pkpy_vm* vm_handle, const char* value, int length) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    Str s = Str(value, length);
    vm->c_data->push(py_var(vm, s));

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_push_voidp(pkpy_vm* vm_handle, void* value) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN
    vm->c_data->push(py_var(vm, value));

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_push_none(pkpy_vm* vm_handle) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN
    vm->c_data->push(vm->None);

    return true;
    ERRHANDLER_CLOSE
}



bool pkpy_set_global(pkpy_vm* vm_handle, const char* name) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    vm->_main->attr().set(name, vm->c_data->top());

    vm->c_data->pop();

    return true;
    ERRHANDLER_CLOSE
}

//get global will also get bulitins
bool pkpy_get_global(pkpy_vm* vm_handle, const char* name) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    PyObject* o = vm->_main->attr().try_get(name);
    if (o == nullptr) {
        o = vm->builtins->attr().try_get(name);
        if (o == nullptr)
            throw Exception("NameError", "could not find requested global");
    }

    vm->c_data->push(o);

    return true;
    ERRHANDLER_CLOSE
}


bool pkpy_call(pkpy_vm* vm_handle, int argc) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    int callable_index = vm->c_data->size() - argc  - 1;

    PyObject* callable = vm->c_data->begin()[callable_index];

    vm->s_data.push(callable);
    vm->s_data.push(PY_NULL);

    for (int i = 0; i < argc; i++) 
        vm->s_data.push(vm->c_data->begin()[callable_index + i + 1]);

    PyObject* o = vm->vectorcall(argc);

    vm->c_data->shrink(argc + 1);

    unpack_return(vm, o);

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_call_method(pkpy_vm* vm_handle, const char* name, int argc) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    int self_index = vm->c_data->size() - argc  - 1;
    PyObject* self = vm->c_data->begin()[self_index];

    PyObject* callable = vm->get_unbound_method(self, name, &self);

    vm->s_data.push(callable);
    vm->s_data.push(self);

    for (int i = 0; i < argc; i++) 
        vm->s_data.push(vm->c_data->begin()[self_index + i + 1]);

    PyObject* o = vm->vectorcall(argc);

    vm->c_data->shrink(argc + 1);

    unpack_return(vm, o);

    return true;
    ERRHANDLER_CLOSE
}



static int lua_to_cstack_index(int index, int size) {
    if (index < 0)
        index = size + index;
    return index;
}

bool pkpy_to_int(pkpy_vm* vm_handle, int index, int* ret) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    index = lua_to_cstack_index(index, vm->c_data->size());

    PyObject* o = vm->c_data->begin()[index];
    if (ret != nullptr)
        *ret = py_cast<int>(vm, o);

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_to_float(pkpy_vm* vm_handle, int index, double* ret) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    index = lua_to_cstack_index(index, vm->c_data->size());

    PyObject* o = vm->c_data->begin()[index];
    if (ret != nullptr)
        *ret = py_cast<double>(vm, o);

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_to_bool(pkpy_vm* vm_handle, int index, bool* ret) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    index = lua_to_cstack_index(index, vm->c_data->size());

    PyObject* o = vm->c_data->begin()[index];
    if (ret != nullptr)
        *ret = py_cast<bool>(vm, o);

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_to_voidp(pkpy_vm* vm_handle, int index, void** ret) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    index = lua_to_cstack_index(index, vm->c_data->size());

    PyObject* o = vm->c_data->begin()[index];
    if (ret != nullptr) 
        *ret = py_cast<void*>(vm, o);

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_to_string(pkpy_vm* vm_handle, int index, char** ret) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    index = lua_to_cstack_index(index, vm->c_data->size());

    PyObject* o = vm->c_data->begin()[index];
    if (ret != nullptr) {
        Str& s = py_cast<Str&>(vm, o);
        *ret = s.c_str_dup();
    }

    return true;
    ERRHANDLER_CLOSE
}

bool pkpy_to_stringn(pkpy_vm* vm_handle, int index, const char** ret, int* size) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN

    index = lua_to_cstack_index(index, vm->c_data->size());

    PyObject* o = vm->c_data->begin()[index];
    if (ret != nullptr) {
        std::string_view sv = py_cast<Str&>(vm, o).sv();
        *ret = sv.data();
        *size = sv.size();
    }

    return true;
    ERRHANDLER_CLOSE
}


bool pkpy_is_int(pkpy_vm* vm_handle, int index) {
    CVM* vm = (CVM*) vm_handle;
    index = lua_to_cstack_index(index, vm->c_data->size());
    PyObject* o = vm->c_data->begin()[index];

    return is_type(o, vm->tp_int);
}
bool pkpy_is_float(pkpy_vm* vm_handle, int index) {
    CVM* vm = (CVM*) vm_handle;
    index = lua_to_cstack_index(index, vm->c_data->size());
    PyObject* o = vm->c_data->begin()[index];

    return is_type(o, vm->tp_float);
}
bool pkpy_is_bool(pkpy_vm* vm_handle, int index) {
    CVM* vm = (CVM*) vm_handle;
    index = lua_to_cstack_index(index, vm->c_data->size());
    PyObject* o = vm->c_data->begin()[index];

    return is_type(o, vm->tp_bool);
}
bool pkpy_is_string(pkpy_vm* vm_handle, int index) {
    CVM* vm = (CVM*) vm_handle;
    index = lua_to_cstack_index(index, vm->c_data->size());
    PyObject* o = vm->c_data->begin()[index];

    return is_type(o, vm->tp_str);
}
bool pkpy_is_voidp(pkpy_vm* vm_handle, int index) {
    CVM* vm = (CVM*) vm_handle;
    index = lua_to_cstack_index(index, vm->c_data->size());
    PyObject* o = vm->c_data->begin()[index];

    return is_type(o, VoidP::_type(vm));
}

bool pkpy_is_none(pkpy_vm* vm_handle, int index) {
    CVM* vm = (CVM*) vm_handle;
    index = lua_to_cstack_index(index, vm->c_data->size());
    PyObject* o = vm->c_data->begin()[index];

    return o == vm->None;
}

bool pkpy_check_global(pkpy_vm* vm_handle, const char* name) {
    CVM* vm = (CVM*) vm_handle;
    SAFEGUARD_OPEN
    PyObject* o = vm->_main->attr().try_get(name);
    if (o == nullptr) {
        o = vm->builtins->attr().try_get(name);
        if (o == nullptr)
            return false;
    }
    return true;

    SAFEGUARD_CLOSE
}

bool pkpy_check_error(pkpy_vm* vm_handle) {
    CVM* vm = (CVM*) vm_handle;
    SAFEGUARD_OPEN
    if (vm->c_data->size() > 0 && vm->c_data->top() == nullptr) 
        return true; 
    return false;
    SAFEGUARD_CLOSE
}


bool pkpy_check_stack(pkpy_vm* vm_handle, int free) {
    CVM* vm = (CVM*) vm_handle;
    return free + vm->c_data->size() <= PKPY_STACK_SIZE;
}

int pkpy_stack_size(pkpy_vm* vm_handle) {
    CVM* vm = (CVM*) vm_handle;
    return vm->c_data->size();
}

bool pkpy_pop(pkpy_vm* vm_handle, int n) {
    CVM* vm = (CVM*) vm_handle;
    vm->c_data->shrink(n);
    return true;
}


bool pkpy_push(pkpy_vm* vm_handle, int index) {
    CVM* vm = (CVM*) vm_handle;
    index = lua_to_cstack_index(index, vm->c_data->size());
    vm->c_data->push(vm->c_data->begin()[index]);
    return true;
}


bool pkpy_error(pkpy_vm* vm_handle, const char* message) {
    CVM* vm = (CVM*) vm_handle;
    ERRHANDLER_OPEN
    throw Exception("CBindingError", message);
    ERRHANDLER_CLOSE
}

