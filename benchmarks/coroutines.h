/* 
 * Copyright (C) Jacob Bartholomew Blankenship - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Written by Jacob Bartholomew Blankenship [jacob.blankenship@roguedb.com] - January 2022
*/

#ifndef COROUTINES_H
#define COROUTINES_H

#include <coroutine>

namespace rogue
{
    namespace concepts
    {
        
        template<typename Value>
        struct Generator
        {
            // The class name 'Generator' is our choice and it is not required for coroutine
            // magic. Compiler recognizes coroutine by the presence of 'co_yield' keyword.
            // You can use name 'MyGenerator' (or any other name) instead as long as you include
            // nested struct promise_type with 'MyGenerator get_return_object()' method.
            // (Note: It is necessary to adjust the declarations of constructors and destructors
            //  when renaming.)
         
            struct promise_type;
         
            struct promise_type // required
            {
                Value m_value;
                std::exception_ptr m_exception;
         
                Generator get_return_object()
                {
                    return Generator(std::coroutine_handle<promise_type>::from_promise(*this));
                }
                std::suspend_always initial_suspend() { return {}; }
                std::suspend_always final_suspend() noexcept { return {}; }
                void unhandled_exception() { m_exception = std::current_exception(); } // saving
                                                                                      // exception
         
                template<std::convertible_to<Value> From> // C++20 concept
                std::suspend_always yield_value(From&& from)
                {
                    m_value = std::forward<From>(from); // caching the result in promise
                    return {};
                }
                void return_value(Value&& value)
                {
                    m_value = std::forward<Value>(value);
                }
            };
         
            std::coroutine_handle<promise_type> m_handler;
         
            Generator(std::coroutine_handle<promise_type> handler) : m_handler(handler) {}
            ~Generator() { m_handler.destroy(); }
            
            bool done() const { return m_handler.done(); }
            
            explicit operator bool()
            {
                fill(); // The only way to reliably find out whether or not we finished coroutine,
                        // whether or not there is going to be a next value generated (co_yield)
                        // in coroutine via C++ getter (operator () below) is to execute/resume
                        // coroutine until the next co_yield point (or let it fall off end).
                        // Then we store/cache result in promise to allow getter (operator() below
                        // to grab it without executing coroutine).
                return !m_handler.done();
            }

            Value operator()()
            {
                fill();
                m_full = false; // we are going to move out previously cached
                               // result to make promise empty again
                return std::move(m_handler.promise().m_value);
            }
         
        private:
            bool m_full = false;
         
            void fill()
            {
                if (!m_full)
                {
                    m_handler();
                    if (m_handler.promise().m_exception)
                        std::rethrow_exception(m_handler.promise().m_exception);
                    // propagate coroutine exception in called context
         
                    m_full = true;
                }
            }
        };
    }
}

#endif //COROUTINES_H