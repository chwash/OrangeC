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
 *     As a special exception, if other files instantiate templates or
 *     use macros or inline functions from this file, or you compile
 *     this file and link it with other works to produce a work based
 *     on this file, this file does not by itself cause the resulting
 *     work to be covered by the GNU General Public License. However
 *     the source code for this file must still be made available in
 *     accordance with section (3) of the GNU General Public License.
 *     
 *     This exception does not invalidate any other reasons why a work
 *     based on this file might be covered by the GNU General Public
 *     License.
 * 
 *     contact information:
 *         email: TouchStone222@runbox.com <David Lindauer>
 * 
 */

/*  timeb.h

    Struct and function declarations for ftime().

*/

#if !defined(__WAIT_H)
#    define __WAIT_H

#    ifndef __DEFS_H__
#        include <_defs.h>
#    endif

#    ifndef __TYPES_H
#        include <sys/types.h>
#    endif

#    ifdef __cplusplus
extern "C"
{
#    endif

#    define WNOHANG 1
#    define WUNTRACED 2

#    define WEXITSTATUS(x) ((x)&0xff)
#    define WIFEXITED(x) ((x) >= 0 && (x) <= 0xff)
#    define WIFSIGNALED(x) (0)
#    define WIFSTOPPED(x) (0)
#    define WSTOPSIG(x) (0)
#    define WIFCONTINUED(x) (0)

#ifndef RC_INVOKED
    pid_t _RTL_FUNC wait(int*);
    pid_t _RTL_FUNC waitpid(pid_t, int*, int);
#endif

#    ifdef __cplusplus
};
#    endif

#    pragma pack()
#endif /* __WAIT_H */
