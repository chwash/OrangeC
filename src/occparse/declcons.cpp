/* Software License Agreement
 *
 *     Copyright(C) 1994-2022 David Lindauer, (LADSoft)
 *
 *     This file is part of the Orange C Compiler package.
 *
 *     The Orange C Compiler package is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     The Orange C Compiler package is distributed in the hope that it will be useful,
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
#include "rtti.h"
#include <stack>
#include <unordered_set>
#include "ccerr.h"
#include "config.h"
#include "template.h"
#include "stmt.h"
#include "mangle.h"
#include "symtab.h"
#include "initbackend.h"
#include "expr.h"
#include "help.h"
#include "lex.h"
#include "cpplookup.h"
#include "declcpp.h"
#include "declare.h"
#include "constopt.h"
#include "memory.h"
#include "inline.h"
#include "init.h"
#include "beinterf.h"
#include "types.h"
#include "lex.h"
#include "declcons.h"
#include "libcxx.h"

namespace Parser
{
std::set<SYMBOL*> defaultRecursionMap;
bool noExcept = true;

static void genAsnCall(BLOCKDATA* b, SYMBOL* cls, SYMBOL* base, int offset, EXPRESSION* thisptr, EXPRESSION* other, bool move,
                       bool isconst);
void createDestructor(SYMBOL* sp);

void ConsDestDeclarationErrors(SYMBOL* sp, bool notype)
{
    if (sp->sb->isConstructor)
    {
        if (!notype)
            error(ERR_CONSTRUCTOR_OR_DESTRUCTOR_NO_TYPE);
        else if (sp->sb->storage_class == sc_virtual)
            errorstr(ERR_INVALID_STORAGE_CLASS, "virtual");
        else if (sp->sb->storage_class == sc_static)
            errorstr(ERR_INVALID_STORAGE_CLASS, "static");
        else if (isconst(sp->tp) || isvolatile(sp->tp))
            error(ERR_CONSTRUCTOR_OR_DESTRUCTOR_NO_CONST_VOLATILE);
    }
    else if (sp->sb->isDestructor)
    {
        if (!notype)
            error(ERR_CONSTRUCTOR_OR_DESTRUCTOR_NO_TYPE);
        else if (sp->sb->storage_class == sc_static)
            errorstr(ERR_INVALID_STORAGE_CLASS, "static");
        else if (isconst(sp->tp) || isvolatile(sp->tp))
            error(ERR_CONSTRUCTOR_OR_DESTRUCTOR_NO_CONST_VOLATILE);
    }
    else if (sp->sb->parentClass && !strcmp(sp->name, sp->sb->parentClass->name))
    {
        error(ERR_CONSTRUCTOR_OR_DESTRUCTOR_NO_TYPE);
    }
}
static bool HasConstexprConstructorInternal(SYMBOL* sym)
{
    sym = search(overloadNameTab[CI_CONSTRUCTOR], sym->tp->syms);
    if (sym)
    {
        auto hr = sym->tp->syms->table[0];
        while (hr)
        {
            if (hr->p->sb->constexpression)
                return true;
            hr = hr->next;
        }
    }
    return false;
}
static bool HasConstexprConstructor(TYPE* tp)
{
    auto sym = basetype(tp)->sp;
    if (HasConstexprConstructorInternal(sym))
        return true;
    auto hr = sym->sb->specializations;
    while (hr)
    {
        if (HasConstexprConstructorInternal(hr->p))
            return true;
        hr = hr->next;
    }
    return false;
}
void ConstexprMembersNotInitializedErrors(SYMBOL* cons)
{
    if (!templateNestingCount || instantiatingTemplate)
    {
        auto hrcons = cons->tp->syms->table[0];
        while (hrcons)
        {
            auto sym = hrcons->p;
            if (sym->sb->constexpression)
            {
                std::unordered_set<std::string> initialized;
                auto m = sym->sb->memberInitializers;
                while (m)
                {
                    initialized.insert(m->name);
                    m = m->next;
                }
                auto hr = sym->sb->parentClass->tp->syms->table[0];
                while (hr)
                {
                    if (!hr->p->sb->init && ismemberdata(hr->p))
                    {
                        if (initialized.find(hr->p->name) == initialized.end())
                        {
                            // this should check the actual base class constructor in use
                            // but that would be difficult to get at this point.
                            if (!isstructured(hr->p->tp) || !HasConstexprConstructor(hr->p->tp))
                                errorsym(ERR_CONSTEXPR_MUST_INITIALIZE, hr->p);
                        }
                    }
                    hr = hr->next;
                }
            }
            hrcons = hrcons->next;
        }
    }
}
LEXLIST* FindClass(LEXLIST* lex, SYMBOL* funcsp, SYMBOL** sym)
{
    SYMBOL* encloser = nullptr;
    NAMESPACEVALUELIST* ns = nullptr;
    bool throughClass = false;
    TYPE* castType = nullptr;
    char buf[512];
    int ov = 0;
    bool namespaceOnly = false;

    *sym = nullptr;

    if (MATCHKW(lex, classsel))
        namespaceOnly = true;
    lex = nestedPath(lex, &encloser, &ns, &throughClass, true, sc_global, false);
    lex = getIdName(lex, funcsp, buf, &ov, &castType);
    if (buf[0])
    {
        *sym = finishSearch(buf, encloser, ns, false, throughClass, namespaceOnly);  // undefined in local context
    }
    return lex;
}
MEMBERINITIALIZERS* GetMemberInitializers(LEXLIST** lex2, SYMBOL* funcsp, SYMBOL* sym)
{
    (void)sym;
    LEXLIST *lex = *lex2, *last = nullptr;
    MEMBERINITIALIZERS *first = nullptr, **cur = &first;
    //    if (sym->name != overloadNameTab[CI_CONSTRUCTOR])
    //        error(ERR_INITIALIZER_LIST_REQUIRES_CONSTRUCTOR);
    while (lex != nullptr)
    {
        if (ISID(lex) || MATCHKW(lex, classsel))
        {
            SYMBOL* sym = nullptr;
            lex = FindClass(lex, funcsp, &sym);
            LEXLIST** mylex;
            char name[1024];
            *cur = Allocate<MEMBERINITIALIZERS>();
            (*cur)->line = lex->data->errline;
            (*cur)->file = lex->data->errfile;
            mylex = &(*cur)->initData;
            name[0] = 0;
            if (ISID(lex))
            {
                strcpy(name, lex->data->value.s.a);
                lex = getsym();
            }
            (*cur)->name = litlate(name);
            if (sym && istype(sym))
                (*cur)->basesym = sym;
            if (MATCHKW(lex, lt))
            {
                int paren = 0, tmpl = 0;
                *mylex = Allocate<LEXLIST>();
                **mylex = *lex;
                (*mylex)->prev = last;
                last = *mylex;
                mylex = &(*mylex)->next;
                lex = getsym();
                while (lex && (!MATCHKW(lex, gt) || paren || tmpl))
                {
                    if (MATCHKW(lex, openpa))
                        paren++;
                    if (MATCHKW(lex, closepa))
                        paren--;
                    if (!paren && MATCHKW(lex, lt))
                        tmpl++;
                    if (!paren && (MATCHKW(lex, gt) || MATCHKW(lex, rightshift)))
                        tmpl--;
                    if (lex->data->type == l_id)
                        lex->data->value.s.a = litlate(lex->data->value.s.a);
                    *mylex = Allocate<LEXLIST>();
                    if (MATCHKW(lex, rightshift))
                    {
                        lex = getGTSym(lex);
                        **mylex = *lex;
                        (*mylex)->data = Allocate<LEXEME>();
                        *(*mylex)->data = *lex->data;
                    }
                    else
                    {
                        **mylex = *lex;
                        lex = getsym();
                    }
                    (*mylex)->prev = last;
                    last = *mylex;
                    mylex = &(*mylex)->next;
                }
                if (MATCHKW(lex, gt))
                {
                    *mylex = Allocate<LEXLIST>();
                    **mylex = *lex;
                    (*mylex)->prev = last;
                    last = *mylex;
                    mylex = &(*mylex)->next;
                    lex = getsym();
                }
            }
            if (MATCHKW(lex, openpa) || MATCHKW(lex, begin))
            {
                enum e_kw open = KW(lex), close = open == openpa ? closepa : end;
                int paren = 0;
                *mylex = Allocate<LEXLIST>();
                **mylex = *lex;
                (*mylex)->prev = last;
                last = *mylex;
                mylex = &(*mylex)->next;
                lex = getsym();
                while (lex && (!MATCHKW(lex, close) || paren))
                {
                    if (MATCHKW(lex, open))
                        paren++;
                    if (MATCHKW(lex, close))
                        paren--;
                    if (lex->data->type == l_id)
                        lex->data->value.s.a = litlate(lex->data->value.s.a);
                    *mylex = Allocate<LEXLIST>();
                    **mylex = *lex;
                    (*mylex)->prev = last;
                    last = *mylex;
                    mylex = &(*mylex)->next;
                    lex = getsym();
                }
                if (MATCHKW(lex, close))
                {
                    *mylex = Allocate<LEXLIST>();
                    **mylex = *lex;
                    (*mylex)->prev = last;
                    last = *mylex;
                    mylex = &(*mylex)->next;
                    lex = getsym();
                }
                if (MATCHKW(lex, ellipse))
                {
                    (*cur)->packed = true;
                    lex = getsym();
                }
                (*mylex) = nullptr;
            }
            else
            {
                error(ERR_MEMBER_INITIALIZATION_REQUIRED);
                skip(&lex, closepa);
                break;
            }
            cur = &(*cur)->next;
        }
        else
        {
            error(ERR_MEMBER_NAME_REQUIRED);
        }
        if (!MATCHKW(lex, comma))
            break;
        lex = getsym();
    }
    *lex2 = lex;
    return first;
}
void SetParams(SYMBOL* cons)
{
    // c style only
    SYMLIST* params = basetype(cons->tp)->syms->table[0];
    int base = Optimizer::chosenAssembler->arch->retblocksize;
    if (isstructured(basetype(cons->tp)->btp) || basetype(basetype(cons->tp)->btp)->type == bt_memberptr)
    {
        // handle structured return values
        base += getSize(bt_pointer);
        if (base % Optimizer::chosenAssembler->arch->parmwidth)
            base += Optimizer::chosenAssembler->arch->parmwidth - base % Optimizer::chosenAssembler->arch->parmwidth;
    }
    if (ismember(cons))
    {
        // handle 'this' pointer
        assignParam(cons, &base, (SYMBOL*)params->p);
        params = params->next;
    }
    while (params)
    {
        assignParam(cons, &base, (SYMBOL*)params->p);
        params = params->next;
    }
    cons->sb->paramsize = base - Optimizer::chosenAssembler->arch->retblocksize;
}
SYMBOL* insertFunc(SYMBOL* sp, SYMBOL* ovl)
{
    SYMBOL* funcs = search(ovl->name, basetype(sp->tp)->syms);
    ovl->sb->parentClass = sp;
    ovl->sb->internallyGenned = true;
    ovl->sb->attribs.inheritable.linkage4 = lk_virtual;
    ovl->sb->defaulted = true;
    ovl->sb->access = ac_public;
    ovl->sb->templateLevel = templateNestingCount;
    if (!ovl->sb->decoratedName)
        SetLinkerNames(ovl, lk_cdecl);
    if (!funcs)
    {
        TYPE* tp = MakeType(bt_aggregate);
        funcs = makeID(sc_overloads, tp, 0, ovl->name);
        funcs->sb->parentClass = sp;
        tp->sp = funcs;
        SetLinkerNames(funcs, lk_cdecl);
        insert(funcs, basetype(sp->tp)->syms);
        funcs->sb->parent = sp;
        funcs->tp->syms = CreateHashTable(1);
        insert(ovl, funcs->tp->syms);
        ovl->sb->overloadName = funcs;
    }
    else if (funcs->sb->storage_class == sc_overloads)
    {
        insertOverload(ovl, funcs->tp->syms);
        ovl->sb->overloadName = funcs;
    }
    else
    {
        diag("insertFunc: invalid overload tab");
    }
    injectThisPtr(ovl, basetype(ovl->tp)->syms);
    SetParams(ovl);
    return ovl;
}
static bool BaseWithVirtualDestructor(SYMBOL* sp)
{
    BASECLASS* b = sp->sb->baseClasses;
    while (b)
    {
        SYMBOL* dest = search(overloadNameTab[CI_DESTRUCTOR], b->cls->tp->syms);
        if (dest)
        {
            dest = dest->tp->syms->table[0]->p;
            if (dest->sb->storage_class == sc_virtual)
                return true;
        }
        b = b->next;
    }
    return false;
}
static SYMBOL* declareDestructor(SYMBOL* sp)
{
    SYMBOL* rv;
    SYMBOL *func, *sp1;
    TYPE* tp = MakeType(bt_func, MakeType(bt_void));
    VBASEENTRY* e;
    BASECLASS* b;
    SYMLIST* hr;
    func = makeID(BaseWithVirtualDestructor(sp) ? sc_virtual : sc_member, tp, nullptr, overloadNameTab[CI_DESTRUCTOR]);
    func->sb->xcMode = xc_none;
    func->sb->noExcept = true;
    //    func->sb->attribs.inheritable.linkage2 = sp->sb->attribs.inheritable.linkage2;
    tp->syms = CreateHashTable(1);
    sp1 = makeID(sc_parameter, tp->btp, nullptr, AnonymousName());
    insert(sp1, tp->syms);
    if (sp->sb->vbaseEntries)
    {
        sp1 = makeID(sc_parameter, &stdint, nullptr, AnonymousName());
        sp1->sb->isDestructor = true;
        insert(sp1, tp->syms);
    }
    rv = insertFunc(sp, func);
    rv->sb->isDestructor = true;
    b = sp->sb->baseClasses;
    while (b)
    {
        if (!b->cls->sb->pureDest)
            break;
        b = b->next;
    }
    if (!b)
    {
        e = sp->sb->vbaseEntries;
        while (e)
        {
            if (!e->cls->sb->pureDest)
                break;
            e = e->next;
        }
        if (!e)
        {
            hr = basetype(sp->tp)->syms->table[0];
            while (hr)
            {
                SYMBOL* cls = hr->p;
                TYPE* tp = cls->tp;
                while (isarray(tp))
                    tp = basetype(tp)->btp;
                if (isstructured(tp) && !basetype(tp)->sp->sb->pureDest)
                    break;
                hr = hr->next;
            }
            if (!hr)
                sp->sb->pureDest = true;
        }
    }

    return rv;
}
static bool hasConstFunc(SYMBOL* sp, int type, bool move)
{
    SYMBOL* ovl = search(overloadNameTab[type], basetype(sp->tp)->syms);
    if (ovl)
    {
        SYMLIST* hr = basetype(ovl->tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* func = hr->p;
            SYMLIST* hra = basetype(func->tp)->syms->table[0]->next;
            if (hra && (!hra->next || (hra->next->p)->sb->init) || (hra->next->p)->sb->deferredCompile)
            {
                SYMBOL* arg = (SYMBOL*)hra->p;
                if (isref(arg->tp))
                {
                    if (isstructured(basetype(arg->tp)->btp))
                    {
                        if (basetype(basetype(arg->tp)->btp)->sp == sp || sameTemplate(basetype(basetype(arg->tp)->btp), sp->tp))
                        {
                            if ((basetype(arg->tp)->type == bt_lref && !move) || (basetype(arg->tp)->type == bt_rref && move))
                            {
                                return isconst(basetype(arg->tp)->btp);
                            }
                        }
                    }
                }
            }
            hr = hr->next;
        }
    }
    return false;
}
static bool constCopyConstructor(SYMBOL* sp)
{
    VBASEENTRY* e;
    BASECLASS* b;
    SYMLIST* hr;
    b = sp->sb->baseClasses;
    while (b)
    {
        if (!b->isvirtual && !hasConstFunc(b->cls, CI_CONSTRUCTOR, false))
            return false;
        b = b->next;
    }
    e = sp->sb->vbaseEntries;
    while (e)
    {
        if (e->alloc && !hasConstFunc(e->cls, CI_CONSTRUCTOR, false))
            return false;
        e = e->next;
    }
    hr = basetype(sp->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* cls = hr->p;
        if (isstructured(cls->tp) && cls->sb->storage_class != sc_typedef && !cls->sb->trivialCons)
            if (!hasConstFunc(basetype(cls->tp)->sp, CI_CONSTRUCTOR, false))
                return false;
        hr = hr->next;
    }

    return true;
}
static SYMBOL* declareConstructor(SYMBOL* sp, bool deflt, bool move)
{
    SYMBOL *func, *sp1;
    TYPE* tp = MakeType(bt_func, MakeType(bt_void));
    func = makeID(sc_member, tp, nullptr, overloadNameTab[CI_CONSTRUCTOR]);
    func->sb->isConstructor = true;
    //    func->sb->attribs.inheritable.linkage2 = sp->sb->attribs.inheritable.linkage2;
    sp1 = makeID(sc_parameter, nullptr, nullptr, AnonymousName());
    tp->syms = CreateHashTable(1);
    tp->syms->table[0] = Allocate<SYMLIST>();
    tp->syms->table[0]->p = (SYMBOL*)sp1;
    if (deflt)
    {
        sp1->tp = MakeType(bt_void);
    }
    else
    {
        sp1->tp = MakeType(move ? bt_rref : bt_lref, basetype(sp->tp));
        if (!move && constCopyConstructor(sp))
        {
            sp1->tp->btp = MakeType(bt_const, sp1->tp->btp);
        }
    }
    UpdateRootTypes(sp1->tp);
    return insertFunc(sp, func);
}
static bool constAssignmentOp(SYMBOL* sp, bool move)
{
    VBASEENTRY* e;
    BASECLASS* b;
    SYMLIST* hr;
    b = sp->sb->baseClasses;
    while (b)
    {
        if (!b->isvirtual && !hasConstFunc(b->cls, assign - kw_new + CI_NEW, move))
            return false;
        b = b->next;
    }
    e = sp->sb->vbaseEntries;
    while (e)
    {
        if (e->alloc && !hasConstFunc(e->cls, assign - kw_new + CI_NEW, move))
            return false;
        e = e->next;
    }
    hr = basetype(sp->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* cls = hr->p;
        if (isstructured(cls->tp) && cls->sb->storage_class != sc_typedef && !cls->sb->trivialCons)
            if (!hasConstFunc(basetype(cls->tp)->sp, assign - kw_new + CI_NEW, move))
                return false;
        hr = hr->next;
    }

    return true;
}
static SYMBOL* declareAssignmentOp(SYMBOL* sp, bool move)
{
    SYMBOL *func, *sp1;
    TYPE* tp = MakeType(bt_func, basetype(sp->tp));
    TYPE* tpx;
    if (isstructured(sp->tp))
    {
        tp->btp = MakeType(move ? bt_rref : bt_lref, tp->btp);
    }
    UpdateRootTypes(tp);
    func = makeID(sc_member, tp, nullptr, overloadNameTab[assign - kw_new + CI_NEW]);
    sp1 = makeID(sc_parameter, nullptr, nullptr, AnonymousName());
    tp->syms = CreateHashTable(1);
    tp->syms->table[0] = Allocate<SYMLIST>();
    tp->syms->table[0]->p = (SYMBOL*)sp1;
    sp1->tp = MakeType(move ? bt_rref : bt_lref, basetype(sp->tp));
    if (constAssignmentOp(sp, move))
    {
        sp1->tp->btp = MakeType(bt_const, sp1->tp->btp);
    }
    UpdateRootTypes(sp1->tp);
    return insertFunc(sp, func);
}
bool matchesDefaultConstructor(SYMBOL* sp)
{
    SYMLIST* hr = basetype(sp->tp)->syms->table[0]->next;
    if (hr)
    {
        SYMBOL* arg1 = hr->p;
        if (arg1->tp->type == bt_void || arg1->sb->init || arg1->sb->deferredCompile)
            return true;
    }
    return false;
}
bool matchesCopy(SYMBOL* sp, bool move)
{
    SYMLIST* hr = basetype(sp->tp)->syms->table[0]->next;
    if (hr)
    {
        SYMBOL* arg1 = hr->p;
        if (!hr->next || (hr->next->p)->sb->init || (hr->next->p)->sb->deferredCompile || (hr->next->p)->sb->constop)
        {
            if (basetype(arg1->tp)->type == (move ? bt_rref : bt_lref))
            {
                TYPE* tp = basetype(arg1->tp)->btp;
                if (isstructured(tp))
                    if (basetype(tp)->sp == sp->sb->parentClass || basetype(tp)->sp == sp->sb->parentClass->sb->mainsym ||
                        basetype(tp)->sp->sb->mainsym == sp->sb->parentClass || sameTemplate(tp, sp->sb->parentClass->tp))
                        return true;
            }
        }
    }
    return false;
}
static bool hasCopy(SYMBOL* func, bool move)
{
    SYMLIST* hr = basetype(func->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sp = hr->p;
        if (!sp->sb->internallyGenned && matchesCopy(sp, move))
            return true;
        hr = hr->next;
    }
    return false;
}
static bool checkDest(SYMBOL* sp, SYMBOL* parent, HASHTABLE* syms, enum e_ac access)
{
    SYMBOL* dest = search(overloadNameTab[CI_DESTRUCTOR], syms);

    if (dest)
    {
        dest = (SYMBOL*)basetype(dest->tp)->syms->table[0]->p;
        if (dest->sb->deleted)
            return true;
        if (!isAccessible(sp, parent, dest, nullptr, access, false))
            return true;
    }
    return false;
}
static bool checkDefaultCons(SYMBOL* sp, HASHTABLE* syms, enum e_ac access)
{
    SYMBOL* cons = search(overloadNameTab[CI_CONSTRUCTOR], syms);
    if (cons)
    {
        SYMBOL* dflt = nullptr;
        SYMLIST* hr = basetype(cons->tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* cur = hr->p;
            if (matchesDefaultConstructor(cur))
            {
                if (dflt)
                    return true;  // ambiguity
                dflt = cur;
            }
            hr = hr->next;
        }
        if (dflt)
        {
            if (dflt->sb->deleted)
                return true;
            if (!isAccessible(sp, dflt->sb->parentClass, dflt, nullptr, access, false))
                return true;
        }
        else
        {
            return true;
        }
    }
    return false;
}
SYMBOL* getCopyCons(SYMBOL* base, bool move)
{
    (void)move;
    SYMBOL* ovl = search(overloadNameTab[CI_CONSTRUCTOR], basetype(base->tp)->syms);
    if (ovl)
    {
        SYMLIST* hr = basetype(ovl->tp)->syms->table[0];
        while (hr)
        {
            SYMBOL *sym = hr->p, *sym1 = nullptr;
            SYMLIST* hrArgs = basetype(sym->tp)->syms->table[0];
            sym = (SYMBOL*)hrArgs->p;
            if (sym->sb->thisPtr)
            {
                hrArgs = hrArgs->next;
                if (hrArgs)
                    sym = (SYMBOL*)hrArgs->p;
            }
            if (hrArgs && hrArgs->next)
            {
                sym1 = (SYMBOL*)hrArgs->next->p;
            }
            if (hrArgs && (!sym1 || sym1->sb->init || sym1->sb->deferredCompile))
            {
                TYPE* tp = basetype(sym->tp);
                if (tp->type == (move ? bt_rref : bt_lref))
                {
                    tp = basetype(tp->btp);
                    if (isstructured(tp))
                    {
                        if (!base->tp->sp)
                        {
                            hr = hr->next;
                            continue;
                        }
                        if (tp->sp == base->tp->sp || tp->sp == base->tp->sp->sb->mainsym || sameTemplate(tp, base->tp))
                        {
                            return hr->p;
                        }
                    }
                }
            }
            hr = hr->next;
        }
    }
    return nullptr;
}
static SYMBOL* GetCopyAssign(SYMBOL* base, bool move)
{
    (void)move;
    SYMBOL* ovl = search(overloadNameTab[assign - kw_new + CI_NEW], basetype(base->tp)->syms);
    if (ovl)
    {
        SYMLIST* hr = basetype(ovl->tp)->syms->table[0];
        while (hr)
        {
            SYMBOL *sym = hr->p, *sym1 = nullptr;
            SYMLIST* hrArgs = basetype(sym->tp)->syms->table[0];
            sym = (SYMBOL*)hrArgs->p;
            if (sym->sb->thisPtr)
            {
                hrArgs = hrArgs->next;
                if (hrArgs)
                    sym = (SYMBOL*)hrArgs->p;
            }
            if (hrArgs && hrArgs->next)
            {
                sym1 = (SYMBOL*)hrArgs->next->p;
            }
            if (hrArgs && (!sym1 || sym1->sb->init || sym1->sb->deferredCompile))
            {
                TYPE* tp = basetype(sym->tp);
                if (tp->type == (move ? bt_rref : bt_lref))
                {
                    tp = basetype(tp->btp);
                    if (isstructured(tp))
                    {
                        if (comparetypes(tp, base->tp, true) || sameTemplate(tp, base->tp))
                        {
                            return hr->p;
                        }
                    }
                }
            }
            hr = hr->next;
        }
    }
    return nullptr;
}
bool hasVTab(SYMBOL* sp)
{
    VTABENTRY* vt = sp->sb->vtabEntries;
    while (vt)
    {
        if (vt->virtuals)
            return true;
        vt = vt->next;
    }
    return false;
}
static bool hasTrivialCopy(SYMBOL* sp, bool move)
{
    SYMLIST* hr;
    SYMBOL* dflt;
    BASECLASS* base;
    if (sp->sb->vbaseEntries || hasVTab(sp))
        return false;
    base = sp->sb->baseClasses;
    while (base)
    {
        dflt = getCopyCons(base->cls, move);
        if (!dflt)
            return false;
        if (!dflt->sb->trivialCons)
            return false;
        base = base->next;
    }
    hr = basetype(sp->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* cls = hr->p;
        if (isstructured(cls->tp))
        {
            dflt = getCopyCons(basetype(cls->tp)->sp, move);
            if (!dflt)
                return false;
            if (!dflt->sb->trivialCons)
                return false;
        }
        hr = hr->next;
    }
    return true;
}
static bool hasTrivialAssign(SYMBOL* sp, bool move)
{
    SYMLIST* hr;
    SYMBOL* dflt;
    BASECLASS* base;
    if (sp->sb->vbaseEntries || hasVTab(sp))
        return false;
    base = sp->sb->baseClasses;
    while (base)
    {
        dflt = GetCopyAssign(base->cls, move);
        if (!dflt)
            return false;
        if (!dflt->sb->trivialCons)
            return false;
        base = base->next;
    }
    hr = basetype(sp->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* cls = hr->p;
        if (isstructured(cls->tp))
        {
            dflt = getCopyCons(basetype(cls->tp)->sp, move);
            if (!dflt)
                return false;
            if (!dflt->sb->trivialCons)
                return false;
        }
        hr = hr->next;
    }
    return true;
}
static bool checkCopyCons(SYMBOL* sp, SYMBOL* base, enum e_ac access)
{
    SYMBOL* dflt = getCopyCons(base, false);
    if (dflt)
    {
        if (dflt->sb->deleted)
            return true;
        if (!isAccessible(sp, dflt->sb->parentClass, dflt, nullptr, access, false))
            return true;
    }
    else
    {
        return true;
    }
    return false;
}
static bool checkCopyAssign(SYMBOL* sp, SYMBOL* base, enum e_ac access)
{
    SYMBOL* dflt = GetCopyAssign(base, false);
    if (dflt)
    {
        if (dflt->sb->deleted)
            return true;
        if (!isAccessible(sp, dflt->sb->parentClass, dflt, nullptr, access, false))
            return true;
    }
    else
    {
        return true;
    }
    return false;
}
static bool checkMoveCons(SYMBOL* sp, SYMBOL* base, enum e_ac access)
{
    SYMBOL* dflt = getCopyCons(base, true);
    if (dflt)
    {
        if (dflt->sb->deleted)
            return true;
        if (!isAccessible(sp, dflt->sb->parentClass, dflt, nullptr, access, false))
            return true;
    }
    return false;
}
static bool checkMoveAssign(SYMBOL* sp, SYMBOL* base, enum e_ac access)
{
    SYMBOL* dflt = GetCopyAssign(base, true);
    if (dflt)
    {
        if (dflt->sb->deleted)
            return true;
        if (!isAccessible(sp, dflt->sb->parentClass, dflt, nullptr, access, false))
            return true;
    }
    else
    {
        if (!hasTrivialAssign(sp, true))
            return true;
    }
    return false;
}
static bool isDefaultDeleted(SYMBOL* sp)
{
    SYMLIST* hr;
    BASECLASS* base;
    VBASEENTRY* vbase;
    if (basetype(sp->tp)->type == bt_union)
    {
        bool allconst = true;
        hr = basetype(sp->tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* sp = hr->p;
            if (!isconst(sp->tp) && sp->tp->type != bt_aggregate)
                allconst = false;
            if (isstructured(sp->tp))
            {
                SYMBOL* cons = search(overloadNameTab[CI_CONSTRUCTOR], basetype(sp->tp)->syms);
                SYMLIST* hr1 = basetype(cons->tp)->syms->table[0];
                while (hr1)
                {
                    cons = (SYMBOL*)hr1->p;
                    if (matchesDefaultConstructor(cons))
                        if (!cons->sb->trivialCons)
                            return true;
                    hr1 = hr1->next;
                }
            }
            hr = hr->next;
        }
        if (allconst)
            return true;
    }
    hr = basetype(sp->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sp1 = hr->p;
        TYPE* m;
        if (sp1->sb->storage_class == sc_member || sp1->sb->storage_class == sc_mutable)
        {
            if (isref(sp1->tp))
                if (!sp1->sb->init)
                    return true;
            if (basetype(sp1->tp)->type == bt_union)
            {
                SYMLIST* hr1 = basetype(sp1->tp)->syms->table[0];
                while (hr1)
                {
                    SYMBOL* member = (SYMBOL*)hr1->p;
                    if (!isconst(member->tp) && basetype(member->tp)->type != bt_aggregate)
                    {
                        break;
                    }
                    hr1 = hr1->next;
                }
                if (!hr1)
                    return true;
            }
            if (isstructured(sp1->tp))
            {
                TYPE* tp = basetype(sp1->tp);
                if (checkDest(sp, tp->sp, basetype(tp->sp->tp)->syms, ac_public))
                    return true;
            }
            m = sp1->tp;
            if (isarray(m))
                m = basetype(sp1->tp)->btp;
            if (isstructured(m))
            {
                TYPE* tp = basetype(m);
                if (checkDefaultCons(sp, basetype(tp->sp->tp)->syms, ac_public))
                    return true;
            }
        }
        hr = hr->next;
    }

    base = sp->sb->baseClasses;
    while (base)
    {
        if (checkDest(sp, base->cls, basetype(base->cls->tp)->syms, ac_protected))
            return true;
        if (checkDefaultCons(sp, basetype(base->cls->tp)->syms, ac_protected))
            return true;
        base = base->next;
    }
    vbase = sp->sb->vbaseEntries;
    while (vbase)
    {
        if (vbase->alloc)
        {
            if (checkDest(sp, vbase->cls, basetype(vbase->cls->tp)->syms, ac_protected))
                return true;
            if (checkDefaultCons(sp, basetype(vbase->cls->tp)->syms, ac_protected))
                return true;
        }
        vbase = vbase->next;
    }
    return false;
}
static bool isCopyConstructorDeleted(SYMBOL* sp)
{
    SYMLIST* hr;
    BASECLASS* base;
    VBASEENTRY* vbase;
    if (basetype(sp->tp)->type == bt_union)
    {
        hr = basetype(sp->tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* sp = hr->p;
            if (isstructured(sp->tp))
            {
                SYMBOL* cons = search(overloadNameTab[CI_CONSTRUCTOR], basetype(sp->tp)->syms);
                SYMLIST* hr1 = basetype(cons->tp)->syms->table[0];
                while (hr1)
                {
                    cons = (SYMBOL*)hr1->p;
                    if (matchesCopy(cons, false))
                        if (!cons->sb->trivialCons)
                            return true;
                    hr1 = hr1->next;
                }
            }
            hr = hr->next;
        }
    }
    hr = basetype(sp->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sp1 = hr->p;
        TYPE* m;
        if (sp1->sb->storage_class == sc_member || sp1->sb->storage_class == sc_mutable)
        {
            if (basetype(sp1->tp)->type == bt_rref)
                return true;
            if (isstructured(sp1->tp))
            {
                TYPE* tp = basetype(sp1->tp);
                if (checkDest(sp, tp->sp, basetype(tp->sp->tp)->syms, ac_public))
                    return true;
            }
            m = sp1->tp;
            if (isarray(m))
                m = basetype(sp1->tp)->btp;
            if (isstructured(m))
            {
                if (checkCopyCons(sp, basetype(m)->sp, ac_public))
                    return true;
            }
        }
        hr = hr->next;
    }

    base = sp->sb->baseClasses;
    while (base)
    {
        if (checkDest(sp, base->cls, basetype(base->cls->tp)->syms, ac_protected))
            return true;
        if (checkCopyCons(sp, base->cls, ac_protected))
            return true;
        base = base->next;
    }
    vbase = sp->sb->vbaseEntries;
    while (vbase)
    {
        if (vbase->alloc)
        {
            if (checkDest(sp, vbase->cls, basetype(vbase->cls->tp)->syms, ac_protected))
                return true;
            if (checkCopyCons(sp, vbase->cls, ac_protected))
                return true;
        }
        vbase = vbase->next;
    }
    return false;
}
static bool isCopyAssignmentDeleted(SYMBOL* sp)
{
    SYMLIST* hr;
    BASECLASS* base;
    VBASEENTRY* vbase;
    if (basetype(sp->tp)->type == bt_union)
    {
        hr = basetype(sp->tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* sp = hr->p;
            if (isstructured(sp->tp))
            {
                SYMBOL* cons = search(overloadNameTab[assign - kw_new + CI_NEW], basetype(sp->tp)->syms);
                SYMLIST* hr1 = basetype(cons->tp)->syms->table[0];
                while (hr1)
                {
                    cons = (SYMBOL*)hr1->p;
                    if (matchesCopy(cons, false))
                        if (!cons->sb->trivialCons)
                            return true;
                    hr1 = hr1->next;
                }
            }
            hr = hr->next;
        }
    }
    hr = basetype(sp->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sp1 = hr->p;
        TYPE* m;
        if (sp1->sb->storage_class == sc_member || sp1->sb->storage_class == sc_mutable)
        {
            if (isref(sp1->tp))
                return true;
            m = sp1->tp;
            if (isarray(m))
                m = basetype(sp1->tp)->btp;
            if (!isstructured(m) && isconst(m) && m->type != bt_aggregate)
                return true;
            if (isstructured(m))
            {
                if (checkCopyAssign(sp, basetype(m)->sp, ac_public))
                    return true;
            }
        }
        hr = hr->next;
    }

    base = sp->sb->baseClasses;
    while (base)
    {
        if (checkCopyAssign(sp, base->cls, ac_protected))
            return true;
        base = base->next;
    }
    vbase = sp->sb->vbaseEntries;
    while (vbase)
    {
        if (vbase->alloc && checkCopyAssign(sp, vbase->cls, ac_protected))
            return true;
        vbase = vbase->next;
    }
    return false;
}
static bool isMoveConstructorDeleted(SYMBOL* sp)
{
    SYMLIST* hr;
    BASECLASS* base;
    VBASEENTRY* vbase;
    if (basetype(sp->tp)->type == bt_union)
    {
        hr = basetype(sp->tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* sp = hr->p;
            if (isstructured(sp->tp))
            {
                SYMBOL* cons = search(overloadNameTab[CI_CONSTRUCTOR], basetype(sp->tp)->syms);
                SYMLIST* hr1 = basetype(cons->tp)->syms->table[0];
                while (hr1)
                {
                    cons = (SYMBOL*)hr1->p;
                    if (matchesCopy(cons, true))
                        if (!cons->sb->trivialCons)
                            return true;
                    hr1 = hr1->next;
                }
            }
            hr = hr->next;
        }
    }
    hr = basetype(sp->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sp1 = hr->p;
        TYPE* m;
        if (sp1->sb->storage_class == sc_member || sp1->sb->storage_class == sc_mutable)
        {
            if (isstructured(sp1->tp))
            {
                TYPE* tp = basetype(sp1->tp);
                if (checkDest(sp, tp->sp, basetype(tp->sp->tp)->syms, ac_public))
                    return true;
            }
            m = sp1->tp;
            if (isarray(m))
                m = basetype(sp1->tp)->btp;
            if (isstructured(m))
            {
                if (checkMoveCons(sp, basetype(m)->sp, ac_public))
                    return true;
            }
        }
        hr = hr->next;
    }

    base = sp->sb->baseClasses;
    while (base)
    {
        if (checkDest(sp, base->cls, basetype(base->cls->tp)->syms, ac_protected))
            return true;
        if (checkMoveCons(sp, base->cls, ac_protected))
            return true;
        base = base->next;
    }
    vbase = sp->sb->vbaseEntries;
    while (vbase)
    {
        if (vbase->alloc)
        {
            if (checkDest(sp, vbase->cls, basetype(vbase->cls->tp)->syms, ac_protected))
                return true;
            if (checkMoveCons(sp, vbase->cls, ac_protected))
                return true;
        }
        vbase = vbase->next;
    }
    return false;
}
static bool isMoveAssignmentDeleted(SYMBOL* sp)
{
    SYMLIST* hr;
    BASECLASS* base;
    VBASEENTRY* vbase;
    if (basetype(sp->tp)->type == bt_union)
    {
        hr = basetype(sp->tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* sp = hr->p;
            if (isstructured(sp->tp))
            {
                SYMBOL* cons = search(overloadNameTab[assign - kw_new + CI_NEW], basetype(sp->tp)->syms);
                SYMLIST* hr1 = basetype(cons->tp)->syms->table[0];
                while (hr1)
                {
                    cons = (SYMBOL*)hr1->p;
                    if (matchesCopy(cons, true))
                        if (!cons->sb->trivialCons)
                            return true;
                    hr1 = hr1->next;
                }
            }
            hr = hr->next;
        }
    }
    hr = basetype(sp->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sp1 = hr->p;
        TYPE* m;
        if (sp1->sb->storage_class == sc_member || sp1->sb->storage_class == sc_mutable)
        {
            if (isref(sp1->tp))
                return true;
            if (!isstructured(sp1->tp) && isconst(sp1->tp) && sp1->tp->type != bt_aggregate)
                return true;
            m = sp1->tp;
            if (isarray(m))
                m = basetype(sp1->tp)->btp;
            if (isstructured(m))
            {
                if (checkMoveAssign(sp, basetype(m)->sp, ac_public))
                    return true;
            }
        }
        hr = hr->next;
    }

    base = sp->sb->baseClasses;
    while (base)
    {
        if (checkMoveAssign(sp, base->cls, ac_protected))
            return true;
        base = base->next;
    }
    vbase = sp->sb->vbaseEntries;
    while (vbase)
    {
        if (vbase->alloc && checkMoveAssign(sp, vbase->cls, ac_protected))
            return true;
        vbase = vbase->next;
    }
    return false;
}
static bool isDestructorDeleted(SYMBOL* sp)
{
    SYMLIST* hr;
    BASECLASS* base;
    VBASEENTRY* vbase;
    if (basetype(sp->tp)->type == bt_union)
    {
        return false;
    }
    hr = basetype(sp->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sp1 = hr->p;
        TYPE* m;
        if (sp1->sb->storage_class == sc_member || sp1->sb->storage_class == sc_mutable)
        {
            if (isstructured(sp1->tp))
            {
                TYPE* tp = basetype(sp1->tp);
                if (checkDest(sp, tp->sp, basetype(tp->sp->tp)->syms, ac_public))
                    return true;
            }
        }
        hr = hr->next;
    }

    base = sp->sb->baseClasses;
    while (base)
    {
        if (checkDest(sp, base->cls, basetype(base->cls->tp)->syms, ac_protected))
            return true;
        base = base->next;
    }
    vbase = sp->sb->vbaseEntries;
    while (vbase)
    {
        if (vbase->alloc)
        {
            if (checkDest(sp, vbase->cls, basetype(vbase->cls->tp)->syms, ac_protected))
                return true;
        }
        vbase = vbase->next;
    }
    return false;
}
static void conditionallyDeleteDefaultConstructor(SYMBOL* func)
{
    SYMLIST* hr = basetype(func->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sp = hr->p;
        if (sp->sb->defaulted && matchesDefaultConstructor(sp))
        {
            if (isDefaultDeleted(sp->sb->parentClass))
            {
                sp->sb->deleted = true;
            }
        }
        hr = hr->next;
    }
}
static bool conditionallyDeleteCopyConstructor(SYMBOL* func, bool move)
{
    SYMLIST* hr = basetype(func->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sp = hr->p;
        if (sp->sb->defaulted && matchesCopy(sp, move))
        {
            if (move && isMoveConstructorDeleted(sp->sb->parentClass))
                sp->sb->deleted = true;
            else if (!move && isCopyConstructorDeleted(sp->sb->parentClass))
                sp->sb->deleted = true;
        }
        hr = hr->next;
    }
    return false;
}
static bool conditionallyDeleteCopyAssignment(SYMBOL* func, bool move)
{
    SYMLIST* hr = basetype(func->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sp = hr->p;
        if (sp->sb->defaulted && matchesCopy(sp, move))
        {
            if (move && isMoveAssignmentDeleted(sp->sb->parentClass))
                sp->sb->deleted = true;
            else if (!move && isCopyAssignmentDeleted(sp->sb->parentClass))
                sp->sb->deleted = true;
        }
        hr = hr->next;
    }
    return false;
}
static bool conditionallyDeleteDestructor(SYMBOL* sp)
{
    if (isDestructorDeleted(sp->sb->parentClass))
    {
        sp->sb->deleted = true;
    }
    return false;
}
void createConstructorsForLambda(SYMBOL* sp)
{
    SYMBOL* newcons;
    declareDestructor(sp);
    newcons = declareConstructor(sp, true, false);  // default
    newcons->sb->deleted = true;
    newcons = declareConstructor(sp, false, false);  // copy
    conditionallyDeleteCopyConstructor(newcons, false);
    newcons = declareAssignmentOp(sp, false);
    newcons->sb->deleted = true;
    if (!isMoveConstructorDeleted(sp))
    {
        newcons = declareConstructor(sp, false, true);
        //        newcons->sb->deleted = isMoveAssignmentDeleted(sp);
    }
}
// if there is a constructor with defaulted args that would match the
// default constructor,   make a shim default constructor that wraps it...
//
static void shimDefaultConstructor(SYMBOL* sp, SYMBOL* cons)
{
    SYMBOL* match = nullptr;
    SYMLIST* hr = basetype(cons->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sym = hr->p;
        if (matchesDefaultConstructor(sym))
        {
            if (match)
                return;  // will get an error elsewhere because of the duplicate
            match = sym;
        }
        hr = hr->next;
    }
    if (match)
    {
        hr = basetype(match->tp)->syms->table[0];
        if (hr->next && ((hr->next->p)->sb->init || (hr->next->p)->sb->deferredCompile))
        {
            if (allTemplateArgsSpecified(sp, sp->templateParams))
            {
                // will match a default constructor but has defaulted args
                SYMBOL* consfunc = declareConstructor(sp, true, false);  // default
                HASHTABLE* syms;
                BLOCKDATA b = {};
                STATEMENT* st;
                EXPRESSION* thisptr = varNode(en_auto, hr->p);
                EXPRESSION* e1;
                FUNCTIONCALL* params = Allocate<FUNCTIONCALL>();
                hr->p->sb->offset = Optimizer::chosenAssembler->arch->retblocksize;
                deref(&stdpointer, &thisptr);
                b.type = begin;
                syms = localNameSpace->valueData->syms;
                localNameSpace->valueData->syms = basetype(consfunc->tp)->syms;
                params->thisptr = thisptr;
                params->thistp = MakeType(bt_pointer, sp->tp);
                params->fcall = varNode(en_pc, match);
                params->functp = match->tp;
                params->sp = match;
                params->ascall = true;
                AdjustParams(match, basetype(match->tp)->syms->table[0], &params->arguments, false, true);
                if (sp->sb->vbaseEntries)
                {
                    INITLIST *x = Allocate<INITLIST>(), **p;
                    x->tp = MakeType(bt_int);
                    x->exp = intNode(en_c_i, 1);
                    p = &params->arguments;
                    while (*p)
                        p = &(*p)->next;
                    *p = x;
                }
                e1 = varNode(en_func, nullptr);
                e1->v.func = params;
                if (e1)  // could probably remove this, only null if ran out of memory.
                {
                    e1 = exprNode(en_thisref, e1, nullptr);
                    e1->v.t.thisptr = params->thisptr;
                    e1->v.t.tp = sp->tp;
                    // hasXCInfo = true;
                }
                st = stmtNode(nullptr, &b, st_return);
                st->select = e1;
                consfunc->sb->xcMode = cons->sb->xcMode;
                if (consfunc->sb->xc)
                    consfunc->sb->xc->xcDynamic = cons->sb->xc->xcDynamic;
                consfunc->sb->inlineFunc.stmt = stmtNode(nullptr, nullptr, st_block);
                consfunc->sb->inlineFunc.stmt->lower = b.head;
                consfunc->sb->inlineFunc.syms = basetype(consfunc->tp)->syms;
                consfunc->sb->retcount = 1;
                consfunc->sb->attribs.inheritable.isInline = true;
                InsertInline(consfunc);
                // now get rid of the first default arg
                // leave others so the old constructor can be considered
                // under other circumstances
                hr = hr->next;
                hr->p->sb->init = nullptr;
                if (match->sb->deferredCompile && !match->sb->inlineFunc.stmt)
                    deferredCompileOne(match);
                localNameSpace->valueData->syms = syms;
            }
        }
    }
}
void createDefaultConstructors(SYMBOL* sp)
{
    SYMBOL* cons = search(overloadNameTab[CI_CONSTRUCTOR], basetype(sp->tp)->syms);
    SYMBOL* dest = search(overloadNameTab[CI_DESTRUCTOR], basetype(sp->tp)->syms);
    SYMBOL* asgn = search(overloadNameTab[assign - kw_new + CI_NEW], basetype(sp->tp)->syms);
    SYMBOL* newcons = nullptr;
    if (!dest)
    {
        declareDestructor(sp);
    }
    else
    {
        sp->sb->hasDest = true;
    }
    if (cons)
    {
        bool defaulted = true;
        for (auto hr = cons->tp->syms->table[0]; hr; hr = hr->next)
            if (!hr->p->sb->defaulted || hr->p->sb->isExplicit)
            {
                defaulted = false;
                break;
            }
        sp->sb->hasUserCons = !defaulted;
        shimDefaultConstructor(sp, cons);
    }
    else
    {
        // create the default constructor
        newcons = declareConstructor(sp, true, false);
        cons = search(overloadNameTab[CI_CONSTRUCTOR], basetype(sp->tp)->syms);
    }
    conditionallyDeleteDefaultConstructor(cons);
    // see if the default constructor could be trivial
    if (!hasVTab(sp) && sp->sb->vbaseEntries == nullptr && !dest)
    {
        BASECLASS* base = sp->sb->baseClasses;
        while (base)
        {
            if (!base->cls->sb->trivialCons || base->accessLevel != ac_public)
                break;
            base = base->next;
        }
        if (!base)
        {
            bool trivialCons = true;
            bool trivialDest = true;
            SYMLIST* p = basetype(sp->tp)->syms->table[0];
            while (p)
            {
                SYMBOL* pcls = (SYMBOL*)p->p;
                TYPE* tp = pcls->tp;
                while (isarray(tp))
                    tp = basetype(tp)->btp;
                if (pcls->sb->storage_class == sc_member || pcls->sb->storage_class == sc_mutable ||
                    pcls->sb->storage_class == sc_overloads)
                {
                    if (pcls->sb->memberInitializers)
                        trivialCons = false;
                    if (isstructured(tp))
                    {
                        if (!basetype(tp)->sp->sb->trivialCons)
                            trivialCons = false;
                    }
                    else if (pcls->sb->storage_class == sc_overloads)
                    {
                        bool err = false;
                        SYMLIST* p = basetype(tp)->syms->table[0];
                        while (p && !err)
                        {
                            SYMBOL* s = (SYMBOL*)p->p;
                            if (s->sb->storage_class != sc_static)
                            {
                                err |= s->sb->isConstructor && !s->sb->defaulted;
                                err |= s->sb->deleted;
                                err |= s->sb->access != ac_public;
                                err |= s->sb->isConstructor && s->sb->isExplicit;
                                if (s->sb->isDestructor && !s->sb->defaulted)
                                    trivialDest = false;
                            }
                            p = p->next;
                        }
                        if (err)
                            trivialCons = false;
                    }
                    else if (pcls->sb->access != ac_public)
                        trivialCons = false;
                }
                p = p->next;
            }
            sp->sb->trivialCons = trivialCons;
            sp->sb->trivialDest = trivialDest;
        }
        auto p = cons->tp->syms->table[0];
        while (p)
        {
            auto s = (SYMBOL*)p->p;
            if (s->sb->constexpression | s->sb->defaulted)
                sp->sb->literalClass = true;
            p = p->next;
        }
    }
    if (newcons)
        newcons->sb->trivialCons = true;
    // now if there is no copy constructor or assignment operator declare them
    if (!hasCopy(cons, false))
    {
        SYMBOL* newcons = declareConstructor(sp, false, false);
        newcons->sb->trivialCons = hasTrivialCopy(sp, false);
        if (hasCopy(cons, true) || (asgn && hasCopy(asgn, true)))
            newcons->sb->deleted = true;
        if (!asgn)
            asgn = search(overloadNameTab[assign - kw_new + CI_NEW], basetype(sp->tp)->syms);
        conditionallyDeleteCopyConstructor(cons, false);
    }
    if (!asgn || !hasCopy(asgn, false))
    {
        SYMBOL* newsp = declareAssignmentOp(sp, false);
        newsp->sb->trivialCons = hasTrivialAssign(sp, false);
        if (hasCopy(cons, true) || (asgn && hasCopy(asgn, true)))
            newsp->sb->deleted = true;
        if (!asgn)
            asgn = search(overloadNameTab[assign - kw_new + CI_NEW], basetype(sp->tp)->syms);
        conditionallyDeleteCopyAssignment(asgn, false);
    }
    // now if there is no move constructor, no copy constructor,
    // no copy assignment, no move assignment, no destructor
    // and wouldn't be defined as deleted
    // declare a move constructor and assignment operator
    if (!dest && !hasCopy(cons, false) && !hasCopy(cons, true) && !hasCopy(asgn, false) && (!asgn || !hasCopy(asgn, true)))
    {
        bool b = isMoveConstructorDeleted(sp);
        SYMBOL* newcons;
        if (!b)
        {
            newcons = declareConstructor(sp, false, true);
            newcons->sb->trivialCons = hasTrivialCopy(sp, true);
        }
        newcons = declareAssignmentOp(sp, true);
        newcons->sb->trivialCons = hasTrivialAssign(sp, true);
        newcons->sb->deleted |= isMoveAssignmentDeleted(sp);
    }
    else
    {
        conditionallyDeleteCopyConstructor(cons, true);
        conditionallyDeleteCopyAssignment(asgn, true);
    }
    dest = search(overloadNameTab[CI_DESTRUCTOR], basetype(sp->tp)->syms);
    conditionallyDeleteDestructor(dest->tp->syms->table[0]->p);
}
EXPRESSION* destructLocal(EXPRESSION* exp)
{
    std::stack<SYMBOL*> destructList;
    std::stack<EXPRESSION*> stk;
    stk.push(exp);
    while (stk.size())
    {
        EXPRESSION* e = stk.top();
        stk.pop();
        if (!isintconst(e) && !isfloatconst(e))
        {
            if (e->left)
                stk.push(e->left);
            if (e->right)
                stk.push(e->right);
        }
        if (e->type == en_thisref)
            e = e->left;
        if (e->type == en_func)
        {
            INITLIST* il = e->v.func->arguments;
            while (il)
            {
                stk.push(il->exp);
                il = il->next;
            }
        }
        if (e->type == en_auto && e->v.sp->sb->allocate && !e->v.sp->sb->destructed)
        {
            TYPE* tp = e->v.sp->tp;
            while (isarray(tp))
                tp = basetype(tp)->btp;
            if (isstructured(tp) && !isref(tp))
            {
                e->v.sp->sb->destructed = true;
                destructList.push(e->v.sp);
            }
        }
    }

    EXPRESSION* rv = exp;
    while (destructList.size())
    {
        SYMBOL* sp = destructList.top();
        destructList.pop();
        if (sp->sb->dest && sp->sb->dest->exp)
            rv = exprNode(en_void, rv, sp->sb->dest->exp);
    }
    return rv;
}
void DestructParams(INITLIST* first)
{
    if (Optimizer::cparams.prm_cplusplus)
        while (first)
        {
            TYPE* tp = first->tp;
            if (tp)
            {
                bool ref = false;
                if (isref(tp))
                {
                    ref = true;
                    tp = basetype(tp)->btp;
                }
                else if (tp->lref || tp->rref)
                {
                    ref = true;
                }
                if (ref || !isstructured(tp))
                {
                    static std::stack<EXPRESSION*> stk;
                    
                    stk.push(first->exp);
                    while (!stk.empty())
                    {
                        auto tst = stk.top();
                        stk.pop();
                        if (tst->type == en_thisref)
                            tst = tst->left;
                        if (tst->type == en_func)
                        {
                            if (tst->v.func->sp->sb->isConstructor)
                            {
                                EXPRESSION* iexp = tst->v.func->thisptr;
                                auto sp = basetype(basetype(tst->v.func->thistp)->btp)->sp;
                                int offs;
                                auto xexp = relptr(iexp, offs);
                                if (xexp)
                                    xexp->v.sp->sb->destructed = true;
                                if (callDestructor(sp, nullptr, &iexp, nullptr, true, false, false, true))
                                {
                                    Optimizer::LIST* entry = Allocate<Optimizer::LIST>();
                                    entry->data = iexp;
                                    entry->next = first->destructors;
                                    first->destructors = entry;
                                }
                            }
                        }
                        else if (tst->type == en_void)
                        {
                            if (tst->right)
                                stk.push(tst->right);
                            if (tst->left)
                                stk.push(tst->left);
                        }
                    }
                }
            }
            first = first->next;
        }
}
void destructBlock(EXPRESSION** exp, SYMLIST* hr, bool mainDestruct)
{
    //    if (!cparams.prm_cplusplus)
    //        return;
    while (hr)
    {
        SYMBOL* sp = hr->p;
        if ((sp->sb->allocate || sp->sb->storage_class == sc_parameter) && !sp->sb->destructed && !isref(sp->tp))
        {
            sp->sb->destructed = mainDestruct;
            if (sp->sb->storage_class == sc_parameter)
            {
                if (isstructured(sp->tp))
                {
                    EXPRESSION* iexp = getThisNode(sp);
                    if (callDestructor(basetype(sp->tp)->sp, nullptr, &iexp, nullptr, true, false, false, true))
                    {
                        optimize_for_constants(&iexp);
                        if (*exp)
                        {
                            *exp = exprNode(en_void, iexp, *exp);
                        }
                        else
                        {
                            *exp = iexp;
                        }
                    }
                }
            }
            else if (sp->sb->storage_class != sc_localstatic && sp->sb->dest)
            {

                EXPRESSION* iexp = sp->sb->dest->exp;
                if (iexp)
                {
                    optimize_for_constants(&iexp);
                    if (*exp)
                    {
                        *exp = exprNode(en_void, iexp, *exp);
                    }
                    else
                    {
                        *exp = iexp;
                    }
                }
            }
        }
        hr = hr->next;
    }
}
static void genConsData(BLOCKDATA* b, SYMBOL* cls, MEMBERINITIALIZERS* mi, SYMBOL* member, int offset, EXPRESSION* thisptr,
                        EXPRESSION* otherptr, SYMBOL* parentCons, bool doCopy)
{
    (void)cls;
    (void)mi;
    if (doCopy && (matchesCopy(parentCons, false) || matchesCopy(parentCons, true)))
    {
        thisptr = exprNode(en_structadd, thisptr, intNode(en_c_i, offset));
        otherptr = exprNode(en_structadd, otherptr, intNode(en_c_i, offset));
        thisptr->right->keepZero = true;
        otherptr->right->keepZero = true;
        if (isstructured(member->tp) || isarray(member->tp) || basetype(member->tp)->type == bt_memberptr)
        {
            EXPRESSION* exp = exprNode(en_blockassign, thisptr, otherptr);
            STATEMENT* st = stmtNode(nullptr, b, st_expr);
            exp->size = member->tp->size;
            exp->altdata = (void*)member->tp;
            optimize_for_constants(&exp);
            st->select = exp;
        }
        else
        {
            STATEMENT* st = stmtNode(nullptr, b, st_expr);
            EXPRESSION* exp;
            deref(member->tp, &thisptr);
            deref(member->tp, &otherptr);
            exp = exprNode(en_assign, thisptr, otherptr);
            optimize_for_constants(&exp);
            st->select = exp;
        }
    }
    else if (member->sb->init)
    {
        EXPRESSION* exp;
        STATEMENT* st = stmtNode(nullptr, b, st_expr);
        exp = convertInitToExpression(member->tp, member, nullptr, nullptr, member->sb->init, thisptr, false);
        optimize_for_constants(&exp);
        st->select = exp;
    }
}
static void genConstructorCall(BLOCKDATA* b, SYMBOL* cls, MEMBERINITIALIZERS* mi, SYMBOL* member, int memberOffs, bool top,
                               EXPRESSION* thisptr, EXPRESSION* otherptr, SYMBOL* parentCons, bool baseClass, bool doCopy,
                               bool useDefault)
{
    STATEMENT* st = nullptr;
    if (cls != member && member->sb->init)
    {
        EXPRESSION* exp;
        if (member->sb->init->exp)
        {
            exp = convertInitToExpression(member->tp, member, nullptr, nullptr, member->sb->init, thisptr, false);
            if (mi && mi->valueInit)
            {
                auto ths = exprNode(en_add, thisptr, intNode(en_c_i, member->sb->offset));
                auto clr = exprNode(en_blockclear, ths, 0);
                clr->size = member->tp->size;
                exp = exprNode(en_void, clr, exp);
            }
        }
        else
        {
            exp = exprNode(en_add, thisptr, intNode(en_c_i, member->sb->offset));
            exp = exprNode(en_blockclear, exp, 0);
            exp->size = member->tp->size;
        }
        st = stmtNode(nullptr, b, st_expr);
        optimize_for_constants(&exp);
        st->select = exp;
    }
    else
    {
        TYPE* ctype = member->tp;
        EXPRESSION* exp = exprNode(en_add, thisptr, intNode(en_c_i, memberOffs));
        if (doCopy && matchesCopy(parentCons, false))
        {
            if (useDefault)
            {
                if (!callConstructor(&ctype, &exp, nullptr, false, nullptr, top, false, false, false, false, false, true))
                    errorsym(ERR_NO_DEFAULT_CONSTRUCTOR, member);
            }
            else
            {
                EXPRESSION* other = exprNode(en_add, otherptr, intNode(en_c_i, memberOffs));

                TYPE* tp = MakeType(bt_lref, member->tp);
                tp->size = getSize(bt_pointer);

                if (isconst(((SYMBOL*)basetype(parentCons->tp)->syms->table[0]->next->p)->tp->btp))
                {
                    tp->btp = MakeType(bt_const, tp->btp);
                }
                UpdateRootTypes(tp);
                if (!callConstructorParam(&ctype, &exp, tp, other, top, false, false, false, true))
                    errorsym(ERR_NO_APPROPRIATE_CONSTRUCTOR, member);
            }
        }
        else if (doCopy && matchesCopy(parentCons, true))
        {
            if (useDefault)
            {
                if (!callConstructor(&ctype, &exp, nullptr, false, nullptr, top, false, false, false, false, false, true))
                    errorsym(ERR_NO_DEFAULT_CONSTRUCTOR, member);
            }
            else
            {
                EXPRESSION* other = exprNode(en_add, otherptr, intNode(en_c_i, memberOffs));
                other = exprNode(en_not_lvalue, other, nullptr);
                TYPE* tp = MakeType(bt_rref, member->tp);

                if (isconst(((SYMBOL*)basetype(parentCons->tp)->syms->table[0]->next->p)->tp->btp))
                {
                    tp->btp = MakeType(bt_const, tp->btp);
                }
                UpdateRootTypes(tp);
                if (!callConstructorParam(&ctype, &exp, tp, other, top, false, false, false, true))
                    errorsym(ERR_NO_APPROPRIATE_CONSTRUCTOR, member);
            }
        }
        else
        {
            if (mi && mi->sp && baseClass)
            {
                while (mi)
                {
                    if (mi->sp && isstructured(mi->sp->tp) &&
                        (basetype(mi->sp->tp)->sp == member || basetype(mi->sp->tp)->sp == member->sb->maintemplate || sameTemplate(mi->sp->tp, member->tp)))
                    {
                        break;
                    }
                    mi = mi->next;
                }
            }
            else
            {
                mi = nullptr;
            }
            if (mi)
            {
                INITIALIZER* init = mi->init;
                FUNCTIONCALL* funcparams = Allocate<FUNCTIONCALL>();
                INITLIST** args = &funcparams->arguments;
                while (init && init->exp)
                {
                    *args = Allocate<INITLIST>();
                    (*args)->tp = init->basetp;
                    (*args)->exp = init->exp;
                    args = &(*args)->next;
                    init = init->next;
                }
                if (!callConstructor(&ctype, &exp, funcparams, false, nullptr, top, false, false, false, false, false, true))
                    errorsym(ERR_NO_DEFAULT_CONSTRUCTOR, member);
                if (mi->sp && !mi->init)
                {
                    EXPRESSION* clr = exprNode(en_blockclear, exp, nullptr);
                    clr->size = mi->sp->tp->size;
                    exp = exprNode(en_void, clr, exp);
                }
                // previously, callConstructor can return false here, meaning that funcparams->sp is null
                // This used to create a nullptr dereference in PromoteConstructorArgs
                // Why this is only being found NOW is somewhat dumb, but it is.
                PromoteConstructorArgs(funcparams->sp, funcparams);
            }
            else
            {
                if (!callConstructor(&ctype, &exp, nullptr, false, nullptr, top, false, false, false, false, false, true))
                    errorsym(ERR_NO_DEFAULT_CONSTRUCTOR, member);
            }
        }
        st = stmtNode(nullptr, b, st_expr);
        optimize_for_constants(&exp);
        st->select = exp;
    }
}
static void virtualBaseThunks(BLOCKDATA* b, SYMBOL* sp, EXPRESSION* thisptr)
{
    VBASEENTRY* entries = sp->sb->vbaseEntries;
    EXPRESSION *first = nullptr, **pos = &first;
    STATEMENT* st;
    while (entries)
    {
        EXPRESSION* left = exprNode(en_add, thisptr, intNode(en_c_i, entries->pointerOffset));
        EXPRESSION* right = exprNode(en_add, thisptr, intNode(en_c_i, entries->structOffset));
        EXPRESSION* asn;
        deref(&stdpointer, &left);
        asn = exprNode(en_assign, left, right);
        if (!*pos)
        {
            *pos = asn;
        }
        else
        {
            *pos = exprNode(en_void, *pos, asn);
            pos = &(*pos)->right;
        }
        entries = entries->next;
    }
    if (first)
    {
        st = stmtNode(nullptr, b, st_expr);
        optimize_for_constants(&first);
        st->select = first;
    }
}
static void HandleEntries(EXPRESSION** pos, VTABENTRY* entries, EXPRESSION* thisptr, EXPRESSION* vtabBase, bool isvirtual)
{
    VTABENTRY* children = entries->children;
    while (entries)
    {
        if (!entries->isdead && entries->isvirtual == isvirtual && hasVTab(entries->cls))
        {
            EXPRESSION* left = exprNode(en_add, thisptr, intNode(en_c_i, entries->dataOffset));
            EXPRESSION* right =
                exprNode(en_add, exprNode(en_add, vtabBase, intNode(en_c_i, entries->vtabOffset)), intNode(en_c_i, VTAB_XT_OFFS));
            EXPRESSION* asn;
            deref(&stdpointer, &left);
            asn = exprNode(en_assign, left, right);
            if (!*pos)
            {
                *pos = asn;
            }
            else
            {
                *pos = exprNode(en_void, *pos, asn);
                pos = &(*pos)->right;
            }
        }
        entries = entries->next;
    }
    if (children)
    {
        HandleEntries(pos, children, thisptr, vtabBase, isvirtual);
    }
}
static void dovtabThunks(BLOCKDATA* b, SYMBOL* sym, EXPRESSION* thisptr, bool isvirtual)
{
    VTABENTRY* entries = sym->sb->vtabEntries;
    EXPRESSION* first = nullptr;
    STATEMENT* st;
    SYMBOL* localsp;
    localsp = sym->sb->vtabsp;
    EXPRESSION* vtabBase = varNode(en_global, localsp);
    if (localsp->sb->attribs.inheritable.linkage2 == lk_import)
        deref(&stdpointer, &vtabBase);
    HandleEntries(&first, entries, thisptr, vtabBase, isvirtual);
    if (first)
    {
        st = stmtNode(nullptr, b, st_expr);
        optimize_for_constants(&first);
        st->select = first;
    }
}
static void doVirtualBases(BLOCKDATA* b, SYMBOL* sp, MEMBERINITIALIZERS* mi, VBASEENTRY* vbe, EXPRESSION* thisptr,
                           EXPRESSION* otherptr, SYMBOL* parentCons, bool doCopy)
{
    if (vbe)
    {
        doVirtualBases(b, sp, mi, vbe->next, thisptr, otherptr, parentCons, doCopy);
        if (vbe->alloc)
            genConstructorCall(b, sp, mi, vbe->cls, vbe->structOffset, false, thisptr, otherptr, parentCons, true, doCopy, false);
    }
}
static EXPRESSION* unshim(EXPRESSION* exp, EXPRESSION* ths);
static STATEMENT* unshimstmt(STATEMENT* block, EXPRESSION* ths)
{
    STATEMENT *rv = nullptr, **last = &rv;
    while (block != nullptr)
    {
        *last = Allocate<STATEMENT>();
        **last = *block;
        block = *last;
        last = &(*last)->next;
        switch (block->type)
        {
            case st__genword:
                break;
            case st_try:
            case st_catch:
            case st___try:
            case st___catch:
            case st___finally:
            case st___fault:
                block->lower = unshimstmt(block->lower, ths);
                block->blockTail = unshimstmt(block->blockTail, ths);
                break;
            case st_return:
            case st_expr:
            case st_declare:
                block->select = unshim(block->select, ths);
                break;
            case st_goto:
            case st_label:
                break;
            case st_select:
            case st_notselect:
                block->select = unshim(block->select, ths);
                break;
            case st_switch:
                block->select = unshim(block->select, ths);
                block->lower = unshimstmt(block->lower, ths);
                break;
            case st_block:
                block->lower = unshimstmt(block->lower, ths);
                block->blockTail = unshimstmt(block->blockTail, ths);
                break;
            case st_passthrough:
                break;
            case st_datapassthrough:
                break;
            case st_nop:
                break;
            case st_line:
            case st_varstart:
            case st_dbgblock:
                break;
            default:
                diag("Invalid block type in unshimstmt");
                break;
        }
        block = block->next;
    }
    return rv;
}

static EXPRESSION* unshim(EXPRESSION* exp, EXPRESSION* ths)
{
    EXPRESSION* nw = nullptr;
    if (!exp)
        return exp;
    if (exp->type == en_thisshim)
    {
        return ths;
    }
    nw = Allocate<EXPRESSION>();
    *nw = *exp;
    nw->left = unshim(nw->left, ths);
    nw->right = unshim(nw->right, ths);
    if (nw->type == en_stmt)
    {
        nw->v.stmt = unshimstmt(nw->v.stmt, ths);
    }
    return nw;
}
SYMBOL* findClassName(const char* name, SYMBOL* cls, BASECLASS* bc, VBASEENTRY* vbase, int* offset)
{
    int n = 0;
    char str[1024];
    char* clslst[100];
    char *p = str, *c;
    SYMBOL* sp = nullptr;
    int vcount = 0, ccount = 0;
    strcpy(str, name);
    while ((c = (char *)strstr(p, "::")))
    {
        clslst[n++] = p;
        p = c;
        *p = 0;
        p += 2;
    }
    clslst[n++] = p;

    while (bc)
    {
        if (!bc->isvirtual)
        {
            SYMBOL* parent = bc->cls;
            int i;
            if (parent->tp->type == bt_typedef)
                parent = basetype(parent->tp)->sp;
            for (i = n - 1; i >= 0 && parent;
                 i--, parent = parent->sb->parentClass ? parent->sb->parentClass : parent->sb->parentNameSpace)
                if (strcmp(parent->name, clslst[i]))
                    break;
            if (i < 0 || (i == 0 && parent == nullptr && clslst[0][0] == '\0'))
            {
                ccount++;
                sp = bc->cls;
                *offset = bc->offset;
            }
        }
        bc = bc->next;
    }
    while (vbase)
    {
        SYMBOL* parent = vbase->cls;
        int i;
        if (parent->tp->type == bt_typedef)
            parent = basetype(parent->tp)->sp;
        for (i = n - 1; i >= 0 && parent;
             i--, parent = parent->sb->parentClass ? parent->sb->parentClass : parent->sb->parentNameSpace)
            if (strcmp(parent->name, clslst[i]))
                break;
        if (i < 0 || (i == 0 && parent == nullptr && clslst[0][0] == '\0'))
        {
            vcount++;
            sp = vbase->cls;
            *offset = vbase->structOffset;
        }
        vbase = vbase->next;
    }
    if ((ccount && vcount) || ccount > 1)
    {
        errorsym2(ERR_NOT_UNAMBIGUOUS_BASE, sp, cls);
    }
    return sp;
}
void ParseMemberInitializers(SYMBOL* cls, SYMBOL* cons)
{
    MEMBERINITIALIZERS* init = cons->sb->memberInitializers;
    bool first = true;
    bool hasDelegate = false;
    while (init)
    {
        LEXLIST* lex;
        BASECLASS* bc = cls->sb->baseClasses;
        VBASEENTRY* vbase = cls->sb->vbaseEntries;
        if (!first && hasDelegate)
            error(ERR_DELEGATING_CONSTRUCTOR_ONLY_INITIALIZER);
        init->sp = search(init->name, basetype(cls->tp)->syms);
        if (init->sp && (!init->basesym || !istype(init->sp)))
        {
            if (init->sp->sb->storage_class == sc_typedef)
            {
                TYPE* tp = basetype(init->sp->tp);
                if (isstructured(tp))
                {
                    init->name = basetype(tp)->sp->name;
                    init->sp = search(init->name, basetype(cls->tp)->syms);
                }
            }
        }
        if (init->sp && (!init->basesym || !istype(init->sp)))
        {
            if (init->sp != cls && init->sp->sb->storage_class != sc_member && init->sp->sb->storage_class != sc_mutable)
            {
                errorsym(ERR_NEED_NONSTATIC_MEMBER, init->sp);
            }
            else
            {
                bool done = false;
                lex = SetAlternateLex(init->initData);
                if (MATCHKW(lex, lt))
                {
                    TEMPLATEPARAMLIST* lst = nullptr;
                    lex = GetTemplateArguments(lex, cons, init->sp, &lst);
                    if (init->sp->sb->templateLevel)
                    {
                        init->sp = TemplateClassInstantiate(init->sp, lst, false, sc_global);
                    }
                    else
                    {
                        errorsym(ERR_NOT_A_TEMPLATE, init->sp);
                    }
                }
                if (!isstructured(init->sp->tp))
                {
                    bool bypa = true;
                    if (MATCHKW(lex, openpa) || MATCHKW(lex, begin))
                    {
                        bypa = MATCHKW(lex, openpa);
                        lex = getsym();
                        if ((bypa && MATCHKW(lex, closepa)) || (!bypa && MATCHKW(lex, end)))
                        {
                            lex = getsym();
                            init->init = nullptr;
                            initInsert(&init->init, init->sp->tp, intNode(en_c_i, 0), 0, false);
                            done = true;
                        }
                        else
                        {
                            lex = backupsym();
                        }
                    }
                    if (!done)
                    {
                        needkw(&lex, bypa ? openpa : begin);
                        init->init = nullptr;
                        argumentNesting++;
                        lex = initType(lex, cons, 0, sc_auto, &init->init, nullptr, init->sp->tp, init->sp, false, 0);
                        argumentNesting--;
                        done = true;
                        needkw(&lex, bypa ? closepa : end);
                    }
                }
                else
                {
                    bool empty = false;
                    if (MATCHKW(lex, openpa))
                    {
                        lex = getsym();
                        if (MATCHKW(lex, closepa))
                            empty = true;
                        lex = backupsym();
                    }
                    if (MATCHKW(lex, openpa) && basetype(init->sp->tp)->sp->sb->trivialCons)
                    {
                        init->init = nullptr;
                        argumentNesting++;
                        lex = initType(lex, cons, 0, sc_auto, &init->init, nullptr, init->sp->tp, init->sp, false, 0);
                        argumentNesting--;
                        done = true;
                        if (init->packed || MATCHKW(lex, ellipse))
                            error(ERR_PACK_SPECIFIER_NOT_ALLOWED_HERE);
                    }
                    else
                    {
                        init->init = nullptr;
                        lex = initType(lex, cons, 0, sc_auto, &init->init, nullptr, init->sp->tp, init->sp, false, 0);
                        if (init->packed)
                            error(ERR_PACK_SPECIFIER_NOT_ALLOWED_HERE);
                    }
                    init->valueInit = empty;
                }
                SetAlternateLex(nullptr);
            }
        }
        else
        {
            SYMBOL* sp = init->basesym;
            if (!sp)
                sp = classsearch(init->name, false, true);
            else
                init->sp = sp;
            if (sp && sp->tp->type == bt_templateparam)
            {
                if (sp->tp->templateParam->p->type == kw_typename)
                {
                    if (sp->tp->templateParam->p->packed)
                    {
                        MEMBERINITIALIZERS** p = &cons->sb->memberInitializers;
                        FUNCTIONCALL shim;
                        lex = SetAlternateLex(init->initData);
                        shim.arguments = nullptr;
                        getMemberInitializers(lex, cons, &shim, MATCHKW(lex, openpa) ? closepa : end, true);
                        if (!init->packed)
                            error(ERR_PACK_SPECIFIER_REQUIRED_HERE);
                        SetAlternateLex(nullptr);
                        expandPackedMemberInitializers(cls, cons, sp->tp->templateParam->p->byPack.pack, p, init->initData,
                                                       shim.arguments);
                        init->sp = cls;
                    }
                    else if (sp->tp->templateParam->p->byClass.val && isstructured(sp->tp->templateParam->p->byClass.val))
                    {
                        TYPE* tp = sp->tp->templateParam->p->byClass.val;
                        int offset = 0;
                        int vcount = 0, ccount = 0;
                        init->name = basetype(tp)->sp->name;
                        while (bc)
                        {
                            if (!strcmp(bc->cls->name, init->name))
                            {
                                if (bc->isvirtual)
                                    vcount++;
                                else
                                    ccount++;
                                init->sp = bc->cls;
                                offset = bc->offset;
                            }
                            bc = bc->next;
                        }
                        if ((ccount && vcount) || ccount > 1)
                        {
                            errorsym2(ERR_NOT_UNAMBIGUOUS_BASE, init->sp, cls);
                        }
                        if (init->sp && init->sp == basetype(tp)->sp)
                        {
                            SYMBOL* sp = makeID(sc_member, init->sp->tp, nullptr, init->sp->name);
                            FUNCTIONCALL shim;
                            INITIALIZER** xinit = &init->init;
                            sp->sb->offset = offset;
                            init->sp = sp;
                            lex = SetAlternateLex(init->initData);
                            shim.arguments = nullptr;
                            getMemberInitializers(lex, cons, &shim, MATCHKW(lex, openpa) ? closepa : end, true);
                            if (init->packed)
                                error(ERR_PACK_SPECIFIER_NOT_ALLOWED_HERE);
                            SetAlternateLex(nullptr);
                            while (shim.arguments)
                            {
                                *xinit = Allocate<INITIALIZER>();
                                (*xinit)->basetp = shim.arguments->tp;
                                (*xinit)->exp = shim.arguments->exp;
                                xinit = &(*xinit)->next;
                                shim.arguments = shim.arguments->next;
                            }
                        }
                        else
                        {
                            init->sp = nullptr;
                        }
                    }
                    else
                    {
                        error(ERR_STRUCTURED_TYPE_EXPECTED_IN_TEMPLATE_PARAMETER);
                    }
                }
                else
                {
                    error(ERR_CLASS_TEMPLATE_PARAMETER_EXPECTED);
                }
            }
            else if (init->packed)
            {
                init = expandPackedBaseClasses(
                    cls, cons, init == cons->sb->memberInitializers ? &cons->sb->memberInitializers : &init, bc, vbase);
                continue;
            }
            else
            {
                int offset = 0;
                // check for a delegating constructor call
                if (!strcmp(init->name, cls->name))
                {
                    init->sp = cls;
                    init->delegating = true;
                    if (!first)
                        error(ERR_DELEGATING_CONSTRUCTOR_ONLY_INITIALIZER);
                    hasDelegate = true;
                    cons->sb->delegated = true;
                }
                if (!init->sp)
                    init->sp = findClassName(init->name, cls, bc, vbase, &offset);
                if (init->sp)
                {
                    // have to make a *real* variable as a fudge...
                    SYMBOL* sp;
                    FUNCTIONCALL shim;
                    INITIALIZER** xinit;
                    lex = SetAlternateLex(init->initData);
                    if (MATCHKW(lex, lt))
                    {
                        TEMPLATEPARAMLIST* lst = nullptr;
                        lex = GetTemplateArguments(lex, cons, init->sp, &lst);
                        if (init->sp->sb->templateLevel)
                        {
                            init->sp = TemplateClassInstantiate(init->sp, lst, false, sc_global);
                        }
                        else
                        {
                            errorsym(ERR_NOT_A_TEMPLATE, init->sp);
                        }
                    }
                    sp = makeID(sc_member, init->sp->tp, nullptr, init->sp->name);
                    xinit = &init->init;
                    sp->sb->offset = offset;
                    init->sp = sp;
                    shim.arguments = nullptr;
                    getMemberInitializers(lex, cons, &shim, MATCHKW(lex, openpa) ? closepa : end, true);
                    SetAlternateLex(nullptr);
                    if (init->packed)
                    {
                        error(ERR_PACK_SPECIFIER_NOT_ALLOWED_HERE);
                    }
                    while (shim.arguments)
                    {
                        *xinit = Allocate<INITIALIZER>();
                        (*xinit)->basetp = shim.arguments->tp;
                        (*xinit)->exp = shim.arguments->exp;
                        xinit = &(*xinit)->next;
                        shim.arguments = shim.arguments->next;
                    }
                }
            }
        }
        if (!init->sp)
        {
            // might be a typedef?
            init->sp = finishSearch(init->name, nullptr, nullptr, false, false, false);
            if (init->sp && init->sp->sb->storage_class == sc_typedef)
            {
                int offset = 0;
                TYPE* tp = init->sp->tp;
                tp = basetype(tp);
                if (isstructured(tp))
                {
                    BASECLASS* bc = cls->sb->baseClasses;
                    while (bc)
                    {
                        if (!comparetypes(bc->cls->tp, init->sp->tp, true) || sameTemplate(bc->cls->tp, init->sp->tp))
                            break;
                        bc = bc->next;
                    }
                    if (bc)
                    {
                        // have to make a *real* variable as a fudge...
                        SYMBOL* sp;
                        FUNCTIONCALL shim;
                        INITIALIZER** xinit;
                        lex = SetAlternateLex(init->initData);
                        if (MATCHKW(lex, lt))
                        {
                            TEMPLATEPARAMLIST* lst = nullptr;
                            lex = GetTemplateArguments(lex, cons, init->sp, &lst);
                            if (init->sp->sb->templateLevel)
                            {
                                init->sp = TemplateClassInstantiate(init->sp, lst, false, sc_global);
                            }
                            else
                            {
                                errorsym(ERR_NOT_A_TEMPLATE, init->sp);
                            }
                        }
                        sp = makeID(sc_member, init->sp->tp, nullptr, init->sp->name);
                        xinit = &init->init;
                        sp->sb->offset = offset;
                        init->sp = sp;
                        shim.arguments = nullptr;
                        getMemberInitializers(lex, cons, &shim, MATCHKW(lex, openpa) ? closepa : end, true);
                        if (init->packed)
                            error(ERR_PACK_SPECIFIER_NOT_ALLOWED_HERE);
                        SetAlternateLex(nullptr);
                        while (shim.arguments)
                        {
                            *xinit = Allocate<INITIALIZER>();
                            (*xinit)->basetp = shim.arguments->tp;
                            (*xinit)->exp = shim.arguments->exp;
                            xinit = &(*xinit)->next;
                            shim.arguments = shim.arguments->next;
                        }
                    }
                }
                else
                {
                    init->sp = nullptr;
                }
            }
            else
            {
                init->sp = nullptr;
            }
        }
        if (!init->sp)
        {
            errorstrsym(ERR_NOT_A_MEMBER_OR_BASE_CLASS, init->name, cls);
        }
        first = false;
        init = init->next;
    }
}
static void allocInitializers(SYMBOL* cls, SYMBOL* cons, EXPRESSION* ths)
{
    SYMLIST* hr = basetype(cls->tp)->syms->table[0];
    MEMBERINITIALIZERS* init = cons->sb->memberInitializers;
    while (hr)
    {
        SYMBOL* sp = hr->p;
        if (sp->sb->storage_class == sc_member || sp->sb->storage_class == sc_mutable)
        {
            sp->sb->lastInit = sp->sb->init;
            if (sp->sb->init)
            {
                sp->sb->init = Allocate<INITIALIZER>();
                *sp->sb->init = *sp->sb->lastInit;
                sp->sb->init->exp = unshim(sp->sb->init->exp, ths);
            }
        }
        hr = hr->next;
    }
    while (init)
    {
        if (init->init)
        {
            init->sp->sb->init = init->init;
            if (init->init->exp)
                init->init->exp = unshim(init->init->exp, ths);
        }
        init = init->next;
    }
    if (!cons->sb->delegated)
    {
        hr = basetype(cls->tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* sp = hr->p;
            if (!sp->sb->init && ismember(sp))
            {
                if (isref(sp->tp))
                    errorsym(ERR_REF_MEMBER_MUST_INITIALIZE, sp);
                else if (isconst(sp->tp))
                    errorsym(ERR_CONSTANT_MEMBER_MUST_BE_INITIALIZED, sp);
            }
            hr = hr->next;
        }
    }
}
static void releaseInitializers(SYMBOL* cls, SYMBOL* cons)
{
    (void)cons;
    SYMLIST* hr = basetype(cls->tp)->syms->table[0];
    while (hr)
    {
        SYMBOL* sp = hr->p;
        if (sp->sb->storage_class == sc_member || sp->sb->storage_class == sc_mutable)
            sp->sb->init = sp->sb->lastInit;
        hr = hr->next;
    }
}
EXPRESSION* thunkConstructorHead(BLOCKDATA* b, SYMBOL* sym, SYMBOL* cons, HASHTABLE* syms, bool parseInitializers, bool doCopy,
                                 bool defaulted)
{
    BASECLASS* bc;
    SYMLIST* hr = syms->table[0];
    EXPRESSION* thisptr = varNode(en_auto, hr->p);
    EXPRESSION* otherptr = nullptr;
    int oldCodeLabel = codeLabel;
    if (defaulted)
        codeLabel = INT_MIN;
    if (hr->next)
        otherptr = varNode(en_auto, hr->next->p);
    deref(&stdpointer, &thisptr);
    deref(&stdpointer, &otherptr);
    if (parseInitializers)
        allocInitializers(sym, cons, thisptr);
    if (cons->sb->memberInitializers && cons->sb->memberInitializers->delegating)
    {
        genConstructorCall(b, sym, cons->sb->memberInitializers, sym, 0, false, thisptr, otherptr, cons, true, doCopy,
                           !cons->sb->defaulted);
    }
    else
    {
        if (sym->tp->type == bt_union)
        {
            AllocateLocalContext(nullptr, cons, codeLabel++);
            hr = sym->tp->syms->table[0];
            while (hr)
            {
                SYMBOL* sp = hr->p;
                if ((sp->sb->storage_class == sc_member || sp->sb->storage_class == sc_mutable) && sp->tp->type != bt_aggregate)
                {
                    if (sp->sb->init)
                    {
                        if (isstructured(sp->tp))
                        {
                            genConstructorCall(b, basetype(sp->tp)->sp, cons->sb->memberInitializers, sp, sp->sb->offset, true,
                                               thisptr, otherptr, cons, false, doCopy, !cons->sb->defaulted);
                        }
                        else
                        {
                            genConsData(b, sym, cons->sb->memberInitializers, sp, sp->sb->offset, thisptr, otherptr, cons, doCopy);
                        }
                    }
                }
                hr = hr->next;
            }
            FreeLocalContext(nullptr, cons, codeLabel++);
        }
        else
        {
            if (sym->sb->vbaseEntries)
            {
                SYMBOL* sp = makeID(sc_parameter, &stdint, nullptr, AnonymousName());
                EXPRESSION* val = varNode(en_auto, sp);
                int lbl = codeLabel++;
                STATEMENT* st;
                sp->sb->constop = true;
                sp->sb->decoratedName = sp->name;
                sp->sb->offset = Optimizer::chosenAssembler->arch->retblocksize + cons->sb->paramsize;
                insert(sp, localNameSpace->valueData->syms);

                deref(&stdint, &val);
                st = stmtNode(nullptr, b, st_notselect);
                optimize_for_constants(&val);
                st->select = val;
                st->label = lbl;
                virtualBaseThunks(b, sym, thisptr);
                doVirtualBases(b, sym, cons->sb->memberInitializers, sym->sb->vbaseEntries, thisptr, otherptr, cons, doCopy);
                if (hasVTab(sym))
                    dovtabThunks(b, sym, thisptr, true);
                st = stmtNode(nullptr, b, st_label);
                st->label = lbl;
            }
            AllocateLocalContext(nullptr, cons, codeLabel++);
            bc = sym->sb->baseClasses;
            while (bc)
            {
                if (!bc->isvirtual)
                    genConstructorCall(b, sym, cons->sb->memberInitializers, bc->cls, bc->offset, false, thisptr, otherptr, cons,
                                       true, doCopy || !cons->sb->memberInitializers, !cons->sb->defaulted);
                bc = bc->next;
            }
            if (hasVTab(sym))
                dovtabThunks(b, sym, thisptr, false);
            hr = sym->tp->syms->table[0];
            while (hr)
            {
                SYMBOL* sp = hr->p;
                if ((sp->sb->storage_class == sc_member || sp->sb->storage_class == sc_mutable) && sp->tp->type != bt_aggregate && !sp->sb->wasUsing)
                {
                    if (isstructured(sp->tp))
                    {
                        genConstructorCall(b, basetype(sp->tp)->sp, cons->sb->memberInitializers, sp, sp->sb->offset, true, thisptr,
                                           otherptr, cons, false, doCopy, !cons->sb->defaulted);
                    }
                    else
                    {
                        genConsData(b, sym, cons->sb->memberInitializers, sp, sp->sb->offset, thisptr, otherptr, cons, doCopy);
                    }
                }
                hr = hr->next;
            }
            FreeLocalContext(nullptr, cons, codeLabel++);
        }
    }
    if (parseInitializers)
        releaseInitializers(sym, cons);
    cons->sb->labelCount = codeLabel - INT_MIN;
    if (defaulted)
        codeLabel = oldCodeLabel;
    return thisptr;
}
static bool DefaultConstructorConstExpression(SYMBOL* sp)
{
    if (sp->sb->constexpression)
        return true;
    if (sp->sb->vbaseEntries)
        return false;
    if (sp->tp->type != bt_union)
    {
        SYMLIST* hr;
        hr = basetype(sp->tp)->syms->table[0];
        while (hr)
        {
            SYMBOL* sp1 = hr->p;
            TYPE* m;
            if (sp1->sb->storage_class == sc_mutable)
                return false;
            if (sp1->sb->storage_class == sc_member)
            {
                if (!is_literal_type(sp1->tp))
                    return false;
                if (!sp1->sb->memberInitializers)
                    return false;
            }
            hr = hr->next;
        }
    }

    BASECLASS* base;
    base = sp->sb->baseClasses;
    while (base)
    {
        if (!DefaultConstructorConstExpression(base->cls))
            return false;
        base = base->next;
    }
    sp->sb->constexpression = true;
    return true;
}

void createConstructor(SYMBOL* sp, SYMBOL* consfunc)
{
    HASHTABLE* syms;
    BLOCKDATA b = {};
    STATEMENT* st;
    EXPRESSION* thisptr;
    bool oldNoExcept = noExcept;
    noExcept = true;
    b.type = begin;
    syms = localNameSpace->valueData->syms;
    localNameSpace->valueData->syms = basetype(consfunc->tp)->syms;
    thisptr = thunkConstructorHead(&b, sp, consfunc, basetype(consfunc->tp)->syms, false, true, true);
    st = stmtNode(nullptr, &b, st_return);
    st->select = thisptr;
    if (!inNoExceptHandler)
    {
        consfunc->sb->inlineFunc.stmt = stmtNode(nullptr, nullptr, st_block);
        consfunc->sb->inlineFunc.stmt->lower = b.head;
        consfunc->sb->inlineFunc.syms = basetype(consfunc->tp)->syms;
        consfunc->sb->retcount = 1;
        consfunc->sb->attribs.inheritable.isInline = true;
        //    consfunc->sb->inlineFunc.stmt->blockTail = b.tail;
        InsertInline(consfunc);
        defaultRecursionMap.clear();
        if (noExcept)
        {
            consfunc->sb->xcMode = xc_none;
            consfunc->sb->noExcept = true;
        }
        else
        {
            consfunc->sb->xcMode = xc_unspecified;
            consfunc->sb->noExcept = false;
        }
    }
    else
    {
        defaultRecursionMap.clear();
        if (noExcept)
        {
            consfunc->sb->noExcept = true;
        }
        else
        {
            consfunc->sb->noExcept = false;
        }
    }
    consfunc->sb->constexpression =
        DefaultConstructorConstExpression(sp) || matchesCopy(consfunc, false) || matchesCopy(consfunc, true);
    sp->sb->literalClass |= consfunc->sb->constexpression;
    localNameSpace->valueData->syms = syms;
    noExcept &= oldNoExcept;
}
void asnVirtualBases(BLOCKDATA* b, SYMBOL* sp, VBASEENTRY* vbe, EXPRESSION* thisptr, EXPRESSION* other, bool move, bool isconst)
{
    if (vbe)
    {
        asnVirtualBases(b, sp, vbe->next, thisptr, other, move, isconst);
        if (vbe->alloc)
            genAsnCall(b, sp, vbe->cls, vbe->structOffset, thisptr, other, move, isconst);
    }
}
static void genAsnData(BLOCKDATA* b, SYMBOL* cls, SYMBOL* member, int offset, EXPRESSION* thisptr, EXPRESSION* other)
{
    EXPRESSION* left = exprNode(en_structadd, thisptr, intNode(en_c_i, offset));
    EXPRESSION* right = exprNode(en_structadd, other, intNode(en_c_i, offset));
    left->right->keepZero = true;
    right->right->keepZero = true;
    STATEMENT* st;
    (void)cls;
    if (isstructured(member->tp) || isarray(member->tp))
    {
        left = exprNode(en_blockassign, left, right);
        left->size = member->tp->size;
        left->altdata = (void*)member->tp;
    }
    else
    {
        deref(member->tp, &left);
        deref(member->tp, &right);
        left = exprNode(en_assign, left, right);
    }
    st = stmtNode(nullptr, b, st_expr);
    optimize_for_constants(&left);
    st->select = left;
}
static void genAsnCall(BLOCKDATA* b, SYMBOL* cls, SYMBOL* base, int offset, EXPRESSION* thisptr, EXPRESSION* other, bool move,
                       bool isconst)
{
    (void)cls;
    EXPRESSION* exp = nullptr;
    STATEMENT* st;
    FUNCTIONCALL* params = Allocate<FUNCTIONCALL>();
    TYPE* tp = CopyType(base->tp);
    SYMBOL* asn1;
    SYMBOL* cons = search(overloadNameTab[assign - kw_new + CI_NEW], basetype(base->tp)->syms);
    EXPRESSION* left = exprNode(en_add, thisptr, intNode(en_c_i, offset));
    EXPRESSION* right = exprNode(en_add, other, intNode(en_c_i, offset));
    if (move)
    {
        right = exprNode(en_not_lvalue, right, nullptr);
    }
    if (isconst)
    {
        tp = MakeType(bt_const, tp);
    }
    if (move)
    {
        tp->rref = true;
        tp->lref = false;
    }
    else
    {
        tp->lref = true;
        tp->rref = false;
    }
    params->arguments = Allocate<INITLIST>();
    params->arguments->tp = tp;
    params->arguments->exp = right;
    params->thisptr = left;
    params->thistp = MakeType(bt_pointer, base->tp);
    params->ascall = true;
    asn1 = GetOverloadedFunction(&tp, &params->fcall, cons, params, nullptr, true, false, true, 0);

    if (asn1)
    {
        SYMBOL* parm;
        AdjustParams(asn1, basetype(asn1->tp)->syms->table[0], &params->arguments, false, true);
        parm = (SYMBOL*)basetype(asn1->tp)->syms->table[0]->next->p;
        if (parm && isref(parm->tp))
        {
            params->arguments->tp = MakeType(bt_lref, params->arguments->tp);
        }
        if (!isAccessible(base, base, asn1, nullptr, ac_protected, false))
        {
            errorsym(ERR_CANNOT_ACCESS, asn1);
        }
        if (asn1->sb->defaulted && !asn1->sb->inlineFunc.stmt)
            createAssignment(base, asn1);
        noExcept &= asn1->sb->noExcept;

        params->functp = asn1->tp;
        params->sp = asn1;
        params->ascall = true;
        exp = varNode(en_func, nullptr);
        exp->v.func = params;
    }
    st = stmtNode(nullptr, b, st_expr);
    optimize_for_constants(&exp);
    st->select = exp;
}
static EXPRESSION* thunkAssignments(BLOCKDATA* b, SYMBOL* sym, SYMBOL* asnfunc, HASHTABLE* syms, bool move, bool isconst)
{
    SYMLIST* hr = syms->table[0];
    EXPRESSION* thisptr = varNode(en_auto, hr->p);
    EXPRESSION* other = nullptr;
    BASECLASS* base;
    int oldCodeLabel = codeLabel;
    codeLabel = INT_MIN;
    if (hr->next)  // this had better be true
        other = varNode(en_auto, hr->next->p);
    deref(&stdpointer, &thisptr);
    deref(&stdpointer, &other);
    if (sym->tp->type == bt_union)
    {
        genAsnData(b, sym, sym, 0, thisptr, other);
    }
    else
    {
        if (sym->sb->vbaseEntries)
        {
            asnVirtualBases(b, sym, sym->sb->vbaseEntries, thisptr, other, move, isconst);
        }
        base = sym->sb->baseClasses;
        while (base)
        {
            if (!base->isvirtual)
            {
                genAsnCall(b, sym, base->cls, base->offset, thisptr, other, move, isconst);
            }
            base = base->next;
        }
        hr = sym->tp->syms->table[0];
        while (hr)
        {
            SYMBOL* sp = hr->p;
            if ((sp->sb->storage_class == sc_member || sp->sb->storage_class == sc_mutable) && sp->tp->type != bt_aggregate)
            {
                if (isstructured(sp->tp))
                {
                    genAsnCall(b, sym, basetype(sp->tp)->sp, sp->sb->offset, thisptr, other, move, isconst);
                }
                else
                {
                    genAsnData(b, sym, sp, sp->sb->offset, thisptr, other);
                }
            }
            hr = hr->next;
        }
    }
    asnfunc->sb->labelCount = codeLabel - INT_MIN;
    codeLabel = oldCodeLabel;
    return thisptr;
}
void createAssignment(SYMBOL* sym, SYMBOL* asnfunc)
{
    // if we get here we are just assuming it is a builtin assignment operator
    // because we only get here for 'default' functions and that is the only one
    // that can be defaulted...
    HASHTABLE* syms;
    bool oldNoExcept = noExcept;
    noExcept = true;
    BLOCKDATA b = {};
    bool move = basetype(((SYMBOL*)basetype(asnfunc->tp)->syms->table[0]->next->p)->tp)->type == bt_rref;
    bool isConst = isconst(((SYMBOL*)basetype(asnfunc->tp)->syms->table[0]->next->p)->tp);
    b.type = begin;
    syms = localNameSpace->valueData->syms;
    localNameSpace->valueData->syms = basetype(asnfunc->tp)->syms;
    auto thisptr = thunkAssignments(&b, sym, asnfunc, basetype(asnfunc->tp)->syms, move, isConst);
    auto st = stmtNode(nullptr, &b, st_return);
    st->select = thisptr;
    if (!inNoExceptHandler)
    {
        asnfunc->sb->inlineFunc.stmt = stmtNode(nullptr, nullptr, st_block);
        asnfunc->sb->inlineFunc.stmt->lower = b.head;
        asnfunc->sb->inlineFunc.syms = basetype(asnfunc->tp)->syms;
        asnfunc->sb->attribs.inheritable.isInline = true;
        //    asnfunc->sb->inlineFunc.stmt->blockTail = b.tail;
        InsertInline(asnfunc);

        defaultRecursionMap.clear();
        if (noExcept)
        {
            asnfunc->sb->xcMode = xc_none;
            asnfunc->sb->noExcept = true;
        }
        else
        {
            asnfunc->sb->xcMode = xc_unspecified;
            asnfunc->sb->noExcept = false;
        }
    }
    else
    {
        defaultRecursionMap.clear();
        if (noExcept)
        {
            asnfunc->sb->noExcept = true;
        }
        else
        {
            asnfunc->sb->noExcept = false;
        }
    }
    localNameSpace->valueData->syms = syms;
    noExcept &= oldNoExcept;
}
static void genDestructorCall(BLOCKDATA* b, SYMBOL* sp, SYMBOL* against, EXPRESSION* base, EXPRESSION* arrayElms, int offset,
                              bool top)
{
    SYMBOL* dest;
    EXPRESSION* exp;
    STATEMENT* st;
    TYPE* tp = PerformDeferredInitialization(sp->tp, nullptr);
    sp = tp->sp;
    dest = search(overloadNameTab[CI_DESTRUCTOR], basetype(sp->tp)->syms);
    if (!dest)  // error handling
        return;
    exp = base;
    deref(&stdpointer, &exp);
    exp = exprNode(en_add, exp, intNode(en_c_i, offset));
    dest = (SYMBOL*)basetype(dest->tp)->syms->table[0]->p;
    if (dest->sb->defaulted && !dest->sb->inlineFunc.stmt)
    {
        createDestructor(sp);
    }
    callDestructor(sp, against, &exp, arrayElms, top, true, false, true);
    st = stmtNode(nullptr, b, st_expr);
    optimize_for_constants(&exp);
    st->select = exp;
}
static void undoVars(BLOCKDATA* b, SYMLIST* vars, EXPRESSION* base)
{
    if (vars)
    {
        std::stack<SYMBOL*> stk;
        while (vars)
        {
            stk.push(vars->p);
            vars = vars->next;
        }
        while (stk.size())
        {
            SYMBOL* s = (SYMBOL*)stk.top();
            stk.pop();
            if ((s->sb->storage_class == sc_member || s->sb->storage_class == sc_mutable) && !s->sb->wasUsing)
            {
                if (isstructured(s->tp))
                {
                    genDestructorCall(b, (SYMBOL*)basetype(s->tp)->sp, nullptr, base, nullptr, s->sb->offset, true);
                }
                else if (isarray(s->tp))
                {
                    TYPE* tp = s->tp;
                    while (isarray(tp))
                        tp = basetype(tp)->btp;
                    tp = basetype(tp);
                    if (isstructured(tp))
                        genDestructorCall(b, tp->sp, nullptr, base, intNode(en_c_i, s->tp->size / tp->size), s->sb->offset, true);
                }
            }
        }
    }
}
static void undoBases(BLOCKDATA* b, SYMBOL* against, BASECLASS* bc, EXPRESSION* base)
{
    if (bc)
    {
        undoBases(b, against, bc->next, base);
        if (!bc->isvirtual)
        {
            genDestructorCall(b, bc->cls, against, base, nullptr, bc->offset, false);
        }
    }
}
void thunkDestructorTail(BLOCKDATA* b, SYMBOL* sp, SYMBOL* dest, HASHTABLE* syms, bool defaulted)
{
    if (sp->tp->type != bt_union)
    {
        EXPRESSION* thisptr;
        VBASEENTRY* vbe = sp->sb->vbaseEntries;
        int oldCodeLabel = codeLabel;
        if (templateNestingCount)
            return;
        if (defaulted)
            codeLabel = INT_MIN;
        thisptr = varNode(en_auto, (SYMBOL*)syms->table[0]->p);
        undoVars(b, basetype(sp->tp)->syms->table[0], thisptr);
        undoBases(b, sp, sp->sb->baseClasses, thisptr);
        if (vbe)
        {
            SYMBOL* sp = (SYMBOL*)syms->table[0]->next->p;
            EXPRESSION* val = varNode(en_auto, sp);
            int lbl = codeLabel++;
            STATEMENT* st;
            sp->sb->decoratedName = sp->name;
            sp->sb->offset = Optimizer::chosenAssembler->arch->retblocksize + getSize(bt_pointer);
            Optimizer::SymbolManager::Get(sp)->offset = sp->sb->offset;
            deref(&stdint, &val);
            st = stmtNode(nullptr, b, st_notselect);
            optimize_for_constants(&val);
            st->select = val;
            st->label = lbl;
            while (vbe)
            {
                if (vbe->alloc)
                    genDestructorCall(b, vbe->cls, sp, thisptr, nullptr, vbe->structOffset, false);
                vbe = vbe->next;
            }
            st = stmtNode(nullptr, b, st_label);
            st->label = lbl;
        }
        dest->sb->labelCount = codeLabel - INT_MIN;
        if (defaulted)
            codeLabel = oldCodeLabel;
    }
}
void createDestructor(SYMBOL* sp)
{
    HASHTABLE* syms;
    SYMBOL* dest = search(overloadNameTab[CI_DESTRUCTOR], basetype(sp->tp)->syms);
    bool oldNoExcept = noExcept;
    noExcept = true;
    BLOCKDATA b = {};
    b.type = begin;
    dest = (SYMBOL*)basetype(dest->tp)->syms->table[0]->p;
    syms = localNameSpace->valueData->syms;
    localNameSpace->valueData->syms = basetype(dest->tp)->syms;
    thunkDestructorTail(&b, sp, dest, basetype(dest->tp)->syms, true);
    if (!inNoExceptHandler)
    {
        dest->sb->inlineFunc.stmt = stmtNode(nullptr, nullptr, st_block);
        dest->sb->inlineFunc.stmt->lower = b.head;
        dest->sb->inlineFunc.syms = basetype(dest->tp)->syms;
        dest->sb->retcount = 1;
        dest->sb->attribs.inheritable.isInline = dest->sb->attribs.inheritable.linkage2 != lk_export;
        InsertInline(dest);
    }
    if (noExcept)
    {
        dest->sb->xcMode = xc_none;
        dest->sb->noExcept = true;
        noExcept = true;
    }
    else
    {
        dest->sb->xcMode = xc_unspecified;
        dest->sb->noExcept = false;
    }
    //    dest->sb->inlineFunc.stmt->blockTail = b.tail;
    localNameSpace->valueData->syms = syms;
    noExcept &= oldNoExcept;
}
void makeArrayConsDest(TYPE** tp, EXPRESSION** exp, SYMBOL* cons, SYMBOL* dest, EXPRESSION* count)
{
    EXPRESSION* size = intNode(en_c_i, (*tp)->size);
    EXPRESSION *econs = (cons ? varNode(en_pc, cons) : nullptr), *edest = varNode(en_pc, dest);
    FUNCTIONCALL* params = Allocate<FUNCTIONCALL>();
    SYMBOL* asn1;
    INITLIST* arg0 = Allocate<INITLIST>();  // this
    INITLIST* arg1 = Allocate<INITLIST>();  // cons
    INITLIST* arg2 = Allocate<INITLIST>();  // dest
    INITLIST* arg3 = Allocate<INITLIST>();  // size
    INITLIST* arg4 = Allocate<INITLIST>();  // count
    SYMBOL* ovl = namespacesearch("__arrCall", globalNameSpace, false, false);
    params->arguments = arg0;
    arg0->next = arg1;
    arg1->next = arg2;
    arg2->next = arg3;
    arg3->next = arg4;

    if (econs)
        thunkForImportTable(&econs);
    thunkForImportTable(&edest);
    arg0->exp = *exp;
    arg0->tp = &stdpointer;
    arg1->exp = econs ? econs : intNode(en_c_i, 0);
    arg1->tp = &stdpointer;
    arg2->exp = edest;
    arg2->tp = &stdpointer;
    arg3->exp = count;
    arg3->tp = &stdint;
    arg4->exp = size;
    arg4->tp = &stdint;

    params->ascall = true;
    asn1 = GetOverloadedFunction(tp, &params->fcall, ovl, params, nullptr, true, false, true, 0);
    if (!asn1)
    {
        diag("makeArrayConsDest: Can't call array iterator");
    }
    else
    {
        params->functp = asn1->tp;
        params->sp = asn1;
        params->ascall = true;
        *exp = varNode(en_func, nullptr);
        (*exp)->v.func = params;
    }
}
bool callDestructor(SYMBOL* sp, SYMBOL* against, EXPRESSION** exp, EXPRESSION* arrayElms, bool top, bool pointer, bool skipAccess,
                    bool novtab)
{
    if (!sp)
        return false;
    SYMBOL* dest;
    SYMBOL* dest1;
    TYPE *tp = nullptr, *stp;
    FUNCTIONCALL* params = Allocate<FUNCTIONCALL>();
    SYMBOL* sym;
    if (!against)
        against = theCurrentFunc ? theCurrentFunc->sb->parentClass : sp;
    if (sp->tp->size == 0)
        sp = PerformDeferredInitialization(sp->tp, nullptr)->sp;
    stp = sp->tp;
    dest = search(overloadNameTab[CI_DESTRUCTOR], basetype(sp->tp)->syms);
    // if it isn't already defined get out, there will be an error from somewhere else..
    if (!basetype(sp->tp)->syms || !dest)
        return false;
    sym = basetype(sp->tp)->sp;
    if (!*exp)
    {
        diag("callDestructor: no this pointer");
    }
    params->thisptr = *exp;
    params->thistp = MakeType(bt_pointer, sp->tp);
    params->ascall = true;
    dest1 = basetype(dest->tp)->syms->table[0]->p;
    if (!dest1 || !dest1->sb->defaulted || dest1->sb->storage_class == sc_virtual)
    {
        dest1 = GetOverloadedFunction(&tp, &params->fcall, dest, params, nullptr, true, false, true, inNothrowHandler ? _F_IS_NOTHROW : 0);
        if (!novtab && dest1 && dest1->sb->storage_class == sc_virtual)
        {
            auto exp_in = params->thisptr;
            deref(&stdpointer, &exp_in);
            exp_in = exprNode(en_add, exp_in, intNode(en_c_i, dest1->sb->vtaboffset));
            deref(&stdpointer, &exp_in);
            params->fcall = exp_in;
        }
        if (dest1)
            noExcept &= dest1->sb->noExcept;
    }
    else
        params->fcall = varNode(en_pc, dest1);
    if (dest1)
    {
        CheckCalledException(dest1, params->thisptr);
        if (!skipAccess && dest1 &&
            !isAccessible(against, sp, dest1, theCurrentFunc,
                          top ? (theCurrentFunc && theCurrentFunc->sb->parentClass == sp ? ac_protected : ac_public) : ac_protected,
                          false))
        {
            errorsym(ERR_CANNOT_ACCESS, dest1);
        }
        if (dest1 && dest1->sb->defaulted && !dest1->sb->inlineFunc.stmt)
            createDestructor(sp);
        params->functp = dest1->tp;
        params->sp = dest1;
        params->ascall = true;
        if (arrayElms)
        {
            makeArrayConsDest(&stp, exp, nullptr, dest1, arrayElms);
        }
        else
        {
            if (sp->sb->vbaseEntries)
            {
                INITLIST *x = Allocate<INITLIST>(), **p;
                x->tp = MakeType(bt_int);
                x->exp = intNode(en_c_i, top);
                p = &params->arguments;
                while (*p)
                    p = &(*p)->next;
                *p = x;
                params->sp->sb->noinline = true;
            }
            *exp = varNode(en_func, nullptr);
            (*exp)->v.func = params;
        }
        if (*exp && !pointer)
        {
            *exp = exprNode(en_thisref, *exp, nullptr);
            (*exp)->dest = true;
            (*exp)->v.t.thisptr = params->thisptr;
            (*exp)->v.t.tp = sp->tp;
            sp->sb->hasDest = true;
            if (!sp->sb->pureDest)
                hasXCInfo = true;
        }
    }
    return true;
}
bool callConstructor(TYPE** tp, EXPRESSION** exp, FUNCTIONCALL* params, bool checkcopy, EXPRESSION* arrayElms, bool top,
                     bool maybeConversion, bool implicit, bool pointer, bool usesInitList, bool isAssign, bool toErr)
{
    (void)checkcopy;
    TYPE* stp = *tp;
    SYMBOL* sp;
    SYMBOL* against;
    SYMBOL* cons;
    SYMBOL* cons1;
    EXPRESSION* e1 = nullptr;
    TYPE* initializerListTemplate = nullptr;
    TYPE* initializerListType = nullptr;
    bool initializerRef = false;
    PerformDeferredInitialization(stp, nullptr);
    sp = basetype(*tp)->sp;
    against = theCurrentFunc ? theCurrentFunc->sb->parentClass : top ? sp : sp->sb->parentClass;

    if (isAssign)
        cons = search(overloadNameTab[assign - kw_new + CI_NEW], basetype(sp->tp)->syms);
    else
        cons = search(overloadNameTab[CI_CONSTRUCTOR], basetype(sp->tp)->syms);

    if (!params)
    {
        params = Allocate<FUNCTIONCALL>();
    }
    else
    {
        INITLIST* list = params->arguments;
        while (list)
        {
            if (!list->nested && isstructured(list->tp))
            {
                SYMBOL* sp1 = basetype(list->tp)->sp;
                if (!templateNestingCount && sp1->sb->templateLevel && sp1->templateParams && !sp1->sb->instantiated)
                {
                    if (!allTemplateArgsSpecified(sp1, sp1->templateParams))
                        sp1 = GetClassTemplate(sp1, sp1->templateParams->next, false);
                    if (sp1)
                        list->tp = TemplateClassInstantiate(sp1, sp1->templateParams, false, sc_global)->tp;
                }
            }
            list = list->next;
        }
    }
    params->thisptr = *exp;
    params->thistp = MakeType(bt_pointer, sp->tp);
    params->ascall = true;

    cons1 = GetOverloadedFunction(tp, &params->fcall, cons, params, nullptr, toErr, maybeConversion, true, usesInitList | _F_INCONSTRUCTOR | (inNothrowHandler ? _F_IS_NOTHROW : 0));

    if (cons1 && isfunction(cons1->tp))
    {
        CheckCalledException(cons1, params->thisptr);

        if (cons1->sb->castoperator)
        {
            FUNCTIONCALL* oparams = Allocate<FUNCTIONCALL>();
            if (!inNoExceptHandler &&
                !isAccessible(cons1->sb->parentClass, cons1->sb->parentClass, cons1, nullptr, ac_public, false))
            {
                errorsym(ERR_CANNOT_ACCESS, cons1);
            }
            if (cons1->sb->isExplicit && implicit)
                error(ERR_IMPLICIT_USE_OF_EXPLICIT_CONVERSION);
            oparams->fcall = params->fcall;
            oparams->thisptr = params->arguments->exp;
            oparams->thistp = MakeType(bt_pointer, cons1->sb->parentClass->tp);
            oparams->functp = cons1->tp;
            oparams->sp = cons1;
            oparams->ascall = true;
            if (!isref(basetype(cons1->tp)->btp))
            {
                optimize_for_constants(exp);
                oparams->returnEXP = *exp;
                oparams->returnSP = sp;
            }
            e1 = varNode(en_func, nullptr);
            e1->v.func = oparams;
        }
        else
        {
            if (!inNoExceptHandler &&
                !isAccessible(against, sp, cons1, theCurrentFunc,
                              top ? (theCurrentFunc && theCurrentFunc->sb->parentClass == sp ? ac_private : ac_public) : ac_private,
                              false))
            {
                errorsym(ERR_CANNOT_ACCESS, cons1);
            }
            if (cons1->sb->isExplicit && implicit)
                error(ERR_IMPLICIT_USE_OF_EXPLICIT_CONVERSION);
            {
                SYMLIST* hr = basetype(cons1->tp)->syms->table[0];
                if (hr->p->sb->thisPtr)
                    hr = hr->next;
                TYPE* tp = hr->p->tp;
                if (isref(tp))
                {
                    initializerRef = true;
                    tp = basetype(tp)->btp;
                }
                if (isstructured(tp))
                {
                    SYMBOL* sym = (basetype(tp)->sp);
                    if (sym->sb->initializer_list && sym->sb->templateLevel)
                    {
                        initializerListTemplate = sym->tp;
                        initializerListType = sym->templateParams->next->p->byClass.val;
                    }
                }
            }
            if (initializerListType && (!params->arguments->tp || !isstructured(params->arguments->tp) ||
                                        !basetype(params->arguments->tp)->sp->sb->initializer_list))
            {
                auto old = params->arguments->next;
                if (params->arguments && params->arguments->nested && params->arguments->nested->nested &&
                    !params->arguments->initializer_list)
                    params->arguments->next = nullptr;
                auto temp = params->arguments;
                if (!params->arguments->initializer_list)
                    params->arguments = params->arguments->nested;
                CreateInitializerList(cons1, initializerListTemplate, initializerListType, &params->arguments, false,
                                      initializerRef);
                if (temp && (!temp->initializer_list || (temp->nested && temp->nested->nested && !temp->initializer_list)))
                    params->arguments->next = old;
                if (basetype(cons1->tp)->syms->table[0]->next->next)
                    AdjustParams(cons1, basetype(cons1->tp)->syms->table[0]->next->next, &params->arguments->next, false,
                                 implicit && !cons1->sb->isExplicit);
            }
            else
            {
                if (params->arguments && params->arguments->nested && !params->arguments->initializer_list)
                    params->arguments = params->arguments->nested;
                AdjustParams(cons1, basetype(cons1->tp)->syms->table[0], &params->arguments, false,
                             implicit && !cons1->sb->isExplicit);
            }
            params->functp = cons1->tp;
            params->sp = cons1;
            params->ascall = true;
            if (cons1->sb->defaulted && !cons1->sb->inlineFunc.stmt)
                createConstructor(sp, cons1);
            noExcept &= cons1->sb->noExcept;
            if (arrayElms)
            {
                SYMBOL* dest = search(overloadNameTab[CI_DESTRUCTOR], basetype(sp->tp)->syms);
                SYMBOL* dest1;
                SYMBOL* against = top ? sp : sp->sb->parentClass;
                TYPE* tp = nullptr;
                FUNCTIONCALL* params = Allocate<FUNCTIONCALL>();
                if (!*exp)
                {
                    diag("callDestructor: no this pointer");
                }
                params->thisptr = *exp;
                params->thistp = MakeType(bt_pointer, sp->tp);
                params->ascall = true;
                dest1 = GetOverloadedFunction(&tp, &params->fcall, dest, params, nullptr, true, false, true, 0);
                if (dest1 &&
                    !isAccessible(against, sp, dest1, nullptr,
                                  top ? (theCurrentFunc && theCurrentFunc->sb->parentClass == sp ? ac_protected : ac_public)
                                      : ac_protected,
                                  false))
                {
                    errorsym(ERR_CANNOT_ACCESS, dest1);
                }
                if (dest1 && dest1->sb->defaulted && !dest1->sb->inlineFunc.stmt)
                    createDestructor(sp);
                makeArrayConsDest(&stp, exp, cons1, dest1, arrayElms);
                e1 = *exp;
            }
            else
            {
                if (sp->sb->vbaseEntries)
                {
                    INITLIST *x = Allocate<INITLIST>(), **p;
                    x->tp = MakeType(bt_int);
                    x->exp = intNode(en_c_i, top);
                    p = &params->arguments;
                    while (*p)
                        p = &(*p)->next;
                    *p = x;
                    params->sp->sb->noinline = true;
                }
                e1 = varNode(en_func, nullptr);
                e1->v.func = params;
            }
        }

        *exp = e1;
        if ((Optimizer::architecture == ARCHITECTURE_MSIL) && *exp)
        {
            // this needs work, won't work for structures as members of other structures...
            EXPRESSION* exp1 = params->thisptr;
            if ((exp1->type == en_add || exp1->type == en_structadd) && isconstzero(&stdint, exp1->right))
                exp1 = exp1->left;
            *exp = exprNode(en_assign, exp1, *exp);
            params->thisptr = nullptr;
        }
        else if (*exp && !pointer)
        {
            *exp = exprNode(en_thisref, *exp, nullptr);
            (*exp)->v.t.thisptr = params->thisptr;
            (*exp)->v.t.tp = sp->tp;
            optimize_for_constants(exp);  // for constexpr constructors
            // hasXCInfo = true;
        }

        return true;
    }
    return false;
}
bool callConstructorParam(TYPE** tp, EXPRESSION** exp, TYPE* paramTP, EXPRESSION* paramExp, bool top, bool maybeConversion,
                          bool implicit, bool pointer, bool toErr)
{
    FUNCTIONCALL* params = Allocate<FUNCTIONCALL>();
    if (paramTP && paramExp)
    {
        params->arguments = Allocate<INITLIST>();
        params->arguments->tp = paramTP;
        params->arguments->exp = paramExp;
    }
    return callConstructor(tp, exp, params, false, nullptr, top, maybeConversion, implicit, pointer, false, false, toErr);
}

void PromoteConstructorArgs(SYMBOL* cons1, FUNCTIONCALL* params)
{
    if (!cons1)
    {
        return;
    }
    SYMLIST* hr = basetype(cons1->tp)->syms->table[0];
    if (hr->p->sb->thisPtr)
        hr = hr->next;
    INITLIST* args = params->arguments;
    while (hr && args)
    {
        SYMBOL* sp = hr->p;
        TYPE* tps = basetype(sp->tp);
        TYPE* tpa = basetype(args->tp);
        if (isarithmetic(tps) && isarithmetic(tpa))
        {
            if (tps->type > bt_int && tps->type != tpa->type)
            {
                args->tp = sp->tp;
                cast(sp->tp, &args->exp);
            }
        }
        hr = hr->next;
        args = args->next;
    }
}
}  // namespace Parser