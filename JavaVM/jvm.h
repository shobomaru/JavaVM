#pragma once

#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <memory>

namespace jvm
{
	using u8 = uint8_t;
	using u16 = uint16_t;
	using u32 = uint32_t;
	using u64 = uint64_t;
	using s8 = int8_t;
	using s16 = int16_t;
	using s32 = int32_t;
	using s64 = int64_t;
	using f32 = float_t;
	using f64 = double_t;
	using c8 = char;
	using c16 = wchar_t;

	inline void revbits(u16& v) { v = _byteswap_ushort(v); }
	inline void revbits(u32& v) { v = _byteswap_ulong(v); }
	inline void revbits(u64& v) { v = _byteswap_uint64(v); }

	class VM;
	struct JClass;

	struct CFClassFile;
	struct CFMethod;

	namespace detail
	{
		struct VMContext
		{
			JClass& jclass;
			const CFMethod& method;
			u32 numArgs;
			u32 baseStackIndex;
		};

		struct VMResource
		{
			VM& vm;
			std::vector<std::wstring>& stringPool;
			std::vector<u32>& stackFrame;
		};
	}

	enum class PrimitiveType
	{
		Boolean,
		Char,
		Float,
		Double,
		Byte,
		Short,
		Int,
		Long,
		Class,
		Void
	};

	struct JObject
	{
		u64 marker;
		PrimitiveType type;
		s32 length;
		std::unique_ptr<u8[]> data;
	};

	struct JValue
	{
		union
		{
			bool z;
			s8 b;
			c16 c;
			s16 s;
			s32 i;
			s64 j;
			f32 f;
			f64 d;
			JObject* l;
		} val;
	};

	struct JType
	{
		u32 aryDim;
		PrimitiveType type;
		u32 nameRef;
	};

	struct JSignature
	{
		JType ret;
		std::vector<JType> args;
	};

	struct JMember
	{
		const std::wstring& name;
		JType type;
		JValue obj;
	};

	struct JClass
	{
		CFClassFile& cf;
		std::vector<JMember> staticFields;
	};

	class VM
	{
	public:
		VM();
		~VM();
		void Load(const char* path);
		void Invoke(const std::wstring& clazz, const std::wstring& method, const std::wstring& signature);
		JObject& NewPrimitiveArray(PrimitiveType type, s32 numElem);
		u32 InternString(std::wstring&& str);
		const std::wstring& GetInternedString(u32 handle)
		{
			return m_stringPool[handle];
		}

		VM(const VM&) = delete;
		VM& operator=(const VM&) = delete;

	private:
		std::vector<std::wstring> m_stringPool;
		std::vector<JClass> m_classPool;
		std::vector<CFClassFile> m_classFilePool;
		std::vector<u32> m_stackFrame;
		std::list<JObject> m_instanceTable;

		void Invoke(JClass& jclass, const CFMethod& method, bool allowNonPublic) noexcept;
	};

	inline intptr_t StackObjectToValue(const JObject& o)
	{
		const JObject* j = &o;
		intptr_t v;
		memcpy(&v, &j, sizeof v);
		return v;
	}

	inline JObject& StackValueToObject(intptr_t v)
	{
		JObject* o;
		memcpy(&o, &v, sizeof o);
		return *o;
	}

	JType DecodeType(VM& vm, const std::wstring& str);
	JSignature DecodeSignature(VM& vm, const std::wstring& str);
}
