﻿#include "metrics/dredger.hpp"

#include "datum/flonum.hpp"

#include "iotables/ai_metrics.hpp"

#include "module.hpp"
#include "brushes.hxx"
#include "moxa.hpp"

using namespace WarGrey::SCADA;
using namespace WarGrey::DTPM;

using namespace Microsoft::Graphics::Canvas::Brushes;

/*************************************************************************************************/
#define SET_METRICS(ms, id, v, p)          ms[_I(id)] = v
#define SET_VALID_METRICS(ms, id, b, v, p) ms[_I(id)] = (b ? v : flnan)

/*************************************************************************************************/
namespace {
	private enum class DM : unsigned int {
		BowDirection, TrackDirection, FlowPressureAngle, LineDirection, Speed, TurnRate,
		BowDraught, SternDraught, AverageDraught,
		PSFlow, SBFlow, PSConcentration, SBConcentration, AverageDensity,
		Capacity, Payload, Displacement, EarthWork,
		PSProduct, SBProduct,
		MudDensity,
		_
	};

	private class DMProvider final : public PLCConfirmation, public GPSReceiver {
	public:
		DMProvider(IMRMaster* plc) {
			plc->push_confirmation_receiver(this);

			this->gps1 = moxa_tcp_as_gps(MOXA_TCP::MRIT_DGPS, this);
			this->gps2 = moxa_tcp_as_gps(MOXA_TCP::DP_DGPS, this);
			this->gyro = moxa_tcp_as_gps(MOXA_TCP::GYRO, this);
		}

	public:
		void on_analog_input(long long timepoint_ms, const uint8* DB2, size_t count2, const uint8* DB203, size_t count203, Syslog* logger) override {
			SET_METRICS(this->metrics, DM::BowDraught, DBD(DB2, fixed_bow_draught), 2);
			SET_METRICS(this->metrics, DM::SternDraught, DBD(DB2, fixed_stern_draught), 2);
			SET_METRICS(this->metrics, DM::AverageDraught, DBD(DB2, average_draught), 2);

			SET_METRICS(this->metrics, DM::PSFlow, DBD(DB2, ps_flow), 0);
			SET_METRICS(this->metrics, DM::SBFlow, DBD(DB2, sb_flow), 0);
			SET_METRICS(this->metrics, DM::PSConcentration, DBD(DB2, ps_concentration), 2);
			SET_METRICS(this->metrics, DM::SBConcentration, DBD(DB2, sb_concentration), 2);
			SET_METRICS(this->metrics, DM::PSProduct, DBD(DB2, ps_accumulated_product), 0);
			SET_METRICS(this->metrics, DM::SBProduct, DBD(DB2, sb_accumulated_product), 0);
			SET_METRICS(this->metrics, DM::MudDensity, DBD(DB2, mud_density), 2);
			SET_METRICS(this->metrics, DM::AverageDensity, DBD(DB2, average_density), 2);

			SET_METRICS(this->metrics, DM::EarthWork, DBD(DB2, earthwork_value), 0);
			SET_METRICS(this->metrics, DM::Capacity, DBD(DB2, vessel_value), 0);
			SET_METRICS(this->metrics, DM::Displacement, DBD(DB2, displacement_value), 0);
			SET_METRICS(this->metrics, DM::Payload, DBD(DB2, payload_value), 0);
		}

	public:
		void on_VTG(int id, long long timepoint_ms, VTG* vtg, Syslog* logger) override {
			double angle = this->metrics[_I(DM::BowDirection)] - vtg->track_deg;

			SET_METRICS(this->metrics, DM::Speed, vtg->s_kn, 1);
			SET_METRICS(this->metrics, DM::TrackDirection, vtg->track_deg, 1);
			SET_METRICS(this->metrics, DM::FlowPressureAngle, angle, 1);
		}

		void on_HDT(int id, long long timepoint_ms, HDT* hdt, Syslog* logger) override {
			bool valid = false;

			if ((this->gyro != nullptr) && this->gyro->connected()) {
				if (id == this->gyro->device_identity()) {
					valid = true;
				}
			} else {
				valid = true;
			}

			if (valid) {
				SET_METRICS(this->metrics, DM::BowDirection, hdt->heading_deg, 1);
			}
		}

		void on_ROT(int id, long long timepoint_ms, ROT* rot, Syslog* logger) override {
			bool valid = false;

			if (this->gyro->connected()) {
				if (id == this->gyro->device_identity()) {
					valid = true;
				}
			} else {
				valid = true;
			}

			if (valid) {
				SET_VALID_METRICS(this->metrics, DM::TurnRate, rot->validity, rot->degpmin, 1);
			}
		}

		bool available(int id) override {
			return ((id == this->gyro->device_identity())
				|| (id == this->gps1->device_identity())
				|| (!this->gps1->connected()));
		}

	public:
		double metrics[_N(DM)];

	private: // never deletes these objects manually
		IGPS* gps1;
		IGPS* gps2;
		IGPS* gyro;
	};
}

/*************************************************************************************************/
static DMProvider* provider = nullptr;

DredgeMetrics::DredgeMetrics(MRMaster* plc) {
	if (provider == nullptr) {
		provider = new DMProvider(plc);
		plc->push_confirmation_receiver(provider);
	}
}

unsigned int DredgeMetrics::capacity() {
	return _N(DM);
}

Platform::String^ DredgeMetrics::label_ref(unsigned int idx) {
	return _speak(_E(DM, idx));
}

double DredgeMetrics::value_ref(unsigned int idx) {
	return provider->metrics[_I(idx)];
}

CanvasSolidColorBrush^ DredgeMetrics::label_color_ref(unsigned int idx) {
	CanvasSolidColorBrush^ color = Colours::Foreground;

	switch (_E(DM, idx)) {
	case DM::BowDirection: case DM::TrackDirection: case DM::FlowPressureAngle: case DM::LineDirection: color = Colours::Yellow; break;
	case DM::Speed: case DM::TurnRate: color = Colours::Green; break;
	case DM::BowDraught: case DM::SternDraught: case DM::AverageDraught: color = Colours::Orange; break;
	case DM::PSFlow: case DM::SBFlow: case DM::PSConcentration: case DM::SBConcentration: case DM::AverageDensity: color = Colours::DodgerBlue; break;
	case DM::EarthWork: case DM::Capacity: case DM::Displacement: case DM::MudDensity: case DM::Payload: color = Colours::Salmon; break;
	case DM::PSProduct: case DM::SBProduct: color = Colours::Cyan; break;
	}

	return color;
}
