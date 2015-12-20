#pragma once

#include "jvm.h"
#include "jvmClass.h"

namespace jvm
{
	void execute(const detail::VMContext& vmcont, detail::VMResource& vmres) noexcept;
}
