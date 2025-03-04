﻿/* Software License Agreement
 *
 *     Copyright(C) 1994-2018 David Lindauer, (LADSoft)
 *
 *     This file is part of the Orange C Compiler package.
 *
 *     The Orange C Compiler package is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version, with the addition of the
 *     Orange C "Target Code" exception.
 *
 *     The Orange C Compiler package is distributed in the hope that it will be 
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with Orange C.  If not, see <http://www.gnu.org/licenses/>.
 *
 *     contact information:
 *         email: TouchStone222@runbox.com <David Lindauer>
 *
 */

#include "compiler.h"
#include <stack>
#include "mangle.h"
#include "initbackend.h"
#include "symtab.h"
#include "cpplookup.h"
#include "template.h"
#include "declcpp.h"
#include "ccerr.h"
#include "declcons.h"
#include "help.h"
#include "expr.h"
#include "declare.h"
#include "lex.h"
#include "beinterf.h"
#include "types.h"
#include "memory.h"
#include "occparse.h"

namespace Parser
{
int inNoExceptHandler;

static HASHTABLE* intrinsicHash;

typedef bool INTRINS_FUNC(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_abstract(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_base_of(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_class(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_constructible(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_convertible_to(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_empty(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_enum(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_final(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_literal(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_nothrow_constructible(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_nothrow_assignable(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_pod(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_polymorphic(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_standard_layout(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_trivial(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_trivially_assignable(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_trivially_constructible(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_trivially_destructible(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_trivially_copyable(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_union(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);
static bool is_literal_type(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp);

static struct _ihash
{
    const char* name;
    INTRINS_FUNC* func;

} defaults[] = {
    {"__is_abstract", is_abstract},
    {"__is_base_of", is_base_of},
    {"__is_class", is_class},
    {"__is_constructible", is_constructible},
    {"__is_convertible_to", is_convertible_to},
    {"__is_empty", is_empty},
    {"__is_enum", is_enum},
    {"__is_final", is_final},
    {"__is_literal", is_literal},
    {"__is_nothrow_constructible", is_nothrow_constructible},
    {"__is_nothrow_assignable", is_nothrow_assignable},
    {"__is_pod", is_pod},
    {"__is_polymorphic", is_polymorphic},
    {"__is_standard_layout", is_standard_layout},
    {"__is_trivial", is_trivial},
    {"__is_trivially_assignable", is_trivially_assignable},
    {"__is_trivially_constructible", is_trivially_constructible},
    {"__is_trivially_destructible", is_trivially_destructible},
    {"__is_trivially_copyable", is_trivially_copyable},
    {"__is_union", is_union},
    {"__is_literal_type", is_literal_type},
};

static std::unordered_map<std::string, std::unordered_map<unsigned, SYMBOL*>> integerSequences;

void libcxx_init(void)
{
    int i;
    intrinsicHash = CreateHashTable(32);
    integerSequences.clear();
    for (i = 0; i < sizeof(defaults) / sizeof(defaults[0]); i++)
        AddName((SYMBOL*)&defaults[i], intrinsicHash);
}
void libcxx_builtins(void)
{
    if (Optimizer::cparams.prm_cplusplus)
    {
        preProcessor->Define("address_sanitizer", "0");
        preProcessor->Define("__has_cxx_access_control_sfinae", "0");
        preProcessor->Define("__has_cxx_alias_templates", "1");
        preProcessor->Define("__has_cxx_alignas", "1");
        preProcessor->Define("__has_cxx_atomic", "0");
        preProcessor->Define("__has_cxx_attributes", "1");
        preProcessor->Define("__has_cxx_auto_type", "1");
        preProcessor->Define("__has_cxx_constexpr", "1");
        preProcessor->Define("__has_cxx_decltype", "1");
        preProcessor->Define("__has_cxx_defaulted_functions", "1");
        preProcessor->Define("__has_cxx_deleted_functions", "1");
        preProcessor->Define("__has_cxx_exceptions", "1");
        preProcessor->Define("__has_cxx_explicit_conversions", "1");
        preProcessor->Define("__has_cxx_generalized_initializers", "1");
        preProcessor->Define("__has_cxx_lambdas", "1");
        preProcessor->Define("__has_cxx_noexcept", "1");
        preProcessor->Define("__has_cxx_nullptr", "1");
        preProcessor->Define("__has_cxx_reference_qualified_functions", "1");
        preProcessor->Define("__has_cxx_relaxed_constexpr", "1");
        preProcessor->Define("__has_cxx_rtti", "1");
        preProcessor->Define("__has_cxx_rvalue_references", "1");
        preProcessor->Define("__has_cxx_static_assert", "1");
        preProcessor->Define("__has_cxx_strong_enums", "1");
        preProcessor->Define("__has_cxx_trailing_return", "1");
        preProcessor->Define("__has_cxx_variadic_templates", "1");
        preProcessor->Define("__has_nothrow_assign", "1");
        preProcessor->Define("__has_nothrow_constructor", "0");
        preProcessor->Define("__has_nothrow_copy", "0");
        preProcessor->Define("__has_trivial_constructor", "0");
        preProcessor->Define("__has_trivial_destructor", "0");
        preProcessor->Define("__has_virtual_destructor_xx", "0");
        preProcessor->Define("__has_is_base_of", "1");
        preProcessor->Define("__has_is_class", "1");
        preProcessor->Define("__has_is_constructible", "1");
        preProcessor->Define("__has_is_convertible_to", "1");
        preProcessor->Define("__has_is_empty", "1");
        preProcessor->Define("__has_is_enum", "1");
        preProcessor->Define("__has_is_final", "1");
        preProcessor->Define("__has_is_literal", "1");
        preProcessor->Define("__has_is_nothrow_constructible", "1");
        preProcessor->Define("__has_is_nothrow_assignable", "1");
        preProcessor->Define("__has_is_pod", "1");
        preProcessor->Define("__has_is_polymorphic", "1");
        preProcessor->Define("__has_is_standard_layout", "1");
        preProcessor->Define("__has_is_trivial", "1");
        preProcessor->Define("__has_is_trivially_assignable", "1");
        preProcessor->Define("__has_is_trivially_constructible", "1");
        preProcessor->Define("__has_is_trivially_copyable", "1");
        preProcessor->Define("__has_is_union", "1");
        preProcessor->Define("__has___reference_binds_to_temporary", "0");
        preProcessor->Define("__has___is_trivially_destructible", "1");
        preProcessor->Define("__has___is_nothrow_constructible", "1");
        preProcessor->Define("__has___is_nothrow_assignable", "1");
        preProcessor->Define("__has___nullptr", "0");
        preProcessor->Define("__has__Atomic", "1");
        preProcessor->Define("__has___is_aggregate", "0");
        preProcessor->Define("__has__builtin_isnan", "0");
        preProcessor->Define("__has__builtin_isinf", "0");
        preProcessor->Define("__has__builtin_isfinite", "0");
        preProcessor->Define("__has__atomic_load_n", "1");
        preProcessor->Define("__has__atomic_add_fetch", "1");
        preProcessor->Define("__has__atomic_exchange_n", "1");
        preProcessor->Define("__has__atomic_compare_exchange_n", "1");
        preProcessor->Define("__has__builtin_operator_new", "0");
        preProcessor->Define("__has__builtin_operator_delete", "0");
        preProcessor->Define("__has__make_integer_seq", "1");
        preProcessor->Define("__has__type_pack_element", "1");
        preProcessor->Define("__has__builtin_launder", "0");
        preProcessor->Define("__has__builtin_addressof", "0");
        preProcessor->Define("__has____no_sanitize__", "0");
        preProcessor->Define("__has____type_visibility__", "0");
        preProcessor->Define("__has__internal_linkage", "1");
        preProcessor->Define("__has__exclude_from_explicit_instantiation", "1");
        preProcessor->Define("__has____enable_if__", "0");
        preProcessor->Define("__has__deprecated", "1");
        preProcessor->Define("__has__no_destroy", "0");
        preProcessor->Define("__has__aquire_capability", "0");
        preProcessor->Define("__has__require_constant_initialization", "0");
        preProcessor->Define("__has__diagnose_if", "0");
        preProcessor->Define("__has__fallthrough", "0");
        preProcessor->Define("__has____nodebug__", "0");
        preProcessor->Define("__has__no_thread_safety_analysis", "0");
    }
}

bool parseBuiltInTypelistFunc(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    SYMLIST** hr = LookupName(sym->name, intrinsicHash);
    if (hr)
    {
        struct _ihash* p = (struct _ihash*)(*hr)->p;
        return p->func(lex, funcsp, sym, tp, exp);
    }
    return false;
}
static LEXLIST* getTypeList(LEXLIST* lex, SYMBOL* funcsp, INITLIST** lptr)
{
    *lptr = nullptr;
    do
    {
        TYPE* tp = nullptr;
        lex = getsym(); /* past ( or , */
        lex = get_type_id(lex, &tp, funcsp, sc_cast, false, true, false);
        if (!tp)
            break;
        if (basetype(tp)->type != bt_templateparam)
        {
            *lptr = Allocate<INITLIST>();
            (*lptr)->tp = tp;
            (*lptr)->exp = intNode(en_c_i, 1);
            lptr = &(*lptr)->next;
        }
        else
        {
            tp = basetype(tp);
            if (tp->templateParam->p->packed)
            {
                TEMPLATEPARAMLIST* tpl = tp->templateParam->p->byPack.pack;
                needkw(&lex, ellipse);
                while (tpl)
                {
                    if (tpl->p->byClass.val)
                    {
                        *lptr = Allocate<INITLIST>();
                        (*lptr)->tp = tpl->p->byClass.val;
                        (*lptr)->exp = intNode(en_c_i, 1);
                        lptr = &(*lptr)->next;
                    }
                    tpl = tpl->next;
                }
            }
            else
            {
                if (tp->templateParam->p->byClass.val)
                {
                    *lptr = Allocate<INITLIST>();
                    (*lptr)->tp = tp->templateParam->p->byClass.val;
                    (*lptr)->exp = intNode(en_c_i, 1);
                    lptr = &(*lptr)->next;
                }
            }
        }
        // this may be a bit naive...
        if (MATCHKW(lex, ellipse))
            lex = getsym();
    } while (MATCHKW(lex, comma));
    needkw(&lex, closepa);
    return lex;
}
static int FindBaseClassWithData(SYMBOL* sym, SYMBOL** result)
{
    SYMLIST* hr;
    int n = 0;
    BASECLASS* bc = sym->sb->baseClasses;
    while (bc)
    {
        n += FindBaseClassWithData(bc->cls, result);
        bc = bc->next;
    }
    hr = basetype(sym->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sym = hr->p;
        if (sym->sb->storage_class == sc_mutable || sym->sb->storage_class == sc_member)
        {
            if (result)
                *result = sym;
            return n + 1;
        }
        hr = hr->next;
    }
    return n;
}
static bool isStandardLayout(TYPE* tp, SYMBOL** result)
{
    if (isstructured(tp) && !hasVTab(basetype(tp)->sp) && !basetype(tp)->sp->sb->vbaseEntries)
    {
        int n;
        int access = -1;
        SYMBOL *found = nullptr, *first;
        SYMLIST* hr;
        n = FindBaseClassWithData(tp->sp, &found);
        if (n > 1)
            return false;
        if (n)
        {
            SYMBOL* first = nullptr;
            hr = basetype(found->tp)->syms->table[0];
            while (hr)
            {
                SYMBOL* sym = hr->p;
                if (!first)
                    first = sym;
                if (sym->sb->storage_class == sc_member || sym->sb->storage_class == sc_mutable)
                {
                    if (isstructured(sym->tp) && !isStandardLayout(sym->tp, nullptr))
                        return false;
                    if (access != -1)
                    {
                        if (access != sym->sb->access)
                            return false;
                    }
                    access = sym->sb->access;
                }
                hr = hr->next;
            }
            if (first && isstructured(first->tp))
            {
                BASECLASS* bc = found->sb->baseClasses;
                while (bc)
                {
                    if (comparetypes(bc->cls->tp, first->tp, true))
                        return false;
                    bc = bc->next;
                }
            }
        }
        if (result)
            *result = found;
        return true;
    }
    return false;
    /*
 hasnonon-staticdatamembersoftypenon-standard-layoutclass(orarrayofsuchtypes)orreference,
   has no virtual functions (10.3) and no virtual base classes (10.1),
   has the same access control (Clause 11) for all non-static data members,
   has no non-standard-layout base classes,
   either has no non-static data members in the most derived class and at most one base class with non-static data members,
    or has no base classes with non-static data members, and
   has no base classes of the same type as the ?rst non-static data member.
 */
}
static bool trivialFunc(SYMBOL* func, bool move)
{
    SYMLIST* hr = basetype(func->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sym = hr->p;
        if (matchesCopy(sym, move))
        {
            return sym->sb->defaulted;
        }
        hr = hr->next;
    }
    return true;
}
static bool trivialCopyConstructible(TYPE* tp, bool rref)
{
    if (isstructured(tp))
    {
        SYMLIST* hr;
        SYMBOL* ovl;
        BASECLASS* bc;
        ovl = search(overloadNameTab[CI_CONSTRUCTOR], basetype(tp)->syms);
        if (ovl)
        {
            if (!trivialFunc(ovl, rref))
                return false;
        }
        bc = basetype(tp)->sp->sb->baseClasses;
        while (bc)
        {
            if (!trivialCopyConstructible(bc->cls->tp, rref))
                return false;
            bc = bc->next;
        }
        hr = basetype(tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* sym = hr->p;
            if (sym->sb->storage_class == sc_mutable || sym->sb->storage_class == sc_member)
                if (!trivialCopyConstructible(sym->tp, rref))
                    return false;
            hr = hr->next;
        }
    }
    return true;
}
static bool trivialAssignable(TYPE* tp, bool rref)
{
    if (isstructured(tp))
    {
        SYMLIST* hr;
        SYMBOL* ovl;
        BASECLASS* bc;
        ovl = search(overloadNameTab[assign - kw_new + CI_NEW], basetype(tp)->syms);
        if (ovl)
        {
            if (!trivialFunc(ovl, rref))
                return false;
        }
        bc = basetype(tp)->sp->sb->baseClasses;
        while (bc)
        {
            if (!trivialAssignable(bc->cls->tp, rref))
                return false;
            bc = bc->next;
        }
        hr = basetype(tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* sym = hr->p;
            if (sym->sb->storage_class == sc_mutable || sym->sb->storage_class == sc_member)
                if (!trivialAssignable(sym->tp, rref))
                    return false;
            hr = hr->next;
        }
    }
    return true;
}
static bool trivialDestructor(TYPE* tp)
{
    if (isstructured(tp))
    {
        SYMLIST* hr;
        SYMBOL* ovl;
        BASECLASS* bc;
        ovl = search(overloadNameTab[CI_DESTRUCTOR], basetype(tp)->syms);
        if (ovl)
        {
            ovl = (SYMBOL*)ovl->tp->syms->table[0]->p;
            if (!ovl->sb->defaulted)
                return false;
        }
        bc = basetype(tp)->sp->sb->baseClasses;
        while (bc)
        {
            if (!trivialDestructor(bc->cls->tp))
                return false;
            bc = bc->next;
        }
        hr = basetype(tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* sym = hr->p;
            if (sym->sb->storage_class == sc_mutable || sym->sb->storage_class == sc_member)
                if (!trivialDestructor(sym->tp))
                    return false;
            hr = hr->next;
        }
    }
    return true;
}
static bool trivialDefaultConstructor(TYPE* tp)
{
    if (isstructured(tp))
    {
        SYMLIST* hr;
        SYMBOL* ovl;
        BASECLASS* bc;
        ovl = search(overloadNameTab[CI_CONSTRUCTOR], basetype(tp)->syms);
        if (ovl)
        {
            SYMLIST* hr = ovl->tp->syms->table[0];
            while (hr)
            {
                SYMBOL* sym = hr->p;
                SYMLIST* hr1 = basetype(sym->tp)->syms->table[0];

                if (!hr1->next || !hr1->next->next || (hr1->next->next->p)->tp->type == bt_void)
                {
                    if (!sym->sb->defaulted)
                        return false;
                    else
                        break;
                }
                hr = hr->next;
            }
        }
        bc = basetype(tp)->sp->sb->baseClasses;
        while (bc)
        {
            if (!trivialDefaultConstructor(bc->cls->tp))
                return false;
            bc = bc->next;
        }
        hr = basetype(tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* sym = hr->p;
            if (sym->sb->storage_class == sc_mutable || sym->sb->storage_class == sc_member)
                if (!trivialDefaultConstructor(sym->tp))
                    return false;
            hr = hr->next;
        }
    }
    return true;
}
static bool triviallyCopyable(TYPE* tp)
{
    return trivialCopyConstructible(tp, false) && trivialAssignable(tp, false) &&
        trivialCopyConstructible(tp, true) && trivialAssignable(tp, true) &&
        trivialDestructor(tp);
}
static bool trivialStructure(TYPE* tp)
{
    if (isref(tp))
        tp = basetype(tp)->btp;
    if (isstructured(tp))
    {
        return triviallyCopyable(tp) && trivialDefaultConstructor(tp);
    }
    return false;
}
/*
A trivially copyable class is a class that:
  has no non-trivial copy constructors (12.8),
  has no non-trivial move constructors (12.8),
  has no non-trivial copy assignment operators (13.5.3, 12.8),
  has no non-trivial move assignment operators (13.5.3, 12.8), and
  has a trivial destructor (12.4).

A trivial class is a class that has a trivial default constructor (12.1) and is trivially copyable.
*/
static bool trivialStructureWithBases(TYPE* tp)
{
    if (isstructured(tp))
    {
        BASECLASS* bc;
        if (!trivialStructure(tp))
            return false;
        bc = basetype(tp)->sp->sb->baseClasses;
        while (bc)
        {
            if (!trivialStructureWithBases(bc->cls->tp))
                return false;
            bc = bc->next;
        }
    }
    return true;
}
static bool isPOD(TYPE* tp)
{
    SYMBOL* found = nullptr;
    if (isStandardLayout(tp, &found) && trivialStructureWithBases(tp))  // DAL fixed
    {
        if (found)
        {
            SYMLIST* hr = basetype(found->tp)->syms->table[0];  // DAL fixed
            while (hr)
            {
                SYMBOL* sym = hr->p;
                if (isstructured(sym->tp) && !trivialStructureWithBases(sym->tp))
                    return false;
                hr = hr->next;
            }
        }
        return true;
    }
    return false;
}
static bool __is_nothrow(TYPE* tp, INITLIST* args, SYMBOL* ovl)
{
    // recursion will have been taken care of elsewhere...
    int i = 0;
    char holdl[100], holdr[100];
    INITLIST* temp;
    EXPRESSION* cexp = nullptr;
    if (ovl)
    {
        if (isref(tp))
        {
            tp = basetype(tp)->btp;
            if (args->next && !args->next->next)
            {
                TYPE* tpy = args->next->tp;
                if (isref(tpy))
                    tpy = basetype(tpy)->btp;
                if (isconst(tpy) && !isconst(tp) || isvolatile(tpy) && !isvolatile(tp))
                {
                    return false;
                }
                if (isstructured(tp))
                {
                    if (isstructured(tpy))
                    {
                        SYMBOL* sp2 = basetype(tp)->sp;
                        SYMBOL* spy = basetype(tpy)->sp;
                        if (sp2->sb->mainsym)
                            sp2 = sp2->sb->mainsym;
                        if (spy->sb->mainsym)
                            spy = spy->sb->mainsym;
                        return sp2 == spy || sameTemplate(sp2->tp, spy->tp);
                    }
                    return true;
                }
            }
        }
        FUNCTIONCALL funcparams = {};
        funcparams.thisptr = intNode(en_c_i, 0);
        funcparams.thistp = MakeType(bt_pointer, basetype(tp));
        funcparams.ascall = true;
        funcparams.arguments = args;
        temp = funcparams.arguments;
        i = 0;
        while (temp)
        {
            while (temp->tp->type == bt_typedef)
                temp->tp = temp->tp->btp;
            bool rref = isstructured(temp->tp);
            if (isref(temp->tp) && !isstructured(basetype(temp->tp)->btp))
                temp->tp = basetype(temp->tp)->btp;
            holdl[i] = temp->tp->lref;
            holdr[i] = temp->tp->rref;
            if (rref)
            {
                temp->tp->rref = true;
                temp->tp->lref = false;
            }
            else
            {
                temp->tp->lref = false;
                temp->tp->rref = false;
            }
            i++;
            temp = temp->next;
        }
        std::stack<SYMBOL*> stk;
        for (auto spl = ovl->tp->syms->table[0]; spl; spl = spl->next)
        {
            if (spl->p->templateParams)
            {
                stk.push(spl->p);
                PushPopTemplateArgs(spl->p, true);
            }
        }
        int oldSpecialize = inTemplateSpecialization;
        inTemplateSpecialization = 0;
        SYMBOL* sp = GetOverloadedFunction(&tp, &funcparams.fcall, ovl, &funcparams, nullptr, false, false, true,
                                           _F_SIZEOF | _F_IS_NOTHROW | _F_RETURN_DELETED);
        inTemplateSpecialization = oldSpecialize;
        while (stk.size())
        {
            PushPopTemplateArgs(stk.top(), false);
            stk.pop();
        }
        if (sp && sp->sb->defaulted && !sp->sb->inlineFunc.stmt)
        {
            inNoExceptHandler++;
            if (!strcmp(ovl->name, overloadNameTab[CI_CONSTRUCTOR]))
                createConstructor(sp->sb->parentClass, sp);
            else
                createAssignment(sp->sb->parentClass, sp);
            inNoExceptHandler--;
        }
        temp = funcparams.arguments;
        i = 0;
        while (temp)
        {
            temp->tp->lref = holdl[i];
            temp->tp->rref = holdr[i];
            temp = temp->next;
            i++;
        }
        bool rv = sp && sp->sb->noExcept && sp->sb->access == ac_public;
        return rv;
    }
    return true;
}
static bool nothrowConstructible(TYPE* tp, INITLIST* args)
{
    if (isstructured(tp))
    {
        return __is_nothrow(tp, args, search(overloadNameTab[CI_CONSTRUCTOR], basetype(tp)->syms));
    }
    return true;
}
static bool nothrowAssignable(TYPE* tp, INITLIST* args)
{
    if (isstructured(tp))
    {
        return __is_nothrow(tp, args, search(overloadNameTab[CI_ASSIGN], basetype(tp)->syms));
    }
    return true;
}
static bool is_abstract(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        rv = isstructured(funcparams.arguments->tp) && basetype(funcparams.arguments->tp)->sp->sb->isabstract;
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_base_of(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments && funcparams.arguments->next && !funcparams.arguments->next->next)
    {
        if (isstructured(funcparams.arguments->tp) && isstructured(funcparams.arguments->next->tp))
            rv = classRefCount(basetype(funcparams.arguments->tp)->sp, basetype(funcparams.arguments->next->tp)->sp) != 0;
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_class(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        rv = isstructured(funcparams.arguments->tp) && basetype(funcparams.arguments->tp)->type != bt_union;
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_constructible(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments)
    {
        TYPE* tp2 = funcparams.arguments->tp;
        if (isarray(tp2))
        {
            while (isarray(tp2) && tp2->size != 0)
                tp2 = tp2->btp;

            if (isarray(tp2))
            {
                tp2 = nullptr;
            }
        }
        if (tp2)
        {
            TYPE* tpf = tp2;
            if (isref(tp2))
            {
                tp2 = basetype(tp2)->btp;
                if (funcparams.arguments->next && !funcparams.arguments->next->next)
                {
                    TYPE* tpy = funcparams.arguments->next->tp;
                    if (isref(tpy))
                        tpy = basetype(tpy)->btp;
                    if (isconst(tpy) && !isconst(tp2) || isvolatile(tpy) && !isvolatile(tp2))
                    {
                        rv = false;
                        *exp = intNode(en_c_i, rv);
                        *tp = &stdint;
                        return true;
                    }
                    if (isstructured(tp2))
                    {
                        bool rv = true;
                        if (isstructured(tpy))
                        {
                            SYMBOL* sp2 = basetype(tp2)->sp;
                            SYMBOL* spy = basetype(tpy)->sp;
                            if (sp2->sb->mainsym)
                                sp2 = sp2->sb->mainsym;
                            if (spy->sb->mainsym)
                                spy = spy->sb->mainsym;
                            rv = sp2 == spy || sameTemplate(sp2->tp, spy->tp);
                        }
                        *exp = intNode(en_c_i, rv);
                        *tp = &stdint;
                        return rv;
                    }
                }
            }
            if (isfunction(tp2))
            {
                if (funcparams.arguments->next && !funcparams.arguments->next->next)
                {
                    TYPE* tpy = funcparams.arguments->next->tp;
                    if (isref(tpf))
                    {
                        if (isref(tpy))
                            tpy = basetype(tpy)->btp;
                        rv = comparetypes(tp2, tpy, true);
                    }
                }
            }
            else if (isfuncptr(tp2))
            {
                if (funcparams.arguments->next && !funcparams.arguments->next->next)
                {
                    TYPE* tp3 = funcparams.arguments->next->tp;
                    if (isref(tp3))
                        tp3 = basetype(basetype(tp3)->btp);
                    if (isfunction(tp3))
                    {
                        rv = comparetypes(basetype(tp2)->btp, tp3, true);
                    }
                    else if (isstructured(tp3))
                    {
                        // look for operator () with args from tp2
                        SYMLIST* hr;
                        EXPRESSION* cexp = nullptr;
                        INITLIST** arg = &funcparams.arguments;
                        SYMBOL* bcall = search(overloadNameTab[CI_FUNC], basetype(tp3)->syms);
                        funcparams.thisptr = intNode(en_c_i, 0);
                        funcparams.thistp = MakeType(bt_pointer, basetype(tp3));
                        funcparams.ascall = true;
                        funcparams.arguments = nullptr;
                        funcparams.sp = nullptr;
                        hr = basetype(basetype(tp2)->btp)->syms->table[0];
                        while (hr)
                        {
                            *arg = Allocate<INITLIST>();
                            (*arg)->tp = hr->p->tp;
                            (*arg)->exp = intNode(en_c_i, 0);
                            arg = &(*arg)->next;
                            hr = hr->next;
                        }
                        auto spx = GetOverloadedFunction(tp, &funcparams.fcall, bcall, &funcparams, nullptr, false, false, false,
                                                         _F_SIZEOF | _F_RETURN_DELETED);
                        rv = spx && spx->sb->access == ac_public && !spx->sb->deleted;
                    }
                    else
                    {
                        // compare two function pointers...
                        rv = comparetypes(tp2, tp3, true);
                    }
                }
                else
                {
                    rv = true;
                }
            }
            else if (basetype(tp2)->type == bt_memberptr)
            {
                if (funcparams.arguments->next && !funcparams.arguments->next->next)
                {
                    TYPE* tp3 = funcparams.arguments->next->tp;
                    if (isref(tp3))
                        tp3 = basetype(basetype(tp3)->btp);
                    if (tp3->type == bt_memberptr)
                    {
                        tp2 = basetype(tp2);
                        SYMBOL* s1 = tp2->sp;
                        SYMBOL* s2 = tp3->sp;
                        if (s1->sb->mainsym)
                            s1 = s1->sb->mainsym;
                        if (s2->sb->mainsym)
                            s2 = s2->sb->mainsym;
                        if (s1 == s2 || sameTemplate(s1->tp, s2->tp))
                        {
                            rv = comparetypes(tp2->btp, tp3->btp, true);
                        }
                    }
                    else if (isfunction(tp3))
                    {
                        tp2 = basetype(tp2);
                        SYMBOL* s1 = tp2->sp;
                        SYMBOL* s2 = tp3->sp;
                        if (s2)
                        {
                            SYMLIST* hr = s2->tp->syms->table[0];
                            if (hr && hr->p->sb->thisPtr)
                            {
                                s2 = basetype(basetype(hr->p->tp)->btp)->sp;
                                if (s1->sb->mainsym)
                                    s1 = s1->sb->mainsym;
                                if (s2->sb->mainsym)
                                    s2 = s2->sb->mainsym;
                                if (s1 == s2 || sameTemplate(s1->tp, s2->tp))
                                {
                                    rv = comparetypes(tp2->btp, tp3, true);
                                }
                            }
                        }
                    }
                }
                else
                {
                    rv = true;
                }
            }
            else if (isarithmetic(tp2) || ispointer(tp2) || basetype(tp2)->type == bt_enum)
            {
                if (!funcparams.arguments->next)
                {
                    rv = true;
                }
                else if (!funcparams.arguments->next->next)
                {
                    rv = comparetypes(tp2, funcparams.arguments->next->tp, false);
                }
            }
            else if (isstructured(tp2))
            {
                TYPE* tp3 = NULL;
                tp2 = basetype(tp2)->sp->tp;
                if (funcparams.arguments->next)
                {
                    tp3 = funcparams.arguments->next->tp;
                }
#if 0
                if (tp3 && isstructured(tp3) &&
                    (comparetypes(tp2, tp3, true) || sameTemplate(tp2, tp3)))
                {
                    rv = true;
                }
                else
#endif
                {
                    int i = 0;
                    char holdl[100], holdr[100];
                    INITLIST* temp;
                    EXPRESSION* cexp = nullptr;
                    SYMBOL* cons = search(overloadNameTab[CI_CONSTRUCTOR], basetype(tp2)->syms);
                    if (cons)
                    {
                        funcparams.thisptr = intNode(en_c_i, 0);
                        funcparams.thistp = MakeType(bt_pointer, basetype(tp2));
                        funcparams.ascall = true;
                        funcparams.arguments = funcparams.arguments->next;
                        temp = funcparams.arguments;
                        i = 0;
                        while (temp)
                        {
                            while (temp->tp->type == bt_typedef)
                                temp->tp = temp->tp->btp;
                            bool rref = isstructured(temp->tp);
                            if (isref(temp->tp) && !isstructured(basetype(temp->tp)->btp))
                                temp->tp = basetype(temp->tp)->btp;
                            holdl[i] = temp->tp->lref;
                            holdr[i] = temp->tp->rref;
                            if (rref)
                            {
                                temp->tp->rref = true;
                                temp->tp->lref = false;
                            }
                            else
                            {
                                temp->tp->lref = false;
                                temp->tp->rref = false;
                            }
                            i++;
                            temp = temp->next;
                        }
                        std::stack<SYMBOL*> stk;
                        for (auto spl = cons->tp->syms->table[0]; spl; spl = spl->next)
                        {
                            if (spl->p->templateParams)
                            {
                                stk.push(spl->p);
                                PushPopTemplateArgs(spl->p, true);
                            }
                        }
                        auto sym = GetOverloadedFunction(tp, &funcparams.fcall, cons, &funcparams, nullptr, false, false, false,
                                                         _F_SIZEOF);
                        rv = sym && sym->sb->access == ac_public;

                        while (stk.size())
                        {
                            PushPopTemplateArgs(stk.top(), false);
                            stk.pop();
                        }
                        temp = funcparams.arguments;
                        i = 0;
                        while (temp)
                        {
                            temp->tp->lref = holdl[i];
                            temp->tp->rref = holdr[i];
                            temp = temp->next;
                            i++;
                        }
                    }
                }
            }
        }
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_convertible_to(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    bool rv = true;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    if (funcparams.arguments && funcparams.arguments->next && !funcparams.arguments->next->next)
    {
        TYPE* from = funcparams.arguments->tp;
        TYPE* to = funcparams.arguments->next->tp;
        if (isref(from))
        {
            if (isref(to))
            {
                if (basetype(to)->type == bt_lref)
                    if (basetype(from)->type == bt_rref)
                        rv = false;
            }
        }
        if (isfunction(from))
            from = basetype(from)->btp;
        if (rv)
        {
            while (isref(from))
                from = basetype(from)->btp;
            while (isref(to))
                to = basetype(to)->btp;
            while (ispointer(from) && ispointer(to))
            {
                from = basetype(from)->btp;
                to = basetype(to)->btp;
            }
            if (to->type == bt_templateparam)
                to = to->templateParam->p->byClass.val;
            if (from->type == bt_templateparam)
                from = from->templateParam->p->byClass.val;
            rv = comparetypes(to, from, false);
            if (!rv && isstructured(from) && isstructured(to))
            {
                if (classRefCount(basetype(to)->sp, basetype(from)->sp) == 1)
                    rv = true;
                else if (lookupGenericConversion(basetype(from)->sp, basetype(to)))
                    rv = true;
            }
            if (!rv && isstructured(from))
            {
                CI_CONSTRUCTOR;
                SYMBOL* sym = search(overloadNameTab[CI_CAST], basetype(from)->syms);
                if (sym)
                {
                    SYMLIST* hr = sym->tp->syms->table[0];
                    while (hr)
                    {
                        if (comparetypes(basetype(hr->p->tp)->btp, to, false))
                        {
                            rv = true;
                            break;
                        }
                        hr = hr->next;
                    }
                }
            }
        }
    }
    else
    {
        rv = false;
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_empty(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        if (isstructured(funcparams.arguments->tp))
            rv = !basetype(funcparams.arguments->tp)->syms->table[0] || !basetype(funcparams.arguments->tp)->syms->table[0]->next;
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_enum(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        rv = basetype(funcparams.arguments->tp)->type == bt_enum;
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_final(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        if (isstructured(funcparams.arguments->tp))
            rv = basetype(funcparams.arguments->tp)->sp->sb->isfinal;
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_literal(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        // yes references are literal types...
        rv = !isstructured(funcparams.arguments->tp);
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_nothrow_constructible(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments)
    {
        TYPE* tp2 = funcparams.arguments->tp;
        if (isref(tp2) && funcparams.arguments->next)
        {
            tp2 = basetype(tp2)->btp;
            TYPE* tpy = funcparams.arguments->next->tp;
            if (isref(tpy))
                tpy = basetype(tpy)->btp;
            if (isconst(tpy) && !isconst(tp2) || isvolatile(tpy) && !isvolatile(tp2))
            {
                rv = false;
                *exp = intNode(en_c_i, rv);
                *tp = &stdint;
                return true;
            }
        }
        if (isstructured(tp2))
        {
            if (!basetype(tp2)->sp->sb->trivialCons)
                rv = nothrowConstructible(funcparams.arguments->tp, funcparams.arguments->next);
            else if (funcparams.arguments->next)
                rv = comparetypes(tp2, funcparams.arguments->next->tp, true);
            else
                rv = true;
        }
        else if (funcparams.arguments->next)
        {
            rv = comparetypes(tp2, funcparams.arguments->next->tp, true);
        }
        else
        {
            rv = true;
        }
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_nothrow_assignable(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments)
    {
        TYPE* tp2 = funcparams.arguments->tp;
        if (isref(tp2) && funcparams.arguments->next)
        {
            tp2 = basetype(tp2)->btp;
            TYPE* tpy = funcparams.arguments->next->tp;
            if (isref(tpy))
                tpy = basetype(tpy)->btp;
            if (isconst(tpy) && !isconst(tp2) || isvolatile(tpy) && !isvolatile(tp2))
            {
                rv = false;
                *exp = intNode(en_c_i, rv);
                *tp = &stdint;
                return true;
            }
        }
        if (isstructured(tp2))
        {
            rv = nothrowAssignable(tp2, funcparams.arguments->next);
        }
        else if (funcparams.arguments->next)
        {
            rv = comparetypes(tp2, funcparams.arguments->next->tp, true);
        }
        else
        {
            rv = true;
        }
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_pod(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        rv = isarithmetic(funcparams.arguments->tp) || !!isPOD(funcparams.arguments->tp);
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_polymorphic(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        // yes references are literal types...
        if (isstructured(funcparams.arguments->tp))
            rv = !!hasVTab(basetype(funcparams.arguments->tp)->sp);
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_standard_layout(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        rv = isarithmetic(funcparams.arguments->tp) || !!isStandardLayout(funcparams.arguments->tp, nullptr);
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_trivial(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        rv = !isstructured(funcparams.arguments->tp) || !!trivialStructure(funcparams.arguments->tp);
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_trivially_assignable(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments && isstructured(funcparams.arguments->tp))
    {
        if (funcparams.arguments->next && !funcparams.arguments->next->next)
        {
           TYPE* tp1 = funcparams.arguments->next->tp;
           if (isref(tp1))
               tp1 = basetype(tp1)->btp;
           if (comparetypes(tp1, funcparams.arguments->tp, true) || sameTemplate(tp1, funcparams.arguments->tp))
              rv = trivialAssignable(funcparams.arguments->tp, basetype(funcparams.arguments->next->tp)->type== bt_rref);
        }
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_trivially_constructible(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments && isstructured(funcparams.arguments->tp))
    {
        if (!funcparams.arguments->next)
           rv = trivialDefaultConstructor(funcparams.arguments->tp);
        else if (!funcparams.arguments->next->next)
        {
           TYPE* tp1 = funcparams.arguments->next->tp;
           if (isref(tp1))
               tp1 = basetype(tp1)->btp;
           if (comparetypes(tp1, funcparams.arguments->tp, true) || sameTemplate(tp1, funcparams.arguments->tp))
              rv = trivialCopyConstructible(funcparams.arguments->tp, basetype(funcparams.arguments->next->tp)->type== bt_rref);
        }
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_trivially_destructible(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        rv = trivialDestructor(funcparams.arguments->tp);
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_trivially_copyable(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    lst = funcparams.arguments;
    while (lst)
    {
        lst->tp = PerformDeferredInitialization(lst->tp, nullptr);
        lst = lst->next;
    }
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        if (isstructured(funcparams.arguments->tp))
            rv = triviallyCopyable(funcparams.arguments->tp);
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool is_union(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        rv = basetype(funcparams.arguments->tp)->type == bt_union;
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
static bool hasConstexprConstructor(TYPE* tp)
{
    auto ovl = search(overloadNameTab[CI_CONSTRUCTOR], basetype(tp)->syms);
    if (ovl)
    {
        auto hr = ovl->tp->syms->table[0];
        while (hr)
        {
            if (hr->p->sb->constexpression)
                return true;
            hr = hr->next;
        }
    }
    return false;
}
bool is_literal_type(TYPE* tp);
static void nonStaticLiteralTypes(TYPE* tp, bool& all, bool& one)
{
    auto hr = basetype(tp)->syms->table[0];
    if (hr)
    {
        hr = hr->next;  // skip shim
        while (hr)
        {
            auto sym = hr->p;
            if (sym->sb->storage_class == sc_member || sym->sb->storage_class == sc_mutable)
            {
                if (isvolatile(sym->tp))
                {
                    all = false;
                }
                else
                {
                    if (is_literal_type(sym->tp))
                        one = true;
                    else
                        all = false;
                }
            }
            hr = hr->next;
        }
    }
}
bool is_literal_type(TYPE* tp)
{
    if (isref(tp) || isarithmetic(tp) || isvoid(tp))
        return true;
    if (isarray(tp))
        return is_literal_type(tp->btp);
    if (isstructured(tp))
    {
        if (trivialDestructor(tp))
        {
            if (basetype(tp)->sp->sb->trivialCons || hasConstexprConstructor(tp))
            {
                bool all = true, one = false;
                nonStaticLiteralTypes(tp, all, one);
                if (basetype(tp)->type == bt_union && one)
                    return true;
                else if (all)
                    return true;
            }
        }
    }
    return false;
}
static bool is_literal_type(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        rv = is_literal_type(funcparams.arguments->tp);
    }
    *exp = intNode(en_c_i, rv);
    *tp = &stdint;
    return true;
}
bool underlying_type(LEXLIST** lex, SYMBOL* funcsp, SYMBOL* sym, TYPE** tp, EXPRESSION** exp)
{
    INITLIST* lst;
    bool rv = false;
    FUNCTIONCALL funcparams;
    memset(&funcparams, 0, sizeof(funcparams));
    funcparams.sp = sym;
    *lex = getTypeList(*lex, funcsp, &funcparams.arguments);
    if (funcparams.arguments && !funcparams.arguments->next)
    {
        *tp = funcparams.arguments->tp;
        if (basetype(*tp)->type == bt_enum)
            *tp = basetype(*tp)->btp;
    }
    else
    {
        *tp = &stdint;
    }
    return true;
}
static SYMBOL* MakeIntegerSeqType(SYMBOL* sp, TEMPLATEPARAMLIST* args)
{
    EXPRESSION* e = nullptr;
    if (args->next && args->next->next)
    {
        e = args->next->next->p->byNonType.dflt;
    }
    if (e && isintconst(e))
    {
        SYMBOL* tpl = args->p->byTemplate.dflt;
        if (tpl->sb->parentTemplate)
            tpl = tpl->sb->parentTemplate;
        int nt = basetype(args->next->p->byClass.dflt)->type + e->v.i;
        const char* nm = args->p->byTemplate.dflt->name;
        auto sym = integerSequences[nm][nt];
        if (sym)
            return sym;
        int n = e->v.i;
        decltype(args) args1;
        args1 = Allocate<TEMPLATEPARAMLIST>();
        args1->p = Allocate<TEMPLATEPARAM>();
        args1->p->type = kw_new;
        args1->next = Allocate<TEMPLATEPARAMLIST>();
        args1->next->p = args->next->p;
        args1->next->next = Allocate<TEMPLATEPARAMLIST>();
        args1->next->next->p = Allocate<TEMPLATEPARAM>();
        args1->next->next->p->type = kw_int;
        args1->next->next->p->byNonType.tp = args1->next->p->byClass.dflt;
        args1->next->next->p->packed = true;
        auto last = &args1->next->next->p->byPack.pack;
        for (int i = 0; i < n; i++, last = &(*last)->next)
        {
            *last = Allocate<TEMPLATEPARAMLIST>();
            (*last)->p = Allocate<TEMPLATEPARAM>();
            (*last)->p->type = kw_int;
            (*last)->p->byNonType.tp = args1->next->p->byClass.dflt;
            (*last)->p->byNonType.val = intNode(en_c_i, i);
        }
        sym = GetClassTemplate(tpl, args1->next, false);
        if (sym)
        {
            auto sym1 = TemplateClassInstantiateInternal(sym, args1, false);
            if (sym1)
            {
                integerSequences[nm][nt] = sym1;
                return sym1;
            }
            return sym;
        }
        return tpl;
    }
    return nullptr;
}
SYMBOL* MakeIntegerSeq(SYMBOL* sym, TEMPLATEPARAMLIST* args)
{
    SYMBOL* rv = CopySymbol(sym);
    rv->sb->mainsym = sym;
    auto rs = MakeIntegerSeqType(sym, args);
    if (rs)
        rv->tp = rs->tp;
    return rv;
}
static TYPE* TypePackElementType(SYMBOL* sym, TEMPLATEPARAMLIST* args)
{
    auto tpl = args;
    if (args->p->packed)
    {
        tpl = args->p->byPack.pack;
        if (!tpl)
        {
            return &stdany;
        }
    }
    auto e = tpl->p->byNonType.val;
    if (!e)
        e = tpl->p->byNonType.dflt;
    if (e && isintconst(e))
    {
        int n = e->v.i;
        TEMPLATEPARAMLIST* lst;
        if (!args->next->p->packed)
        {
            if (n == 0)
            {
                lst = args->next;
            }
            else
            {
                lst = nullptr;
            }
        }
        else
        {
            lst = args->next->p->byPack.pack;
        }
        for (; n && lst; n--)
        {
            lst = lst->next;
        }
        if (lst)
        {
            return MakeType(bt_derivedfromtemplate, lst->p->byClass.val ? lst->p->byClass.val : lst->p->byClass.dflt);
        }
        else
        {
            return &stdany;
        }
    }
    else
    {
        return &stdany;
    }
}
SYMBOL* TypePackElementCls(SYMBOL* sym, TEMPLATEPARAMLIST* args)
{
    SYMBOL* rv = CopySymbol(sym);
    rv->sb->mainsym = sym;
    rv->tp = CopyType(sym->tp);
    rv->tp->syms = CreateHashTable(1);
    rv->tp->syms->table[0] = Allocate<SYMLIST>();
    rv->tp->syms->table[0]->p = CopySymbol(rv);
    rv->tp->syms->table[0]->next = Allocate<SYMLIST>();
    auto tp1 = MakeType(bt_typedef, TypePackElementType(sym, args));
    auto sym1 = makeID(sc_typedef, tp1, nullptr, "type");
    rv->tp->syms->table[0]->next->p = sym1;
    return rv;
}
SYMBOL* TypePackElement(SYMBOL* sym, TEMPLATEPARAMLIST* args)
{
    SYMBOL* rv = CopySymbol(sym);
    rv->sb->mainsym = sym;
    rv->tp = TypePackElementType(sym, args);
    return rv;
}
}  // namespace Parser