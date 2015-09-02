
#include <frigg/types.hpp>
#include <frigg/traits.hpp>
#include <frigg/support.hpp>
#include <frigg/debug.hpp>
#include <frigg/libc.hpp>
#include <frigg/algorithm.hpp>
#include <frigg/initializer.hpp>
#include <frigg/atomic.hpp>
#include <frigg/memory.hpp>
#include <frigg/string.hpp>
#include <frigg/array.hpp>
#include <frigg/vector.hpp>
#include <frigg/hashmap.hpp>
#include <frigg/linked.hpp>
#include <frigg/priority_queue.hpp>
#include <frigg/tuple.hpp>
#include <frigg/variant.hpp>
#include <frigg/smart_ptr.hpp>

#include <frigg/arch_x86/machine.hpp>

typedef uint64_t Word;

typedef uint64_t PhysicalAddr;
typedef uint64_t VirtualAddr;
typedef uint64_t VirtualOffset;

#include "physical.hpp"
#include "arch_x86/cpu.hpp"
#include "arch_x86/ints.hpp"
#include "arch_x86/paging.hpp"
#include "arch_x86/pic.hpp"
#include "arch_x86/hpet.hpp"
#include "arch_x86/system.hpp"

#include "core.hpp"
#include "schedule.hpp"

#include <thor.h>

namespace thor {
namespace k_init {

void main();

} } // namespace thor::k_init

