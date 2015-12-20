#include "jvm.h"

int main(int argc, char** argv)
{
	jvm::VM vm;
	vm.Load("sample/Main.class");
	vm.Invoke(L"Main", L"main", L"([Ljava/lang/String;)V");
	//vm.Load("sample/Fibonacci.class");
	//vm.Invoke(L"Fibonacci", L"main", L"([Ljava/lang/String;)V");
	return 0;
}