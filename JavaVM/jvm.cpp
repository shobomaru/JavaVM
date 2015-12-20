#include "jvm.h"
#include "jvmClass.h"
#include "jvmExec.h"
#include <iostream>
#include <cassert>

using namespace std;
using namespace jvm;

VM::VM()
{
	m_stackFrame.reserve(4096);
}

VM::~VM()
{
}

void VM::Load(const char* path)
{
	{
		CFClassFile cf;
		bool r = loadClass(cf, path, *this);
		if (!r)
			return;
		m_classFilePool.emplace_back(move(cf));
	}

	// staticなフィールドの構築
	JClass jc = { m_classFilePool.back() };
	jc.staticFields.reserve(jc.cf.fields_count);
	for (int i = 0; i < jc.cf.fields_count; i++)
	{
		auto& field = jc.cf.fields[i];
		auto& fldNameRef = jc.cf.constant_pool[field.name_index].val.f5.idx;
		auto& fldName = m_stringPool[fldNameRef];
		auto& typeNameRef = jc.cf.constant_pool[field.descriptor_index].val.f5.idx;
		auto& typeName = m_stringPool[typeNameRef];

		JType jt = DecodeType(*this, typeName);
		JValue val = {};

		JMember mem = { fldName, jt, val };
		jc.staticFields.emplace_back(move(mem));
	}

	// Static initializer
	for (auto& method : jc.cf.methods)
	{
		auto& metNameRef = jc.cf.constant_pool[method.name_index].val.f5.idx;
		auto& metName = m_stringPool[metNameRef];
		auto& sigNameRef = jc.cf.constant_pool[method.descriptor_index].val.f5.idx;
		auto& sigName = m_stringPool[sigNameRef];
		if (metName == L"<clinit>" && sigName == L"()V")
		{
			Invoke(jc, method, true);
			m_stackFrame.clear(); // HACK: string[]がpushされてしまっている
			break;
		}
	}

	m_classPool.emplace_back(move(jc));
}

JObject& VM::NewPrimitiveArray(PrimitiveType type, s32 numElem)
{
	size_t sz = 0;

	switch (type)
	{
	case PrimitiveType::Int:
		sz = 4 * numElem;
		break;
	default:
		assert(0); // 未実装
	}

	m_instanceTable.emplace_back(JObject{0, type, numElem, make_unique<u8[]>(sz)});
	return m_instanceTable.back();
}

u32 VM::InternString(std::wstring&& str)
{
	for (u32 i = 0; i < m_stringPool.size(); ++i)
	{
		if (m_stringPool[i] == str)
			return i;
	}
	m_stringPool.emplace_back(str);
	return m_stringPool.size() - 1;
}

void VM::Invoke(const wstring& clazz, const wstring& method, const wstring& signature)
{
	for (auto& jc : m_classPool)
	{
		CFClassFile& cls = jc.cf;
		auto& thisClass = cls.constant_pool[cls.this_class];
		auto& classNameRef = cls.constant_pool[thisClass.val.f1.v].val.f5.idx;
		auto& className = m_stringPool[classNameRef];
		if (className == clazz)
		{
			for (auto& met : cls.methods)
			{
				auto& metNameRef = cls.constant_pool[met.name_index].val.f5.idx;
				auto& metName = m_stringPool[metNameRef];
				auto& sigNameRef = cls.constant_pool[met.descriptor_index].val.f5.idx;
				auto& sigName = m_stringPool[sigNameRef];
				if (metName == method && sigName == signature)
				{
					Invoke(jc, met, false);
					return;
				}
			}
		}
	}
	cout << "Method not found" << endl;
}

void VM::Invoke(JClass& jclass, const CFMethod& method, bool allowNonPublic) noexcept
{
	const CFClassFile& classFile = jclass.cf;

	if ((!allowNonPublic) && (~method.access_flags & 0x0001)) // ACC_PUBLIC
	{
		cout << "Invoke method must be public" << endl;
		return;
	}
	if (~method.access_flags & 0x0008) // ACC_STATIC
	{
		cout << "Invoke method must be static" << endl;
		return;
	}
	if (method.access_flags & 0x0400) // ACC_ABSTRACT
	{
		cout << "Invoke method must not be abstract" << endl;
		return;
	}

	bool methodFound = false;
	for (auto& attr : method.attributes)
	{
		auto& nameRef = classFile.constant_pool[attr.attribute_name_index].val.f5.idx;
		auto& name = m_stringPool[nameRef];
		if (name == L"Code")
		{
			detail::VMContext cont = { jclass, method };
			detail::VMResource res = { *this, m_stringPool, m_stackFrame };
			m_stackFrame.resize(1);
			auto vmcont = detail::VMContext{
				cont.jclass,
				cont.method,
				1, // string[] args
				m_stackFrame.size()
			};
			//TODO: push args value
			execute(vmcont, res);
			methodFound = true;
		}
	}

	if (!methodFound)
		cout << "Cannot invoke method because method name not found" << endl;
}

namespace
{
	bool ParseType(VM& vm, const std::wstring& str, JType& type, int& pos)
	{
		u32 aryDim = 0;
		while (str[pos] == L'[')
		{
			pos++;
			aryDim++;
		}
		type.aryDim = aryDim;

		PrimitiveType t;
		switch (str[pos])
		{
		case L'V': t = PrimitiveType::Void; break;
		case L'Z': t = PrimitiveType::Boolean; break;
		case L'B': t = PrimitiveType::Byte; break;
		case L'C': t = PrimitiveType::Char; break;
		case L'S': t = PrimitiveType::Short; break;
		case L'I': t = PrimitiveType::Int; break;
		case L'J': t = PrimitiveType::Long; break;
		case L'F': t = PrimitiveType::Float; break;
		case L'D': t = PrimitiveType::Double; break;
		case L'L': t = PrimitiveType::Class; break;
		default: return false;
		}
		type.type = t;
		pos++;

		type.nameRef = 0;
		if (t == PrimitiveType::Class)
		{
			auto p = str.find(';', pos);
			if (p == string::npos)
				p = str.length() - 1;
			type.nameRef = vm.InternString(wstring(str, pos, p - pos));
			pos = p + 1;
		}

		return true;
	}
}

JType jvm::DecodeType(VM& vm, const std::wstring& str)
{
	JType t;
	int p = 0;
	if (!ParseType(vm, str, t, p))
		assert(0);

	return t;
}

JSignature jvm::DecodeSignature(VM& vm, const std::wstring& str)
{
	JSignature sig;
	JType t;
	int p = 0;

	if (str[p] != L'(')
		assert(0);
	p++;

	while (str[p] != L')')
	{
		if (!ParseType(vm, str, t, p))
			assert(0);
		sig.args.push_back(t);
	}
	p++;

	if (!ParseType(vm, str, t, p))
		assert(0);
	sig.ret = t;

	return sig;
}
