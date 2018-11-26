/*******************************************************************************
* Copyright (c) 2004-2012, 2015 Sergey Radionov <rsatom_gmail.com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*   1. Redistributions of source code must retain the above copyright notice,
*      this list of conditions and the following disclaimer.
*   2. Redistributions in binary form must reproduce the above copyright notice,
*      this list of conditions and the following disclaimer in the documentation
*      and/or other materials provided with the distribution.

* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
* OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#pragma once

class finally_execute_base
{
public:
    void dismiss() const {m_dismissed=true;};

protected:
    finally_execute_base(): m_dismissed(false){}
    finally_execute_base(const finally_execute_base& from)
        :m_dismissed(from.m_dismissed){from.dismiss();}

protected:
    mutable bool m_dismissed;
};

template<typename exec_type>
class finally_execute_impl: public finally_execute_base
{
public:
    finally_execute_impl(exec_type exec) : m_exec(exec) {}
    ~finally_execute_impl()
        //try-catch needed to avoid terminate() in case of exception during m_exec()
        { if( !m_dismissed ) try{ m_exec(); } catch(...) { }; }

private:
    exec_type m_exec;
};

typedef const finally_execute_base& finally_execute;

template <typename exec_type>
inline finally_execute_impl<exec_type>
make_fin_exec(const exec_type& exec)
{
    return finally_execute_impl<exec_type>(exec);
}
