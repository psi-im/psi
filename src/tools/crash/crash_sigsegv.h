/*
 * sigsegv.h -- sigsegv handlers
 *
 * Copyright (c) 2003 Juan F. Codagnone <juam@users.sourceforge.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef CRASH_SIGSEGV_H
#define CRASH_SIGSEGV_H

namespace Crash {

/**
 * sets the print function (it is used to print the backtrace)
 * if it isn't set, or it is set to NULL, printf will be used.
 *
 * \param  _need_cr    needs to append an \n at the end?
 */
void *sigsegv_set_print( int (* fnc)(const char *format, ...), int _needs_cr);

/**
 * try to print a backtrace of the programa, and dump the core
 */
void sigsegv_handler_fnc(int signal);

/**
 * try to print a verbose backtrace of the program, and dump the core
 */
void sigsegv_handler_bt_full_fnc(int signal);

}; // namespace Crash

#endif
