//===--------------------- cxa_exception_storage.cpp ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//  
//  This file implements the storage for the "Caught Exception Stack"
//  http://mentorembedded.github.io/cxx-abi/abi-eh.html (section 2.2.2)
//  
//===----------------------------------------------------------------------===//
#define _LIBCPP_BUILDING_LIBRARY
#define _LIBCPP_BUILDING_NEW
#include "cxa_exception.hpp"

#include <__threading_support>

#if defined(_LIBCXXABI_HAS_NO_THREADS)

namespace __cxxabiv1 {
extern "C" {
    static __cxa_eh_globals eh_globals;
    __cxa_eh_globals *__cxa_get_globals() { return &eh_globals; }
    __cxa_eh_globals *__cxa_get_globals_fast() { return &eh_globals; }
    }
}

#elif defined(HAS_THREAD_LOCAL)

namespace __cxxabiv1 {

namespace {
    __cxa_eh_globals * __globals () {
        static thread_local __cxa_eh_globals eh_globals;
        return &eh_globals;
        }
    }

extern "C" {
    __cxa_eh_globals * __cxa_get_globals      () { return __globals (); }
    __cxa_eh_globals * __cxa_get_globals_fast () { return __globals (); }
    }
}

#else

#include "abort_message.h"
#include "fallback_malloc.h"

//  In general, we treat all threading errors as fatal.
//  We cannot call std::terminate() because that will in turn
//  call __cxa_get_globals() and cause infinite recursion.

namespace __cxxabiv1 {
namespace {
    std::__libcpp_tls_key key_;
    std::__libcpp_exec_once_flag flag_ = _LIBCPP_EXEC_ONCE_INITIALIZER;

    void destruct_ (void *p) {
        __free_with_fallback ( p );
        if ( 0 != std::__libcpp_tls_set ( key_, NULL ) )
            abort_message("cannot zero out thread value for __cxa_get_globals()");
        }

    void construct_ () {
        if ( 0 != std::__libcpp_tls_create ( &key_, destruct_ ) )
            abort_message("cannot create thread specific key for __cxa_get_globals()");
        }
}   

extern "C" {
    __cxa_eh_globals * __cxa_get_globals () {
    //  Try to get the globals for this thread
        __cxa_eh_globals* retVal = __cxa_get_globals_fast ();
    
    //  If this is the first time we've been asked for these globals, create them
        if ( NULL == retVal ) {
            retVal = static_cast<__cxa_eh_globals*>
                        (__calloc_with_fallback (1, sizeof (__cxa_eh_globals)));
            if ( NULL == retVal )
                abort_message("cannot allocate __cxa_eh_globals");
            if ( 0 != std::__libcpp_tls_set ( key_, retVal ) )
               abort_message("std::__libcpp_tls_set failure in __cxa_get_globals()");
           }
        return retVal;
        }

    // Note that this implementation will reliably return NULL if not
    // preceded by a call to __cxa_get_globals().  This is an extension
    // to the Itanium ABI and is taken advantage of in several places in
    // libc++abi.
    __cxa_eh_globals * __cxa_get_globals_fast () {
    //  First time through, create the key.
        if (0 != std::__libcpp_execute_once(&flag_, construct_))
            abort_message("execute once failure in __cxa_get_globals_fast()");
//        static int init = construct_();
        return static_cast<__cxa_eh_globals*>(std::__libcpp_tls_get(key_));
        }
    
}
}
#endif
