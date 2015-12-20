#pragma once

#include "jvm.h"
#include <vector>

namespace jvm
{
	using namespace std;

	//---------- Basic class formats ----------//

	struct CFConstantPool
	{
		enum class Type
		{
			Utf8 = 1, // Format5
			Integer = 3, // Format3
			Float = 4, // Format3
			Long = 5, // Format4
			Double = 6, // Format4
			Class = 7, // Format1
			String = 8, // Format1
			Fieldref = 9, // Format2
			Methodref = 10, // Format2
			InterfaceMethodref = 11, // Format2
			NameAndType = 12, // Format2
			MethodHandle = 15, // Format6
			MethodType = 16,// Format1
			InvokeDynamic = 18 // Format2
		} type;
		union
		{
			struct Format1
			{
				u16 v;
			} f1;
			struct Format2
			{
				u16 v1, v2;
			} f2;
			struct Format3
			{
				u32 v;
			} f3;
			struct Format4
			{
				u64 v;
			} f4;
			struct Format5
			{
				u16 len;
				u32 idx; // index of VM string pool
			} f5;
			struct Format6
			{
				u8 kind;
				u16 idx;
			} f6;
		} val;
	};

	struct CFAttribute;

	struct CFField
	{
		u16 access_flags;
		u16 name_index;
		u16 descriptor_index;
		u16 attributes_count;
		vector<CFAttribute> attributes;
	};

	struct CFMethod
	{
		u16 access_flags;
		u16 name_index;
		u16 descriptor_index;
		u16 attributes_count;
		vector<CFAttribute> attributes;

		// Pre decode
		JSignature signature;
	};

	struct CFClassFile
	{
		u32 magic;
		u16 minor_version;
		u16 major_version;
		u16 constant_pool_count;
		vector<CFConstantPool> constant_pool;
		u16 access_flags;
		u16 this_class;
		u16 super_class;
		u16 interfaces_count;
		vector<u16> interfaces;
		u16 fields_count;
		vector<CFField> fields;
		u16 methods_count;
		vector<CFMethod> methods;
		u16 attributes_count;
		vector<CFAttribute> attributes;
	};

	//---------- Attributes ----------//

	struct CFAttribute
	{
		enum class Type
		{
			Undef,
			Unknown,
			ConstantValue,
			Code,
			LineNumberTable,
			LocalVariableTable,
			SourceFile,
			Exception
		} type = Type::Undef;

		u16 attribute_name_index;
		u32 attribute_length;
		union Value
		{
			struct Unknown
			{
				vector<u8> info;
			} unknown;

			struct ConstantValue
			{
				u16 constantvalue_index;
			} constantValue;

			struct Code
			{
				u16 max_stack;
				u16 max_locals;
				u32 code_length;
				vector<u8> code;
				u16 exception_table_lenth;
				struct Exception {
					u16 start_pc;
					u16 end_pc;
					u16 handler_pc;
					u16 catch_type;
				};
				vector<Exception> exception_table;
				u16 attributes_count;
				vector<CFAttribute> attributes;
			} code;

			struct LineNumberTable
			{
				u16 line_number_table_length;
				vector<pair<u16, u16>> line_number_table; // {start_pt, line_number}
			} lineNumberTable;

			struct LocalVariableTable
			{
				u16 local_variable_table_length;
				struct LocalVariable
				{
					u16 start_pc;
					u16 length;
					u16 name_index;
					u16 descriptor_index;
					u16 index; // slot
				};
				vector<LocalVariable> local_variable_table;
			} localVariableTable;

			struct SourceFile
			{
				u16 sourcefile_index;
			} sourceFile;

			struct Exception
			{
				u16 number_of_exceptions;
				vector<u16> exception_index_table;
			} exception;

			Value() {}
			~Value() {}
		} val;

		CFAttribute()
		{
		}
		~CFAttribute();
		CFAttribute(const CFAttribute& c)
		{
			*this = c;
		}
		CFAttribute(CFAttribute&& c);
		CFAttribute& operator=(const CFAttribute& c);
	};

	//---------- Functions ----------//

	bool loadClass(CFClassFile& cf, const char* path, VM& vm);
}
