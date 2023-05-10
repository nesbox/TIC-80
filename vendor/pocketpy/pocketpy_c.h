#ifndef POCKETPY_C_H 
#define POCKETPY_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct pkpy_vm_handle pkpy_vm;

//we we take a lot of inspiration from the lua api for these bindings
//the key difference being most methods return a bool, 
//true if it succeeded false if it did not

//if a method returns false call the pkpy_clear_error method to check the error and clear it
//if pkpy_clear_error returns false it means that no error was set, and it takes no action
//if pkpy_clear_error returns true it means there was an error and it was cleared, 
//it will provide a string summary of the error in the message parameter (if it is not NULL)
//if null is passed in as message, and it will just print the message to stderr
bool pkpy_clear_error(pkpy_vm*, char** message);
//NOTE you are responsible for freeing message 

//this will cause the vm to enter an error state and report the given message
//when queried
//note that at the moment this is more like a panic than throwing an error
//the user will not be able to catch it with python code
bool pkpy_error(pkpy_vm*, const char* message);


pkpy_vm* pkpy_vm_create(bool use_stdio, bool enable_os);
bool pkpy_vm_run(pkpy_vm*, const char* source);
void pkpy_vm_destroy(pkpy_vm*);

typedef int (*pkpy_function)(pkpy_vm*); 

bool pkpy_pop(pkpy_vm*, int n);

//push the item at index onto the top of the stack (as well as leaving it where
//it is on the stack)
bool pkpy_push(pkpy_vm*, int index);

bool pkpy_push_function(pkpy_vm*, pkpy_function);
bool pkpy_push_int(pkpy_vm*, int);
bool pkpy_push_float(pkpy_vm*, double);
bool pkpy_push_bool(pkpy_vm*, bool);
bool pkpy_push_string(pkpy_vm*, const char*);
bool pkpy_push_stringn(pkpy_vm*, const char*, int length);
bool pkpy_push_voidp(pkpy_vm*, void*);
bool pkpy_push_none(pkpy_vm*);

bool pkpy_set_global(pkpy_vm*, const char* name);
bool pkpy_get_global(pkpy_vm*, const char* name);

//first push callable you want to call
//then push the arguments to send
//argc is the number of arguments that was pushed (not counting the callable)
bool pkpy_call(pkpy_vm*, int argc);

//first push the object the method belongs to (self)
//then push the the argments
//argc is the number of arguments that was pushed (not counting the callable or self)
//name is the name of the method to call on the object
bool pkpy_call_method(pkpy_vm*, const char* name, int argc);


//we will break with the lua api here
//lua uses 1 as the index to the first pushed element for all of these functions
//but we will start counting at zero to match python
//we will allow negative numbers to count backwards from the top
bool pkpy_to_int(pkpy_vm*, int index, int* ret);
bool pkpy_to_float(pkpy_vm*, int index, double* ret);
bool pkpy_to_bool(pkpy_vm*, int index, bool* ret);
bool pkpy_to_voidp(pkpy_vm*, int index, void** ret);

//this method provides a strong reference, you are responsible for freeing the
//string when you are done with it
bool pkpy_to_string(pkpy_vm*, int index, char** ret);

//this method provides a weak reference, it is only valid until the
//next api call
//it is not null terminated
bool pkpy_to_stringn(pkpy_vm*, int index, const char** ret, int* size);


//these do not follow the same error semantics as above, their return values
//just say whether the check succeeded or not, or else return the value asked for

bool pkpy_is_int(pkpy_vm*, int index);
bool pkpy_is_float(pkpy_vm*, int index);
bool pkpy_is_bool(pkpy_vm*, int index);
bool pkpy_is_string(pkpy_vm*, int index);
bool pkpy_is_voidp(pkpy_vm*, int index);
bool pkpy_is_none(pkpy_vm*, int index);


//will return true if global exists
bool pkpy_check_global(pkpy_vm*, const char* name);

//will return true if the vm is currently in an error state
bool pkpy_check_error(pkpy_vm*);

//will return true if at least free empty slots remain on the stack
bool pkpy_check_stack(pkpy_vm*, int free);

//returns the number of elements on the stack
int pkpy_stack_size(pkpy_vm*);


#ifdef __cplusplus
}
#endif

#endif
