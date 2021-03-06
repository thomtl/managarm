#include <hw.frigg_pb.hpp>
#include <mbus.frigg_pb.hpp>
#include "../../generic/execution/coroutine.hpp"
#include "../../generic/fiber.hpp"
#include "../../generic/service_helpers.hpp"
#include "../../system/acpi/acpi.hpp"

namespace thor {
	extern frigg::LazyInitializer<LaneHandle> mbusClient;
}

namespace thor::legacy_pc {

namespace {
	// TODO: Distinguish protocol errors and end-of-lane.
	//       Print a log message on protocol errors.

	coroutine<Error> handleRequest(LaneHandle boundLane) {
		auto respondWithError = [] (LaneHandle lane, uint32_t protoError)
				-> coroutine<Error> {
			managarm::hw::SvrResponse<KernelAlloc> resp(*kernelAlloc);
			resp.set_error(protoError);

			frg::string<KernelAlloc> ser(*kernelAlloc);
			resp.SerializeToString(&ser);
			frigg::UniqueMemory<KernelAlloc> respBuffer{*kernelAlloc, ser.size()};
			memcpy(respBuffer.data(), ser.data(), ser.size());
			auto respError = co_await SendBufferSender{lane, std::move(respBuffer)};
			if(respError)
				co_return respError;
			co_return kErrSuccess;
		};

		auto [acceptError, lane] = co_await AcceptSender{boundLane};
		if(acceptError)
			co_return acceptError;

		auto [reqError, reqBuffer] = co_await RecvBufferSender{lane};
		if(reqError)
			co_return reqError;
		managarm::hw::CntRequest<KernelAlloc> req(*kernelAlloc);
		req.ParseFromArray(reqBuffer.data(), reqBuffer.size());

		if(req.req_type() == managarm::hw::CntReqType::GET_PCI_INFO) {
			managarm::hw::SvrResponse<KernelAlloc> resp(*kernelAlloc);
			resp.set_error(managarm::hw::Errors::SUCCESS);

			managarm::hw::PciBar<KernelAlloc> mainBar(*kernelAlloc);
			mainBar.set_io_type(managarm::hw::IoType::PORT);
			mainBar.set_address(0x1F0);
			mainBar.set_length(8);
			resp.add_bars(std::move(mainBar));

			managarm::hw::PciBar<KernelAlloc> altBar(*kernelAlloc);
			altBar.set_io_type(managarm::hw::IoType::PORT);
			altBar.set_address(0x3F6);
			altBar.set_length(1);
			resp.add_bars(std::move(altBar));

			for(size_t k = 2; k < 6; k++) {
				managarm::hw::PciBar<KernelAlloc> noBar(*kernelAlloc);
				noBar.set_io_type(managarm::hw::IoType::NO_BAR);
				resp.add_bars(std::move(noBar));
			}

			frg::string<KernelAlloc> ser(*kernelAlloc);
			resp.SerializeToString(&ser);
			frigg::UniqueMemory<KernelAlloc> respBuffer{*kernelAlloc, ser.size()};
			memcpy(respBuffer.data(), ser.data(), ser.size());
			auto respError = co_await SendBufferSender{lane, std::move(respBuffer)};
			if(respError)
				co_return respError;
		}else if(req.req_type() == managarm::hw::CntReqType::ACCESS_BAR) {
			auto space = frigg::makeShared<IoSpace>(*kernelAlloc);
			if(req.index() == 0) {
				for(size_t p = 0; p < 8; ++p)
					space->addPort(0x1F0 + p);
			}else if(req.index() == 1) {
				space->addPort(0x3F6);
			}else{
				co_return co_await respondWithError(std::move(lane),
						managarm::hw::Errors::OUT_OF_BOUNDS);
			}

			managarm::hw::SvrResponse<KernelAlloc> resp{*kernelAlloc};
			resp.set_error(managarm::hw::Errors::SUCCESS);

			frg::string<KernelAlloc> ser(*kernelAlloc);
			resp.SerializeToString(&ser);
			frigg::UniqueMemory<KernelAlloc> respBuffer{*kernelAlloc, ser.size()};
			memcpy(respBuffer.data(), ser.data(), ser.size());
			auto respError = co_await SendBufferSender{lane, std::move(respBuffer)};
			if(respError)
				co_return respError;
			auto ioError = co_await PushDescriptorSender{lane, IoDescriptor{space}};
			if(ioError)
				co_return ioError;
		}else if(req.req_type() == managarm::hw::CntReqType::ACCESS_IRQ) {
			managarm::hw::SvrResponse<KernelAlloc> resp{*kernelAlloc};
			resp.set_error(managarm::hw::Errors::SUCCESS);

			auto object = frigg::makeShared<IrqObject>(*kernelAlloc,
					frigg::String<KernelAlloc>{*kernelAlloc, "isa-irq.ata"});
			auto irqOverride = resolveIsaIrq(14);
			IrqPin::attachSink(getGlobalSystemIrq(irqOverride.gsi), object.get());

			frg::string<KernelAlloc> ser(*kernelAlloc);
			resp.SerializeToString(&ser);
			frigg::UniqueMemory<KernelAlloc> respBuffer{*kernelAlloc, ser.size()};
			memcpy(respBuffer.data(), ser.data(), ser.size());
			auto respError = co_await SendBufferSender{lane, std::move(respBuffer)};
			if(respError)
				co_return respError;
			auto irqError = co_await PushDescriptorSender{lane, IrqDescriptor{object}};
			if(irqError)
				co_return irqError;
		}else{
			co_return co_await respondWithError(std::move(lane),
					managarm::hw::Errors::ILLEGAL_REQUEST);
		}

		co_return kErrSuccess;
	}

	coroutine<void> handleBind(LaneHandle objectLane) {
		auto [acceptError, lane] = co_await AcceptSender{objectLane};
		assert(!acceptError && "Unexpected mbus transaction");

		auto [reqError, reqBuffer] = co_await RecvBufferSender{lane};
		assert(!reqError && "Unexpected mbus transaction");
		managarm::mbus::SvrRequest<KernelAlloc> req(*kernelAlloc);
		req.ParseFromArray(reqBuffer.data(), reqBuffer.size());
		assert(req.req_type() == managarm::mbus::SvrReqType::BIND
				&& "Unexpected mbus transaction");

		auto stream = createStream();
		managarm::mbus::CntResponse<KernelAlloc> resp(*kernelAlloc);
		resp.set_error(managarm::mbus::Error::SUCCESS);

		frg::string<KernelAlloc> ser(*kernelAlloc);
		resp.SerializeToString(&ser);
		frigg::UniqueMemory<KernelAlloc> respBuffer{*kernelAlloc, ser.size()};
		memcpy(respBuffer.data(), ser.data(), ser.size());
		auto respError = co_await SendBufferSender{lane, std::move(respBuffer)};
		assert(!respError && "Unexpected mbus transaction");
		auto boundError = co_await PushDescriptorSender{lane, LaneDescriptor{stream.get<1>()}};
		assert(!boundError && "Unexpected mbus transaction");

		auto boundLane = stream.get<0>();
		while(true) {
			// Terminate the connection on protocol errors.
			auto error = co_await handleRequest(boundLane);
			if(error == kErrEndOfLane)
				break;
			if(isRemoteIpcError(error))
				frigg::infoLogger() << "thor: Aborting legacy-pc.ata request"
						" after remote violated the protocol" << frigg::endLog;
			assert(!error);
		}
	}
}

coroutine<void> initializeAtaDevice() {
	managarm::mbus::Property<KernelAlloc> legacy_prop(*kernelAlloc);
	legacy_prop.set_name(frg::string<KernelAlloc>(*kernelAlloc, "legacy"));
	auto &legacy_item = legacy_prop.mutable_item().mutable_string_item();
	legacy_item.set_value(frg::string<KernelAlloc>(*kernelAlloc, "ata"));

	managarm::mbus::CntRequest<KernelAlloc> req(*kernelAlloc);
	req.set_req_type(managarm::mbus::CntReqType::CREATE_OBJECT);
	req.set_parent_id(1);
	req.add_properties(std::move(legacy_prop));

	frg::string<KernelAlloc> ser{*kernelAlloc};
	req.SerializeToString(&ser);
	frigg::UniqueMemory<KernelAlloc> reqBuffer{*kernelAlloc, ser.size()};
	memcpy(reqBuffer.data(), ser.data(), ser.size());
	auto [offerError, lane] = co_await OfferSender{*mbusClient};
	assert(!offerError && "Unexpected mbus transaction");
	auto reqError = co_await SendBufferSender{lane, std::move(reqBuffer)};
	assert(!reqError && "Unexpected mbus transaction");

	auto [respError, respBuffer] = co_await RecvBufferSender{lane};
	assert(!respError && "Unexpected mbus transaction");
	managarm::mbus::SvrResponse<KernelAlloc> resp(*kernelAlloc);
	resp.ParseFromArray(respBuffer.data(), respBuffer.size());
	assert(resp.error() == managarm::mbus::Error::SUCCESS && "Unexpected mbus transaction");

	auto [objectError, objectDescriptor] = co_await PullDescriptorSender{lane};
	assert(!objectError && "Unexpected mbus transaction");
	assert(objectDescriptor.is<LaneDescriptor>() && "Unexpected mbus transaction");
	auto objectLane = objectDescriptor.get<LaneDescriptor>().handle;

	while(true)
		co_await handleBind(objectLane);
}

void initializeDevices() {
	// For now, we only need the kernel fiber to make sure mbusClient is already initialized.
	KernelFiber::run([=] {
		async::detach_with_allocator(*kernelAlloc, initializeAtaDevice());
	});
}

} // namespace thor::legacy_pc
