#include "jvmExec.h"
#include <iostream>
#include <cassert>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using namespace std;
using namespace jvm;

void jvm::execute(const detail::VMContext& vmcont, detail::VMResource& vmres) noexcept
{
	const auto& ConstantPool = vmcont.jclass.cf.constant_pool;

	// Code属性の取得
	const CFAttribute& codeSection = [&]()
	{
		for (auto& m : vmcont.method.attributes)
		{
			if (m.type == CFAttribute::Type::Code)
				return m;
		}
		return CFAttribute();
	}();
	const auto& Code = codeSection.val.code;

	// スタックフレームの確保
	const u32 StackSize = Code.max_locals + Code.max_stack - vmcont.numArgs;
	vmres.stackFrame.resize(vmcont.baseStackIndex + StackSize);

	// インタプリタの準備
	auto& stack = vmres.stackFrame;
	u32 localIdx = vmcont.baseStackIndex - vmcont.numArgs;
	u32 stackIdx = localIdx + Code.max_locals;
	u32 codeIdx = 0;

	// デバッグ用：ローカル変数の出力
	const auto PrintLocalVariables = [&]()
	{
		for (auto& a : Code.attributes)
		{
			if (a.type == CFAttribute::Type::LocalVariableTable)
			{
				u32 baseLocalIdx = vmcont.baseStackIndex - vmcont.numArgs;
				auto& lvt = a.val.localVariableTable;
				wcout << L"Print local variables" << endl;
				for (auto& v : lvt.local_variable_table)
				{
					if (v.start_pc <= codeIdx && codeIdx < ((u32)(v.start_pc) + v.length))
					{
						u32 *stackPtr = &stack[baseLocalIdx + v.index];
						auto& varName = vmres.vm.GetInternedString(ConstantPool[v.name_index].val.f5.idx);
						wcout << L'\t' << varName << L" = " << *stackPtr << endl;
					}
				}
			}
		}
	};

	// インタプリタの実行
	bool executeBytecode = true;
	while (executeBytecode)
	{
		const u8 mnemonic = Code.code[codeIdx];
		switch (mnemonic)
		{
		case 0x02: // iconst_m1
			stack[stackIdx++] = static_cast<u32>(-1);
			codeIdx++;
			break;
		case 0x03: // iconst_0
			stack[stackIdx++] = 0;
			codeIdx++;
			break;
		case 0x04: // iconst_1
			stack[stackIdx++] = 1;
			codeIdx++;
			break;
		case 0x05: // iconst_2
			stack[stackIdx++] = 2;
			codeIdx++;
			break;
		case 0x06: // iconst_3
			stack[stackIdx++] = 3;
			codeIdx++;
			break;
		case 0x07: // iconst_4
			stack[stackIdx++] = 4;
			codeIdx++;
			break;
		case 0x08: // iconst_5
			stack[stackIdx++] = 5;
			codeIdx++;
			break;

		case 0x10: // bipush
			stack[stackIdx++] = static_cast<s8>(Code.code[codeIdx + 1]);
			codeIdx += 2;
			break;

		case 0x12: // ldc
			assert(ConstantPool[codeIdx + 1].type != CFConstantPool::Type::String); // TODO: 未実装
			stack[stackIdx++] = ConstantPool[Code.code[codeIdx + 1]].val.f3.v;
			codeIdx += 2;
			break;

		case 0x15: // iload
			stack[stackIdx++] = stack[localIdx + Code.code[codeIdx + 1]];
			codeIdx += 2;
			break;
		case 0x1a: // iload_0
			stack[stackIdx++] = stack[localIdx];
			codeIdx++;
			break;
		case 0x1b: // iload_1
			stack[stackIdx++] = stack[localIdx + 1];
			codeIdx++;
			break;
		case 0x1c: // iload_2
			stack[stackIdx++] = stack[localIdx + 2];
			codeIdx++;
			break;
		case 0x1d: // iload_3
			stack[stackIdx++] = stack[localIdx + 3];
			codeIdx++;
			break;

		case 0x2a: // aload_0
			stack[stackIdx++] = stack[localIdx];
			codeIdx++;
			break;
		case 0x2b: // aload_1
			stack[stackIdx++] = stack[localIdx + 1];
			codeIdx++;
			break;
		case 0x2c: // aload_2
			stack[stackIdx++] = stack[localIdx + 2];
			codeIdx++;
			break;
		case 0x2d: // aload_3
			stack[stackIdx++] = stack[localIdx + 3];
			codeIdx++;
			break;

		case 0x2e: // iaload
		{
			JObject& aryref = StackValueToObject(stack[stackIdx - 2]);
			s32 idx = static_cast<s32>(stack[stackIdx - 1]);
			if (idx < 0 || aryref.length <= idx)
				assert(0); // throw ArrayIndexOutOfBoundsException
			memcpy(&stack[stackIdx - 2], aryref.data.get() + 4 * idx, 4);
			stackIdx--;
			codeIdx++;
			break;
		}

		case 0x36: // istore
			stack[localIdx + Code.code[codeIdx + 1]] = stack[--stackIdx];
			codeIdx += 2;
			break;
		case 0x3b: // istore_0
			stack[localIdx] = stack[--stackIdx];
			codeIdx++;
			break;
		case 0x3c: // istore_1
			stack[localIdx + 1] = stack[--stackIdx];
			codeIdx++;
			break;
		case 0x3d: // istore_2
			stack[localIdx + 2] = stack[--stackIdx];
			codeIdx++;
			break;
		case 0x3e: // istore_3
			stack[localIdx + 3] = stack[--stackIdx];
			codeIdx++;
			break;

		case 0x4b: // astore_0
			stack[localIdx] = stack[--stackIdx];
			codeIdx++;
			break;
		case 0x4c: // astore_1
			stack[localIdx + 1] = stack[--stackIdx];
			codeIdx++;
			break;
		case 0x4d: // astore_2
			stack[localIdx + 2] = stack[--stackIdx];
			codeIdx++;
			break;
		case 0x4e: // astore_3
			stack[localIdx + 3] = stack[--stackIdx];
			codeIdx++;
			break;

		case 0x4f: // iastore
		{
			JObject& aryref = StackValueToObject(stack[stackIdx - 3]);
			s32 idx = static_cast<s32>(stack[stackIdx - 2]);
			u32 val = stack[stackIdx - 1];
			if (idx < 0 || aryref.length <= idx)
				assert(0); // throw ArrayIndexOutOfBoundsException
			memcpy(aryref.data.get() + 4 * idx, &val, 4);
			stackIdx -= 3;
			codeIdx++;
			break;
		}

		case 0x59: // dup
			stack[stackIdx] = stack[stackIdx - 1];
			stackIdx++;
			codeIdx++;
			break;

		case 0x60: // iadd
			stack[stackIdx - 2] += stack[stackIdx - 1];
			stackIdx--;
			codeIdx++;
			break;
		case 0x64: // isub
			stack[stackIdx - 2] -= stack[stackIdx - 1];
			stackIdx--;
			codeIdx++;
			break;
		case 0x68: // imul
			stack[stackIdx - 2] *= stack[stackIdx - 1];
			stackIdx--;
			codeIdx++;
			break;
		case 0x6c: // idiv
			if (stack[stackIdx - 1] == 0)
			{
				const auto ThrowException = [&](const wstring& name, u32& codeIdx)
				{
					for (auto& e : Code.exception_table)
					{
						if (e.start_pc <= codeIdx
							&& codeIdx < e.end_pc)
						{
							u16 ec = ConstantPool[e.catch_type].val.f1.v;
							wstring& ecName = vmres.stringPool[ConstantPool[ec].val.f5.idx];
							if (ec == 0 || ecName == name)
							{
								codeIdx = e.handler_pc;
								return true;
							}
						}
					}
					return false;
				};
				u32 threwPc = codeIdx;
				bool handled = ThrowException(L"java/lang/ArithmeticException", codeIdx);
				if (handled) // TODO: 親クラス例外の探索
				{
					stack[stackIdx++] = 555555555; // TODO: ArithmeticExceptionをnewしてスタックに積む
				}
				else
				{
					const auto GetSourceLine = [&](u32 pc) -> int
					{
						for (auto& a : Code.attributes)
						{
							if (a.type == CFAttribute::Type::LineNumberTable)
							{
								if (a.val.lineNumberTable.line_number_table_length == 0)
									return 0;
								auto prev = a.val.lineNumberTable.line_number_table[0];
								for (auto& ln : a.val.lineNumberTable.line_number_table)
								{
									int startPc = ln.first;
									int lineNum = ln.second;
									if (pc < prev.first)
										return prev.second;
									prev = ln;
								}
								return prev.second;
							}
						}
						return 0;
					};
					const auto GetSourceFileName = [&]() -> wstring
					{
						for (auto& a : vmcont.jclass.cf.attributes)
						{
							if (a.type == CFAttribute::Type::SourceFile)
							{
								u32 nameIdx = ConstantPool[a.val.sourceFile.sourcefile_index].val.f5.idx;
								return vmres.vm.GetInternedString(nameIdx);
							}
						}
						return L"Unknown Source";
					};
					u16 thisCls = ConstantPool[vmcont.jclass.cf.this_class].val.f1.v;
					auto& thisClsName = vmres.vm.GetInternedString(ConstantPool[thisCls].val.f5.idx);
					u16 thisMet = vmcont.method.name_index;
					auto& thisMetName = vmres.vm.GetInternedString(ConstantPool[thisMet].val.f5.idx);
					wstring fileName = GetSourceFileName();
					int lineNum = GetSourceLine(threwPc);
					wcerr << L"java.lang.ArithmeticException" << endl;
					wcerr << L"\tat " << thisClsName << L"." << thisMetName << L"(" << fileName << L":" << lineNum << L")" << endl;
					PrintLocalVariables();
					assert(0); // TODO: コールスタックをたどる
				}
			}
			else
			{
				stack[stackIdx - 2] /= stack[stackIdx - 1];
				stackIdx--;
				codeIdx++;
			}
			break;

		case 0x84: // iinc
			stack[localIdx + Code.code[codeIdx + 1]] += static_cast<s8>(Code.code[codeIdx + 2]);
			codeIdx += 3;
			break;

		case 0x9f: // if_icmpeq
			if (static_cast<s32>(stack[stackIdx - 2]) == static_cast<s32>(stack[stackIdx - 1])) {
				codeIdx += static_cast<s16>((Code.code[codeIdx + 1] << 8) + Code.code[codeIdx + 2]);
			}
			else {
				codeIdx += 3;
			}
			stackIdx -= 2;
			break;
		case 0xa0: // if_icmpne
			if (static_cast<s32>(stack[stackIdx - 2]) != static_cast<s32>(stack[stackIdx - 1])) {
				codeIdx += static_cast<s16>((Code.code[codeIdx + 1] << 8) + Code.code[codeIdx + 2]);
			}
			else {
				codeIdx += 3;
			}
			stackIdx -= 2;
			break;
		case 0xa1: // if_icmplt
			if (static_cast<s32>(stack[stackIdx - 2]) < static_cast<s32>(stack[stackIdx - 1])) {
				codeIdx += static_cast<s16>((Code.code[codeIdx + 1] << 8) + Code.code[codeIdx + 2]);
			}
			else {
				codeIdx += 3;
			}
			stackIdx -= 2;
			break;
		case 0xa2: // if_icmpge
			if (static_cast<s32>(stack[stackIdx - 2]) >= static_cast<s32>(stack[stackIdx - 1])) {
				codeIdx += static_cast<s16>((Code.code[codeIdx + 1] << 8) + Code.code[codeIdx + 2]);
			} else {
				codeIdx += 3;
			}
			stackIdx -= 2;
			break;
		case 0xa3: // if_icmpgt
			if (static_cast<s32>(stack[stackIdx - 2]) > static_cast<s32>(stack[stackIdx - 1])) {
				codeIdx += static_cast<s16>((Code.code[codeIdx + 1] << 8) + Code.code[codeIdx + 2]);
			} else {
				codeIdx += 3;
			}
			stackIdx -= 2;
			break;
		case 0xa4: // if_icmple
			if (static_cast<s32>(stack[stackIdx - 2]) <= static_cast<s32>(stack[stackIdx - 1])) {
				codeIdx += static_cast<s16>((Code.code[codeIdx + 1] << 8) + Code.code[codeIdx + 2]);
			} else {
				codeIdx += 3;
			}
			stackIdx -= 2;
			break;
		case 0xa7: // goto
			codeIdx += static_cast<s16>((Code.code[codeIdx + 1] << 8) + Code.code[codeIdx + 2]);
			break;

		case 0xac: // ireturn
			stack[vmcont.baseStackIndex - vmcont.numArgs] = stack[stackIdx - 1];
			executeBytecode = false;
			break;
		case 0xb1: // return
			executeBytecode = false;
			break;

		case 0xb2: // getstatic
		{
			u32 fieldRef = (Code.code[codeIdx + 1] << 8) + Code.code[codeIdx + 2];
			u16 cls = ConstantPool[fieldRef].val.f2.v1;
			u16 nat = ConstantPool[fieldRef].val.f2.v2;
			u16 fieldNameRef = ConstantPool[nat].val.f2.v1;
			u16 fieldTypeRef = ConstantPool[nat].val.f2.v2;
			auto& fieldName = vmres.vm.GetInternedString(ConstantPool[fieldNameRef].val.f5.idx);
			auto& typeName = vmres.vm.GetInternedString(ConstantPool[fieldTypeRef].val.f5.idx);
			JType type = DecodeType(vmres.vm, typeName);

			if (cls != vmcont.jclass.cf.this_class)
				assert(0); // 未実装

			bool found = false;
			for (auto& fld : vmcont.jclass.staticFields)
			{
				if (fld.name == fieldName
					&& fld.type.aryDim == type.aryDim
					&& fld.type.type == type.type)
				{
					memcpy(&stack[stackIdx++] ,&fld.obj.val.i, 4); // TODO: long/double型では64bit代入する
					found = true;
					break;
				}
			}
			if (!found)
				assert(0); // Throw java.lang.NoSuchFieldException

			codeIdx += 3;
			break;
		}

		case 0xb3: // putstatic
		{
			u32 fieldRef = (Code.code[codeIdx + 1] << 8) + Code.code[codeIdx + 2];
			u16 cls = ConstantPool[fieldRef].val.f2.v1;
			u16 nat = ConstantPool[fieldRef].val.f2.v2;
			u16 fieldNameRef = ConstantPool[nat].val.f2.v1;
			u16 fieldTypeRef = ConstantPool[nat].val.f2.v2;
			auto& fieldName = vmres.vm.GetInternedString(ConstantPool[fieldNameRef].val.f5.idx);
			auto& typeName = vmres.vm.GetInternedString(ConstantPool[fieldTypeRef].val.f5.idx);
			JType type = DecodeType(vmres.vm, typeName);

			if (cls != vmcont.jclass.cf.this_class)
				assert(0); // 未実装

			bool found = false;
			for (auto& fld : vmcont.jclass.staticFields)
			{
				if (fld.name == fieldName
					&& fld.type.aryDim == type.aryDim
					&& fld.type.type == type.type)
				{
					memcpy(&fld.obj.val.i, &stack[--stackIdx], 4); // TODO: long/double型では64bit代入する
					found = true;
					break;
				}
			}
			if (!found)
				assert(0); // Throw java.lang.NoSuchFieldException

			codeIdx += 3;
			break;
		}

		case 0xbc: // newarray
		{
			s32 sz = static_cast<s32>(stack[stackIdx - 1]);
			if (sz < 0)
				assert(0); // TODO: throw Java.lang.NegativeArraySizeException
			u32 type = Code.code[codeIdx + 1];
			PrimitiveType ptype = PrimitiveType::Boolean;
			switch (type)
			{
			case 4: ptype = PrimitiveType::Boolean; break;
			case 5: ptype = PrimitiveType::Char; break;
			case 6: ptype = PrimitiveType::Float; break;
			case 7: ptype = PrimitiveType::Double; break;
			case 8: ptype = PrimitiveType::Byte; break;
			case 9: ptype = PrimitiveType::Short; break;
			case 10: ptype = PrimitiveType::Int; break;
			case 11: ptype = PrimitiveType::Long; break;
			default: assert(0);
			}
			auto& ary = vmres.vm.NewPrimitiveArray(ptype, sz);
			stack[stackIdx - 1] = StackObjectToValue(ary);
			codeIdx += 2;
			break;
		}

		case 0xb8: // invokestatic
		{
			u32 methodRef = (Code.code[codeIdx + 1] << 8) + Code.code[codeIdx + 2];
			u16 cls = ConstantPool[methodRef].val.f2.v1;
			u16 clazz = ConstantPool[cls].val.f1.v;
			u16 nat = ConstantPool[methodRef].val.f2.v2;
			u16 name = ConstantPool[nat].val.f2.v1;
			u16 type = ConstantPool[nat].val.f2.v2;
			wstring& clsName = vmres.stringPool[ConstantPool[clazz].val.f5.idx];
			wstring& methodName = vmres.stringPool[ConstantPool[name].val.f5.idx];
			wstring& typeName = vmres.stringPool[ConstantPool[type].val.f5.idx];
			// Special function
			if (clsName == L"Main" && methodName == L"output" && typeName == L"(I)V")
			{
				cout << "Main.output(int) : " << stack[stackIdx - 1] << endl;
				stackIdx--;
				codeIdx += 3;
				break;
			}
			u16 thisCls = ConstantPool[vmcont.jclass.cf.this_class].val.f1.v;
			if (thisCls == clazz)
			{
				s32 callNat = -1;
				for (int i = 0; i < vmcont.jclass.cf.methods_count; i++)
				{
					auto& m = vmcont.jclass.cf.methods[i];
					u16 n = m.name_index;
					u16 d = m.descriptor_index;
					if (n == name && d == type)
					{
						callNat = i;
						break;
					}
				}
				if (callNat == -1)
					assert(0); // throw NoSuchMethodException

				auto& m = vmcont.jclass.cf.methods[callNat];
				u32 numArgs = m.signature.args.size(); // TODO: longとdoubleのときは2倍

				auto context = detail::VMContext {
					vmcont.jclass,
					m,
					numArgs,
					stackIdx
				};
				execute(context, vmres);

				bool isRetTypeVoid = (m.signature.ret.type == PrimitiveType::Void);
				stackIdx = stackIdx - numArgs + (isRetTypeVoid ? 0 : 1);
			}
			else
			{
				cout << "Error: invokestatic" << endl;
				assert(0);
			}
			codeIdx += 3;
			break;
		}

		default:
			cout << "Error: unknown mnemonic : 0x" << hex << static_cast<int>(mnemonic) << endl;
			assert(0);
		}
	}
}
