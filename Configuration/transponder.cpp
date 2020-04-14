#include "transponder.hpp"
#include "moxa.hpp"

using namespace WarGrey::SCADA;
using namespace WarGrey::DTPM;
using namespace WarGrey::GYDM;

#define ON_MOBILE(responders, on_respond, logger, ...) do { \
for (auto r : responders) { \
if (r->respondable()) { \
r->pre_respond(logger); \
r->on_respond(__VA_ARGS__, logger); \
r->post_respond(logger); \
} \
} \
} while (0)

/*************************************************************************************************/
Transponder::Transponder() {
	this->tranceiver = moxa_tcp_as_ais(MOXA_TCP::AIS, this);
}

void Transponder::push_receiver(IAISResponder* r) {
	if (r != nullptr) {
		this->responders.push_back(r);
	}
}

/*************************************************************************************************/
void Transponder::on_ASO(int id, long long timepoint_ms, bool self, uint16 mmsi, ASO* prca, uint8 priority, Syslog* logger) {
	if (!self) {
		AISPositionReport pr;

		pr.longitude = ais_longitude_filter(prca->longitude);
		pr.latitude = ais_latitude_filter(prca->latitude);
		pr.turn = ais_turn_filter(prca->turn);
		pr.speed = ais_speed_filter(prca->speed);
		pr.course = ais_course_filter(prca->course);
		pr.heading = ais_heading360_filter(prca->heading);

		ON_MOBILE(this->responders, on_position_report, logger, id, timepoint_ms, mmsi, &pr);

		logger->log_message(Log::Info, L"timestamp: (%lf, %lf)", pr.longitude, pr.latitude);
	}
}

void Transponder::on_SDR(int id, long long timepoint_ms, bool self, uint16 mmsi, SDR* sdr, uint8 priority, Syslog* logger) {
	switch (sdr->partno) {
	case SDR::Format::PartA: logger->log_message(Log::Info, L"SDR: A[%S]", sdr->part.a.shipname.c_str()); break;
	case SDR::Format::PartB: logger->log_message(Log::Info, L"SDR: B[%S:%s]", sdr->part.b.vendorid.c_str(), (sdr->part.b.auxiliary ? L"Auxiliary Craft" : L"Craft")); break;
	}
}
