
#include "kernel.hpp"

namespace traits = frigg::traits;

namespace thor {

frigg::util::LazyInitializer<ThreadQueue> scheduleQueue;

SharedPtr<Thread, KernelAlloc> resetCurrentThread() {
	ASSERT(!intsAreEnabled());
	auto cpu_context = (CpuContext *)thorRtGetCpuContext();
	ASSERT(cpu_context->currentThread);

	UnsafePtr<Thread, KernelAlloc> thread = cpu_context->currentThread;
	thread->deactivate();
	return traits::move(cpu_context->currentThread);
}

void dropCurrentThread() {
	ASSERT(!intsAreEnabled());
	resetCurrentThread();
	doSchedule();
}

void enterThread(SharedPtr<Thread, KernelAlloc> &&thread) {
	ASSERT(!intsAreEnabled());
	auto cpu_context = (CpuContext *)thorRtGetCpuContext();
	ASSERT(!cpu_context->currentThread);

	thread->activate();
	cpu_context->currentThread = traits::move(thread);
	restoreThisThread();
}

void doSchedule() {
	ASSERT(!intsAreEnabled());
	
	auto cpu_context = (CpuContext *)thorRtGetCpuContext();
	ASSERT(!cpu_context->currentThread);
	
	while(scheduleQueue->empty()) {
		enableInts();
		halt();
		disableInts();
	}

	enterThread(scheduleQueue->removeFront());
}

void enqueueInSchedule(SharedPtr<Thread, KernelAlloc> &&thread) {
	scheduleQueue->addBack(traits::move(thread));
}

} // namespace thor

