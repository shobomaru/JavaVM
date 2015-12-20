#include "jvmClass.h"
#include <fstream>
#include <iostream>
#include <cassert>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
//#include <codecvt>
//#include <sstream>
//#include <locale>

using namespace std;
using namespace jvm;

namespace
{
	wstring utf8toucs(const char* str, int len)
	{
#if 0
		// FIXME: Cannot handle non-shortest null character.
		wstringstream input;
		input.imbue(locale(input.getloc(), new codecvt_utf8_utf16<wchar_t>()));
		input << str;
		return input.str();
#else
		int ulen = ::MultiByteToWideChar(CP_UTF8, 0, str, len, 0, 0);
		if (ulen == 0)
			return wstring();

		wstring ucs(ulen, L'\0');
		::MultiByteToWideChar(CP_UTF8, 0, str, len, (wchar_t*)ucs.data(), ulen);
		return ucs;
#endif
	}

	void readAttribute(CFAttribute& ai, ifstream& ifs, const vector<CFConstantPool>& constPool, VM& vm)
	{
		ifs.read((char*)&ai.attribute_name_index, 2);
		revbits(ai.attribute_name_index);

		ifs.read((char*)&ai.attribute_length, 4);
		revbits(ai.attribute_length);

		auto& nameRef = constPool[ai.attribute_name_index].val.f5.idx;
		auto& name = vm.GetInternedString(nameRef);
		if (name == L"Code")
		{
			ai.type = CFAttribute::Type::Code;
			new (&ai.val.code) CFAttribute::Value::Code;
			auto& cd = ai.val.code;

			ifs.read((char*)&cd.max_stack, 2);
			revbits(cd.max_stack);

			ifs.read((char*)&cd.max_locals, 2);
			revbits(cd.max_locals);

			ifs.read((char*)&cd.code_length, 4);
			revbits(cd.code_length);

			cd.code.resize(cd.code_length);
			ifs.read((char*)&cd.code[0], cd.code_length);

			ifs.read((char*)&cd.exception_table_lenth, 2);
			revbits(cd.exception_table_lenth);

			cd.exception_table.resize(cd.exception_table_lenth);
			for (auto& e : cd.exception_table)
			{
				ifs.read((char*)&e.start_pc, 2);
				revbits(e.start_pc);

				ifs.read((char*)&e.end_pc, 2);
				revbits(e.end_pc);

				ifs.read((char*)&e.handler_pc, 2);
				revbits(e.handler_pc);

				ifs.read((char*)&e.catch_type, 2);
				revbits(e.catch_type);
			}

			ifs.read((char*)&cd.attributes_count, 2);
			revbits(cd.attributes_count);

			cd.attributes.resize(cd.attributes_count);
			for (auto& at : cd.attributes)
			{
				readAttribute(at, ifs, constPool, vm);
			}
		}
		else if (name == L"LineNumberTable")
		{
			ai.type = CFAttribute::Type::LineNumberTable;
			new (&ai.val.lineNumberTable) CFAttribute::Value::LineNumberTable;
			auto& ln = ai.val.lineNumberTable;

			ifs.read((char*)&ln.line_number_table_length, 2);
			revbits(ln.line_number_table_length);

			ln.line_number_table.resize(ln.line_number_table_length);
			for (auto& e : ln.line_number_table)
			{
				u16 start_pc;
				u16 line_number;
				ifs.read((char*)&start_pc, 2);
				ifs.read((char*)&line_number, 2);
				revbits(start_pc);
				revbits(line_number);
				e = make_pair(start_pc, line_number);
			}
		}
		else if (name == L"LocalVariableTable")
		{
			ai.type = CFAttribute::Type::LocalVariableTable;
			new (&ai.val.localVariableTable) CFAttribute::Value::LocalVariableTable;
			auto& lv = ai.val.localVariableTable;

			ifs.read((char*)&lv.local_variable_table_length, 2);
			revbits(lv.local_variable_table_length);

			lv.local_variable_table.resize(lv.local_variable_table_length);
			for (auto& v : lv.local_variable_table)
			{
				ifs.read((char*)&v.start_pc, 2);
				ifs.read((char*)&v.length, 2);
				ifs.read((char*)&v.name_index, 2);
				ifs.read((char*)&v.descriptor_index, 2);
				ifs.read((char*)&v.index, 2);
				revbits(v.start_pc);
				revbits(v.length);
				revbits(v.name_index);
				revbits(v.descriptor_index);
				revbits(v.index);
			}
		}
		else if (name == L"SourceFile")
		{
			ai.type = CFAttribute::Type::SourceFile;
			new (&ai.val.sourceFile) CFAttribute::Value::SourceFile;

			ifs.read((char*)&ai.val.sourceFile.sourcefile_index, 2);
			revbits(ai.val.sourceFile.sourcefile_index);
		}
		else
		{
			ai.type = CFAttribute::Type::Unknown;
			new (&ai.val.unknown) CFAttribute::Value::Unknown;

			ai.val.unknown.info.resize(ai.attribute_length);
			ifs.read((char*)&ai.val.unknown.info[0], ai.attribute_length);
		}
	};
}

bool jvm::loadClass(CFClassFile& cf, const char* path, VM& vm)
{
	cf.magic = 0;

	ifstream ifs(path, ios::binary);
	if (!ifs)
	{
		cout << "Failed to open class file : " << path << endl;
		return false;
	}

	ifs.read((char*)&cf.magic, 4);
	revbits(cf.magic);
	if (cf.magic != 0xCAFEBABE)
	{
		cout << "Not class file" << endl;
		return false;
	}

	ifs.read((char*)&cf.minor_version, 2);
	revbits(cf.minor_version);
	cout << "Minor version : " << cf.minor_version << endl;

	ifs.read((char*)&cf.major_version, 2);
	revbits(cf.major_version);
	cout << "Major version : " << cf.major_version << endl;

	ifs.read((char*)&cf.constant_pool_count, 2);
	revbits(cf.constant_pool_count);
	cout << "Constant pool count : " << cf.constant_pool_count << endl;

	cf.constant_pool.resize(cf.constant_pool_count);
	cf.constant_pool[0] = CFConstantPool();
	memset(&cf.constant_pool[0], 0, sizeof cf.constant_pool[0]);

	for (int i = 1; i < cf.constant_pool_count; i++)
	{
		CFConstantPool cp;
		memset(&cp, 0, sizeof cp);
		char temp[3 * 65537];

		ifs.read((char*)&cp.type, 1);
		switch (cp.type)
		{
		case CFConstantPool::Type::Class:
		case CFConstantPool::Type::String:
		case CFConstantPool::Type::InvokeDynamic:
			ifs.read((char*)&cp.val.f1.v, 2);
			revbits(cp.val.f1.v);
			break;
		case CFConstantPool::Type::Fieldref:
		case CFConstantPool::Type::Methodref:
		case CFConstantPool::Type::InterfaceMethodref:
		case CFConstantPool::Type::NameAndType:
		case CFConstantPool::Type::MethodType:
			ifs.read((char*)&cp.val.f2.v1, 2);
			ifs.read((char*)&cp.val.f2.v2, 2);
			revbits(cp.val.f2.v1);
			revbits(cp.val.f2.v2);
			break;
		case CFConstantPool::Type::Integer:
		case CFConstantPool::Type::Float:
			ifs.read((char*)&cp.val.f3.v, 4);
			revbits(cp.val.f3.v);
			break;
		case CFConstantPool::Type::Long:
		case CFConstantPool::Type::Double:
			ifs.read((char*)&cp.val.f4.v, 8);
			revbits(cp.val.f4.v);
			break;
		case CFConstantPool::Type::Utf8:
			ifs.read((char*)&cp.val.f5.len, 2);
			revbits(cp.val.f5.len);
			ifs.read(temp, cp.val.f5.len);
			cp.val.f5.idx = vm.InternString(utf8toucs(temp, cp.val.f5.len));
			break;
		case CFConstantPool::Type::MethodHandle:
			ifs.read((char*)&cp.val.f6.kind, 1);
			ifs.read((char*)&cp.val.f6.idx, 2);
			revbits(cp.val.f6.idx);
			break;
		default:
			cout << "Unknown constant pool tag" << endl;
			return false;
		}
		cf.constant_pool[i] = cp;
	}

	ifs.read((char*)&cf.access_flags, 2);
	revbits(cf.access_flags);
	cout << "Access flags : " << cf.access_flags << endl;

	ifs.read((char*)&cf.this_class, 2);
	revbits(cf.this_class);
	cout << "This class : " << cf.this_class << endl;

	ifs.read((char*)&cf.super_class, 2);
	revbits(cf.super_class);
	cout << "Super class : " << cf.super_class << endl;

	ifs.read((char*)&cf.interfaces_count, 2);
	revbits(cf.interfaces_count);
	cout << "Interfaces count : " << cf.interfaces_count << endl;

	if (cf.interfaces_count > 0)
	{
		cf.interfaces.resize(cf.interfaces_count);
		ifs.read((char*)&cf.interfaces[0], 2 * cf.interfaces_count);
		for (auto& n : cf.interfaces)
			revbits(n);
	}

	ifs.read((char*)&cf.fields_count, 2);
	revbits(cf.fields_count);
	cout << "Fields count : " << cf.fields_count << endl;

	cf.methods.reserve(cf.fields_count);
	for (int m = 0; m < cf.fields_count; m++)
	{
		CFField field;

		ifs.read((char*)&field.access_flags, 2);
		revbits(field.access_flags);

		ifs.read((char*)&field.name_index, 2);
		revbits(field.name_index);

		ifs.read((char*)&field.descriptor_index, 2);
		revbits(field.descriptor_index);

		ifs.read((char*)&field.attributes_count, 2);
		revbits(field.attributes_count);

		field.attributes.resize(field.attributes_count);
		for (int i = 0; i < field.attributes_count; i++)
		{
			readAttribute(field.attributes[i], ifs, cf.constant_pool, vm);
		}
		cf.fields.emplace_back(move(field));
	}

	ifs.read((char*)&cf.methods_count, 2);
	revbits(cf.methods_count);
	cout << "Methods count : " << cf.methods_count << endl;

	cf.methods.reserve(cf.methods_count);
	for (int m = 0; m < cf.methods_count; m++)
	{
		CFMethod method;

		ifs.read((char*)&method.access_flags, 2);
		revbits(method.access_flags);

		ifs.read((char*)&method.name_index, 2);
		revbits(method.name_index);

		ifs.read((char*)&method.descriptor_index, 2);
		revbits(method.descriptor_index);

		ifs.read((char*)&method.attributes_count, 2);
		revbits(method.attributes_count);

		method.attributes.resize(method.attributes_count);
		for (int i = 0; i < method.attributes_count; i++)
		{
			readAttribute(method.attributes[i], ifs, cf.constant_pool, vm);
		}

		// Pre decode
		auto& str = vm.GetInternedString(cf.constant_pool[method.descriptor_index].val.f5.idx);
		method.signature = DecodeSignature(vm, str);

		cf.methods.emplace_back(move(method));
	}

	ifs.read((char*)&cf.attributes_count, 2);
	revbits(cf.attributes_count);
	cout << "Attributes count : " << cf.attributes_count << endl;

	cf.attributes.resize(cf.attributes_count);
	for (int i = 0; i < cf.attributes_count; i++)
	{
		readAttribute(cf.attributes[i], ifs, cf.constant_pool, vm);
	}

	if (ifs.bad() || ifs.eof())
	{
		cout << "Invalid class file format" << endl;
		return false;
	}

	cout << "Class file loaded" << endl;
	return true;
}

jvm::CFAttribute::~CFAttribute()
{
	switch (type)
	{
	case Type::Undef:
		break;
	case Type::Unknown:
		val.unknown.~Unknown();
		break;
	case Type::ConstantValue:
		val.constantValue.~ConstantValue();
		break;
	case Type::Code:
		val.code.~Code();
		break;
	case Type::LineNumberTable:
		val.lineNumberTable.~LineNumberTable();
		break;
	case Type::LocalVariableTable:
		val.localVariableTable.~LocalVariableTable();
		break;
	case Type::SourceFile:
		val.sourceFile.~SourceFile();
		break;
	case Type::Exception:
		val.exception.~Exception();
		break;
	default:
		assert(0);
	}
}

jvm::CFAttribute::CFAttribute(CFAttribute&& c)
{
	type = c.type;
	attribute_name_index = c.attribute_name_index;
	attribute_length = c.attribute_length;

	switch (type)
	{
	case Type::Undef:
		break;
	case Type::Unknown:
		new (&val.unknown) Value::Unknown;
		val.unknown = move(c.val.unknown);
		break;
	case Type::ConstantValue:
		new (&val.constantValue) Value::ConstantValue;
		val.constantValue = move(c.val.constantValue);
		break;
	case Type::Code:
		new (&val.code) Value::Code;
		val.code = move(c.val.code);
		break;
	case Type::LineNumberTable:
		new (&val.lineNumberTable) Value::LineNumberTable;
		val.lineNumberTable = c.val.lineNumberTable;
		break;
	case Type::LocalVariableTable:
		new (&val.localVariableTable) Value::LocalVariableTable;
		val.localVariableTable = c.val.localVariableTable;
		break;
	case Type::SourceFile:
		new (&val.sourceFile) Value::SourceFile;
		val.sourceFile = c.val.sourceFile;
		break;
	case Type::Exception:
		new (&val.exception) Value::Exception;
		val.exception = move(c.val.exception);
		break;
	default:
		assert(0);
	}
}

CFAttribute& jvm::CFAttribute::operator=(const CFAttribute& c)
{
	type = c.type;
	attribute_name_index = c.attribute_name_index;
	attribute_length = c.attribute_length;

	switch (type)
	{
	case Type::Undef:
		break;
	case Type::Unknown:
		new (&val.unknown) Value::Unknown;
		val.unknown = c.val.unknown;
		break;
	case Type::ConstantValue:
		new (&val.constantValue) Value::ConstantValue;
		val.constantValue = c.val.constantValue;
		break;
	case Type::Code:
		new (&val.code) Value::Code;
		val.code = c.val.code;
		break;
	case Type::LineNumberTable:
		new (&val.lineNumberTable) Value::LineNumberTable;
		val.lineNumberTable = c.val.lineNumberTable;
		break;
	case Type::LocalVariableTable:
		new (&val.localVariableTable) Value::LocalVariableTable;
		val.localVariableTable = c.val.localVariableTable;
		break;
	case Type::SourceFile:
		new (&val.sourceFile) Value::SourceFile;
		val.sourceFile = c.val.sourceFile;
		break;
	case Type::Exception:
		new (&val.exception) Value::Exception;
		val.exception = c.val.exception;
		break;
	default:
		assert(0);
	}

	return *this;
}
