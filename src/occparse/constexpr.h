#pragma once
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

namespace Parser
{
void constexprinit();
void constexprfunctioninit(bool start);
void ConstExprPatch(EXPRESSION** node);
void ConstExprPromote(EXPRESSION* node, bool isconst);
void ConstExprStructElemEval(EXPRESSION** node);
bool checkconstexprfunc(EXPRESSION* node);
bool IsConstantExpression(EXPRESSION* node, bool allowParams, bool allowFunc, bool fromFunc = false);
bool EvaluateConstexprFunction(EXPRESSION*& node);
}  // namespace Parser