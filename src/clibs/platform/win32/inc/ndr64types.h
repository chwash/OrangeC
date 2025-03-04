#ifndef _NDR64TYPES_H
#define _NDR64TYPES_H

#ifdef __ORANGEC__ 
#pragma once
#endif

/* NDR64 format string definitions */

#include <pshpack8.h>

#include <guiddef.h>


#define INVALID_FRAGMENT_ID  0

#if defined(_M_IX86) || defined(_M_IA64) || defined(_M_AMD64)
#define Ia64Axp(a,b)  (a)
#else
#error Invalid platform
#endif

#define NDR64_FC_EXPLICIT_HANDLE  0
#define NDR64_FC_BIND_GENERIC  1
#define NDR64_FC_BIND_PRIMITIVE  2
#define NDR64_FC_AUTO_HANDLE  3
#define NDR64_FC_CALLBACK_HANDLE  4
#define NDR64_FC_NO_HANDLE  5

#define NDR64_PTR_WIRE_ALIGN (sizeof(NDR64_PTR_WIRE_TYPE)-1)
#define NDR64_WIRE_COUNT_ALIGN (sizeof(NDR64_WIRE_COUNT_TYPE)-1)

typedef const void *FormatInfoRef;

typedef unsigned __int8 NDR64_UINT8;
typedef unsigned __int16 NDR64_UINT16;
typedef unsigned __int32 NDR64_UINT32;
typedef unsigned __int64 NDR64_UINT64;

typedef __int8 NDR64_INT8;
typedef __int16 NDR64_INT16;
typedef __int32 NDR64_INT32;
typedef __int64 NDR64_INT64;

typedef NDR64_UINT8 NDR64_FORMAT_CHAR;
typedef const void *PNDR64_FORMAT;
typedef NDR64_UINT8 NDR64_ALIGNMENT;
typedef NDR64_UINT32 NDR64_FORMAT_UINT32;

#if defined(__RPC_WIN32__)
typedef NDR64_INT32 NDR64_PTR_WIRE_TYPE;
#else
typedef NDR64_INT64 NDR64_PTR_WIRE_TYPE;
#endif /* __RPC_WIN32__ */

typedef NDR64_UINT64 NDR64_WIRE_COUNT_TYPE;

typedef struct _NDR64_PROC_FLAGS {
    NDR64_UINT32 HandleType:3;
    NDR64_UINT32 ProcType:3;
    NDR64_UINT32 IsInterpreted:2;
    NDR64_UINT32 IsObject:1;
    NDR64_UINT32 IsAsync:1;
    NDR64_UINT32 IsEncode:1;
    NDR64_UINT32 IsDecode:1;
    NDR64_UINT32 UsesFullPtrPackage:1;
    NDR64_UINT32 UsesRpcSmPackage:1;
    NDR64_UINT32 UsesPipes:1;
    NDR64_UINT32 HandlesExceptions:2;
    NDR64_UINT32 ServerMustSize:1;
    NDR64_UINT32 ClientMustSize:1;
    NDR64_UINT32 HasReturn:1;
    NDR64_UINT32 HasComplexReturn:1;
    NDR64_UINT32 ServerHasCorrelation:1;
    NDR64_UINT32 ClientHasCorrelation:1;
    NDR64_UINT32 HasNotify:1;
    NDR64_UINT32 HasOtherExtensions:1;
    NDR64_UINT32 Reserved:7;
} NDR64_PROC_FLAGS;

typedef struct _NDR64_RPC_FLAGS {
    NDR64_UINT16 Idempotent:1;
    NDR64_UINT16 Broadcast:1;
    NDR64_UINT16 Maybe:1;
    NDR64_UINT16 Reserved1:5;
    NDR64_UINT16 Message:1;
    NDR64_UINT16 Reserved2:4;
    NDR64_UINT16 InputSynchronous:1;
    NDR64_UINT16 Asynchronous:1;
    NDR64_UINT16 Reserved3:1;
} NDR64_RPC_FLAGS;

typedef struct _NDR64_PROC_FORMAT {
    NDR64_UINT32 Flags;
    NDR64_UINT32 StackSize;
    NDR64_UINT32 ConstantClientBufferSize;
    NDR64_UINT32 ConstantServerBufferSize;
    NDR64_UINT16 RpcFlags;
    NDR64_UINT16 FloatDoubleMask;
    NDR64_UINT16 NumberOfParams;
    NDR64_UINT16 ExtensionSize;
} NDR64_PROC_FORMAT, *PNDR64_PROC_FORMAT;

typedef struct _NDR64_PARAM_FLAGS {
    NDR64_UINT16 MustSize:1;
    NDR64_UINT16 MustFree:1;
    NDR64_UINT16 IsPipe:1;
    NDR64_UINT16 IsIn:1;
    NDR64_UINT16 IsOut:1;
    NDR64_UINT16 IsReturn:1;
    NDR64_UINT16 IsBasetype:1;
    NDR64_UINT16 IsByValue:1;
    NDR64_UINT16 IsSimpleRef:1;
    NDR64_UINT16 IsDontCallFreeInst:1;
    NDR64_UINT16 SaveForAsyncFinish:1;
    NDR64_UINT16 IsPartialIgnore:1;
    NDR64_UINT16 IsForceAllocate:1;
    NDR64_UINT16 Reserved:2;
    NDR64_UINT16 UseCache:1;
} NDR64_PARAM_FLAGS;

typedef struct _NDR64_PARAM_FORMAT {
    PNDR64_FORMAT Type;
    NDR64_PARAM_FLAGS Attributes;
    NDR64_UINT16 Reserved;
    NDR64_UINT32 StackOffset;
} NDR64_PARAM_FORMAT, *PNDR64_PARAM_FORMAT;

typedef struct _NDR64_RANGE_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_FORMAT_CHAR RangeType;
    NDR64_UINT16 Reserved;
    NDR64_INT64 MinValue;
    NDR64_INT64 MaxValue;
} NDR64_RANGE_FORMAT;

typedef struct _NDR64_CONTEXT_HANDLE_FLAGS {
    NDR64_UINT8 CannotBeNull:1;
    NDR64_UINT8 Serialize:1;
    NDR64_UINT8 NoSerialize:1;
    NDR64_UINT8 Strict:1;
    NDR64_UINT8 IsReturn:1;
    NDR64_UINT8 IsOut:1;
    NDR64_UINT8 IsIn:1;
    NDR64_UINT8 IsViaPointer:1;
} NDR64_CONTEXT_HANDLE_FLAGS;

typedef struct _NDR64_CONTEXT_HANDLE_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 ContextFlags;
    NDR64_UINT8 RundownRoutineIndex;
    NDR64_UINT8 Ordinal;
} NDR64_CONTEXT_HANDLE_FORMAT;

typedef struct _NDR64_BIND_PRIMITIVE {
    NDR64_FORMAT_CHAR HandleType;
    NDR64_UINT8 Flags;
    NDR64_UINT16 StackOffset;
    NDR64_UINT16 Reserved;
} NDR64_BIND_PRIMITIVE;

typedef struct _NDR64_BIND_GENERIC {
    NDR64_FORMAT_CHAR HandleType;
    NDR64_UINT8 Flags;
    NDR64_UINT16 StackOffset;
    NDR64_UINT8 RoutineIndex;
    NDR64_UINT8 Size;
} NDR64_BIND_GENERIC;

typedef struct _NDR64_BIND_CONTEXT {
    NDR64_FORMAT_CHAR HandleType;
    NDR64_UINT8 Flags;
    NDR64_UINT16 StackOffset;
    NDR64_UINT8 RoutineIndex;
    NDR64_UINT8 Ordinal;
} NDR64_BIND_CONTEXT;

typedef union _NDR64_BINDINGS {
    NDR64_BIND_PRIMITIVE Primitive;
    NDR64_BIND_GENERIC Generic;
    NDR64_BIND_CONTEXT Context;
} NDR64_BINDINGS;

typedef struct _NDR64_BIND_AND_NOTIFY_EXTENSION {
    NDR64_BIND_CONTEXT Binding;
    NDR64_UINT16 NotifyIndex;
} NDR64_BIND_AND_NOTIFY_EXTENSION;

typedef struct _NDR64_POINTER_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Flags;
    NDR64_UINT16 Reserved;
    PNDR64_FORMAT Pointee;
} NDR64_POINTER_FORMAT;

typedef struct _NDR64_NO_REPEAT_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Flags;
    NDR64_UINT16 Reserved1;
    NDR64_UINT32 Reserved2;
} NDR64_NO_REPEAT_FORMAT;

typedef struct _NDR64_POINTER_INSTANCE_HEADER_FORMAT {
    NDR64_UINT32 Offset;
    NDR64_UINT32 Reserved;
} NDR64_POINTER_INSTANCE_HEADER_FORMAT;

typedef struct _NDR64_POINTER_REPEAT_FLAGS {
    NDR64_UINT8 SetCorrMark:1;
    NDR64_UINT8 Reserved:7;
} NDR64_POINTER_REPEAT_FLAGS, *PNDR64_POINTER_REPEAT_FLAGS;

typedef struct _NDR64_REPEAT_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_POINTER_REPEAT_FLAGS Flags;
    NDR64_UINT16 Reserved;
    NDR64_UINT32 Increment;
    NDR64_UINT32 OffsetToArray;
    NDR64_UINT32 NumberOfPointers;
} NDR64_REPEAT_FORMAT, *PNDR64_REPEAT_FORMAT;

typedef struct _NDR64_FIXED_REPEAT_FORMAT {
    NDR64_REPEAT_FORMAT RepeatFormat;
    NDR64_UINT32 Iterations;
    NDR64_UINT32 Reserved;
} NDR64_FIXED_REPEAT_FORMAT, *PNDR64_FIXED_REPEAT_FORMAT;

typedef struct _NDR64_IID_FLAGS {
    NDR64_UINT8 ConstantIID:1;
    NDR64_UINT8 Reserved:7;
} NDR64_IID_FLAGS;

typedef struct _NDR64_CONSTANT_IID_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Flags;
    NDR64_UINT16 Reserved;
    GUID Guid;
} NDR64_CONSTANT_IID_FORMAT;

typedef struct _NDR64_IID_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Flags;
    NDR64_UINT16 Reserved;
    PNDR64_FORMAT IIDDescriptor;
} NDR64_IID_FORMAT;

typedef struct _NDR64_STRUCTURE_FLAGS {
    NDR64_UINT8 HasPointerInfo:1;
    NDR64_UINT8 HasMemberInfo:1;
    NDR64_UINT8 HasConfArray:1;
    NDR64_UINT8 HasOrigPointerInfo:1;
    NDR64_UINT8 HasOrigMemberInfo:1;
    NDR64_UINT8 Reserved1:1;
    NDR64_UINT8 Reserved2:1;
    NDR64_UINT8 Reserved3:1;
} NDR64_STRUCTURE_FLAGS;

typedef struct _NDR64_STRUCTURE_HEADER_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_ALIGNMENT Alignment;
    NDR64_STRUCTURE_FLAGS Flags;
    NDR64_UINT8 Reserve;
    NDR64_UINT32 MemorySize;
} NDR64_STRUCTURE_HEADER_FORMAT;

typedef struct _NDR64_CONF_STRUCTURE_HEADER_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_ALIGNMENT Alignment;
    NDR64_STRUCTURE_FLAGS Flags;
    NDR64_UINT8 Reserve;
    NDR64_UINT32 MemorySize;
    PNDR64_FORMAT ArrayDescription;
} NDR64_CONF_STRUCTURE_HEADER_FORMAT;

typedef struct _NDR64_BOGUS_STRUCTURE_HEADER_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_ALIGNMENT Alignment;
    NDR64_STRUCTURE_FLAGS Flags;
    NDR64_UINT8 Reserve;
    NDR64_UINT32 MemorySize;
    PNDR64_FORMAT OriginalMemberLayout;
    PNDR64_FORMAT OriginalPointerLayout;
    PNDR64_FORMAT PointerLayout;
} NDR64_BOGUS_STRUCTURE_HEADER_FORMAT;

typedef struct _NDR64_CONF_BOGUS_STRUCTURE_HEADER_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_ALIGNMENT Alignment;
    NDR64_STRUCTURE_FLAGS Flags;
    NDR64_UINT8 Dimensions;
    NDR64_UINT32 MemorySize;
    PNDR64_FORMAT OriginalMemberLayout;
    PNDR64_FORMAT OriginalPointerLayout;
    PNDR64_FORMAT PointerLayout;
    PNDR64_FORMAT ConfArrayDescription;
} NDR64_CONF_BOGUS_STRUCTURE_HEADER_FORMAT;

typedef struct _NDR64_SIMPLE_MEMBER_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Reserved1;
    NDR64_UINT16 Reserved2;
    NDR64_UINT32 Reserved3;
} NDR64_SIMPLE_MEMBER_FORMAT;

typedef struct _NDR64_MEMPAD_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Reserve1;
    NDR64_UINT16 MemPad;
    NDR64_UINT32 Reserved2;
} NDR64_MEMPAD_FORMAT;

typedef struct _NDR64_EMBEDDED_COMPLEX_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Reserve1;
    NDR64_UINT16 Reserve2;
    PNDR64_FORMAT Type;
} NDR64_EMBEDDED_COMPLEX_FORMAT;

typedef struct _NDR64_BUFFER_ALIGN_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_ALIGNMENT Alignment;
    NDR64_UINT16 Reserved;
    NDR64_UINT32 Reserved2;
} NDR64_BUFFER_ALIGN_FORMAT;

typedef struct _NDR64_SIMPLE_REGION_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_ALIGNMENT Alignment;
    NDR64_UINT16 RegionSize;
    NDR64_UINT32 Reserved;
} NDR64_SIMPLE_REGION_FORMAT;

typedef struct _NDR64_ENCAPSULATED_UNION {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Alignment;
    NDR64_UINT8 Flags;
    NDR64_FORMAT_CHAR SwitchType;
    NDR64_UINT32 MemoryOffset;
    NDR64_UINT32 MemorySize;
    NDR64_UINT32 Reserved;
} NDR64_ENCAPSULATED_UNION;

typedef struct _NDR64_NON_ENCAPSULATED_UNION {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Alignment;
    NDR64_UINT8 Flags;
    NDR64_FORMAT_CHAR SwitchType;
    NDR64_UINT32 MemorySize;
    PNDR64_FORMAT Switch;
    NDR64_UINT32 Reserved;
} NDR64_NON_ENCAPSULATED_UNION;

typedef struct _NDR64_UNION_ARM_SELECTOR {
    NDR64_UINT8 Reserved1;
    NDR64_UINT8 Alignment;
    NDR64_UINT16 Reserved2;
    NDR64_UINT32 Arms;
} NDR64_UNION_ARM_SELECTOR;

typedef struct _NDR64_UNION_ARM {
    NDR64_INT64 CaseValue;
    PNDR64_FORMAT Type;
    NDR64_UINT32 Reserved;
} NDR64_UNION_ARM;

typedef struct _NDR64_ARRAY_FLAGS {
    NDR64_UINT8 HasPointerInfo:1;
    NDR64_UINT8 HasElementInfo:1;
    NDR64_UINT8 IsMultiDimensional:1;
    NDR64_UINT8 IsArrayofStrings:1;
    NDR64_UINT8 Reserved1:1;
    NDR64_UINT8 Reserved2:1;
    NDR64_UINT8 Reserved3:1;
    NDR64_UINT8 Reserved4:1;
} NDR64_ARRAY_FLAGS;

typedef struct _NDR64_ARRAY_ELEMENT_INFO {
    NDR64_UINT32 ElementMemSize;
    PNDR64_FORMAT Element;
} NDR64_ARRAY_ELEMENT_INFO;

typedef struct _NDR64_FIX_ARRAY_HEADER_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_ALIGNMENT Alignment;
    NDR64_ARRAY_FLAGS Flags;
    NDR64_UINT8 Reserved;
    NDR64_UINT32 TotalSize;
} NDR64_FIX_ARRAY_HEADER_FORMAT;

typedef struct _NDR64_CONF_ARRAY_HEADER_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_ALIGNMENT Alignment;
    NDR64_ARRAY_FLAGS Flags;
    NDR64_UINT8 Reserved;
    NDR64_UINT32 ElementSize;
    PNDR64_FORMAT ConfDescriptor;
} NDR64_CONF_ARRAY_HEADER_FORMAT;

typedef struct _NDR64_CONF_VAR_ARRAY_HEADER_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_ALIGNMENT Alignment;
    NDR64_ARRAY_FLAGS Flags;
    NDR64_UINT8 Reserved;
    NDR64_UINT32 ElementSize;
    PNDR64_FORMAT ConfDescriptor;
    PNDR64_FORMAT VarDescriptor;
} NDR64_CONF_VAR_ARRAY_HEADER_FORMAT;

typedef struct _NDR64_VAR_ARRAY_HEADER_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_ALIGNMENT Alignment;
    NDR64_ARRAY_FLAGS Flags;
    NDR64_UINT8 Reserved;
    NDR64_UINT32 TotalSize;
    NDR64_UINT32 ElementSize;
    PNDR64_FORMAT VarDescriptor;
} NDR64_VAR_ARRAY_HEADER_FORMAT;

typedef struct _NDR64_BOGUS_ARRAY_HEADER_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_ALIGNMENT Alignment;
    NDR64_ARRAY_FLAGS Flags;
    NDR64_UINT8 NumberDims;
    NDR64_UINT32 NumberElements;
    PNDR64_FORMAT Element;
} NDR64_BOGUS_ARRAY_HEADER_FORMAT;

typedef struct _NDR64_CONF_VAR_BOGUS_ARRAY_HEADER_FORMAT {
    NDR64_BOGUS_ARRAY_HEADER_FORMAT FixedArrayFormat;
    PNDR64_FORMAT ConfDescription;
    PNDR64_FORMAT VarDescription;
    PNDR64_FORMAT OffsetDescription;
} NDR64_CONF_VAR_BOGUS_ARRAY_HEADER_FORMAT;

typedef struct _NDR64_STRING_FLAGS {
    NDR64_UINT8 IsSized:1;
    NDR64_UINT8 Reserved2:1;
    NDR64_UINT8 Reserved3:1;
    NDR64_UINT8 Reserved4:1;
    NDR64_UINT8 Reserved5:1;
    NDR64_UINT8 Reserved6:1;
    NDR64_UINT8 Reserved7:1;
    NDR64_UINT8 Reserved8:1;
} NDR64_STRING_FLAGS;

typedef struct NDR64_STRING_HEADER_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_STRING_FLAGS Flags;
    NDR64_UINT16 ElementSize;
} NDR64_STRING_HEADER_FORMAT;

typedef struct _NDR64_NON_CONFORMANT_STRING_FORMAT {
    NDR64_STRING_HEADER_FORMAT Header;
    NDR64_UINT32 TotalSize;
} NDR64_NON_CONFORMANT_STRING_FORMAT;

typedef struct _NDR64_CONFORMANT_STRING_FORMAT {
    NDR64_STRING_HEADER_FORMAT Header;
} NDR64_CONFORMANT_STRING_FORMAT;

typedef struct NDR64_SIZED_CONFORMANT_STRING_FORMAT {
    NDR64_STRING_HEADER_FORMAT Header;
    PNDR64_FORMAT SizeDescription;
} NDR64_SIZED_CONFORMANT_STRING_FORMAT;

typedef enum _tagEXPR_TOKEN {
    FC_EXPR_START = 0,
    FC_EXPR_ILLEGAL = FC_EXPR_START,
    FC_EXPR_CONST32,
    FC_EXPR_CONST64,
    FC_EXPR_VAR,
    FC_EXPR_OPER,
    FC_EXPR_NOOP,
    FC_EXPR_END
} EXPR_TOKEN;

typedef struct _NDR64_EXPR_OPERATOR {
    NDR64_FORMAT_CHAR ExprType;
    NDR64_FORMAT_CHAR Operator;
    NDR64_FORMAT_CHAR CastType;
    NDR64_UINT8 Reserved;
} NDR64_EXPR_OPERATOR;

typedef struct _NDR64_EXPR_CONST32 {
    NDR64_FORMAT_CHAR ExprType;
    NDR64_FORMAT_CHAR Reserved;
    NDR64_UINT16 Reserved1;
    NDR64_UINT32 ConstValue;
} NDR64_EXPR_CONST32;

typedef struct _NDR64_EXPR_CONST64 {
    NDR64_FORMAT_CHAR ExprType;
    NDR64_FORMAT_CHAR Reserved;
    NDR64_UINT16 Reserved1;
    NDR64_INT64 ConstValue;
} NDR64_EXPR_CONST64;

typedef struct _NDR64_EXPR_VAR {
    NDR64_FORMAT_CHAR ExprType;
    NDR64_FORMAT_CHAR VarType;
    NDR64_UINT16 Reserved;
    NDR64_UINT32 Offset;
} NDR64_EXPR_VAR;

typedef struct _NDR64_EXPR_NOOP {
    NDR64_FORMAT_CHAR ExprType;
    NDR64_UINT8 Size;
    NDR64_UINT16 Reserved;
} NDR64_EXPR_NOOP;

typedef struct _NDR64_TRANSMIT_AS_FLAGS {
    NDR64_UINT8 PresentedTypeIsArray:1;
    NDR64_UINT8 PresentedTypeAlign4:1;
    NDR64_UINT8 PresentedTypeAlign8:1;
    NDR64_UINT8 Reserved:5;
} NDR64_TRANSMIT_AS_FLAGS;

typedef struct _NDR64_TRANSMIT_AS_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Flags;
    NDR64_UINT16 RoutineIndex;
    NDR64_UINT16 TransmittedTypeWireAlignment;
    NDR64_UINT16 MemoryAlignment;
    NDR64_UINT32 PresentedTypeMemorySize;
    NDR64_UINT32 TransmittedTypeBufferSize;
    PNDR64_FORMAT TransmittedType;
} NDR64_TRANSMIT_AS_FORMAT;

typedef NDR64_TRANSMIT_AS_FORMAT NDR64_REPRESENT_AS_FORMAT;

typedef struct _NDR64_USER_MARSHAL_FLAGS {
    NDR64_UINT8 Reserved:5;
    NDR64_UINT8 IID:1;
    NDR64_UINT8 RefPointer:1;
    NDR64_UINT8 UniquePointer:1;
} NDR64_USER_MARSHAL_FLAGS;

typedef struct _NDR64_USER_MARSHAL_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Flags;
    NDR64_UINT16 RoutineIndex;
    NDR64_UINT16 TransmittedTypeWireAlignment;
    NDR64_UINT16 MemoryAlignment;
    NDR64_UINT32 UserTypeMemorySize;
    NDR64_UINT32 TransmittedTypeBufferSize;
    PNDR64_FORMAT TransmittedType;
} NDR64_USER_MARSHAL_FORMAT;

typedef struct NDR64_PIPE_FLAGS {
    NDR64_UINT8 Reserved1:5;
    NDR64_UINT8 HasRange:1;
    NDR64_UINT8 BlockCopy:1;
    NDR64_UINT8 Reserved2:1;
} NDR64_PIPE_FLAGS;

typedef struct _NDR64_PIPE_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Flags;
    NDR64_UINT8 Alignment;
    NDR64_UINT8 Reserved;
    PNDR64_FORMAT Type;
    NDR64_UINT32 MemorySize;
    NDR64_UINT32 BufferSize;
} NDR64_PIPE_FORMAT;

typedef struct _NDR64_RANGE_PIPE_FORMAT {
    NDR64_FORMAT_CHAR FormatCode;
    NDR64_UINT8 Flags;
    NDR64_UINT8 Alignment;
    NDR64_UINT8 Reserved;
    PNDR64_FORMAT Type;
    NDR64_UINT32 MemorySize;
    NDR64_UINT32 BufferSize;
    NDR64_UINT32 MinValue;
    NDR64_UINT32 MaxValue;
} NDR64_RANGE_PIPE_FORMAT;


#include <poppack.h>

#endif /* _NDR64TYPES_H */
