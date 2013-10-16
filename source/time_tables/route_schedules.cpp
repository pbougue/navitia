#include "route_schedules.h"
#include "thermometer.h"
#include "request_handle.h"
#include "type/pb_converter.h"
#include "ptreferential/ptreferential.h"
#include "utils/paginate.h"
#include "type/datetime.h"

namespace pt = boost::posix_time;

namespace navitia { namespace timetables {

std::vector<std::vector<datetime_stop_time> >
get_all_stop_times(const vector_idx &journey_patterns,
                   const DateTime &dateTime,
                   const DateTime &max_datetime, type::Data &d) {
    std::vector<std::vector<datetime_stop_time> > result;

    //On cherche les premiers journey_pattern_points
    //de tous les journey_patterns
    std::vector<type::idx_t> first_journey_pattern_points;
    for(type::idx_t jp_idx : journey_patterns) {
        auto jpp = d.pt_data.journey_patterns[jp_idx];
        auto first_jpp_idx = jpp->journey_pattern_point_list.front()->idx;
        first_journey_pattern_points.push_back(first_jpp_idx);
    }

    //On fait un best_stop_time sur ces journey_pattern points
    auto first_dt_st = get_stop_times(first_journey_pattern_points,
                                      dateTime, max_datetime,
                                      std::numeric_limits<int>::max(), d);

    //On va chercher tous les prochains horaires
    for(auto ho : first_dt_st) {
        result.push_back(std::vector<datetime_stop_time>());
        DateTime dt = ho.first;
        for(const type::StopTime* stop_time : ho.second->vehicle_journey->stop_time_list) {
            DateTimeUtils::update(dt, stop_time->departure_time);
            result.back().push_back(std::make_pair(dt, stop_time));
        }
    }
    return result;
}


std::vector<std::vector<datetime_stop_time> >
make_matrice(const std::vector<std::vector<datetime_stop_time> >& stop_times,
             Thermometer &thermometer, type::Data &) {
    std::vector<std::vector<datetime_stop_time> > result;
    //On initilise le tableau vide
    for(unsigned int i=0; i<thermometer.get_thermometer().size(); ++i) {
        result.push_back(std::vector<datetime_stop_time>());
        result.back().resize(stop_times.size());
    }

    //On remplit le tableau
    int y=0;
    for(std::vector<datetime_stop_time> vec : stop_times) {
        auto jpp = *vec.front().second->vehicle_journey->journey_pattern;
        std::vector<uint32_t> orders = thermometer.match_journey_pattern(jpp);
        int order = 0;
        for(datetime_stop_time dt_stop_time : vec) {
            result[orders[order]][y] = dt_stop_time;
            ++order;
        }
        ++y;
    }

    return result;
}


std::vector<type::VehicleJourney*>
get_vehicle_jorney(const std::vector<std::vector<datetime_stop_time> >& stop_times){
    std::vector<type::VehicleJourney*> result;
    for(const std::vector<datetime_stop_time> vec : stop_times){
        type::VehicleJourney* vj = vec.front().second->vehicle_journey;
        auto it = std::find_if(result.begin(), result.end(),
                               [vj](type::VehicleJourney* vj1){
                                return (vj->idx == vj1->idx);
                        });
        if(it == result.end()){
            result.push_back(vj);
        }
    }
    return result;
}


pbnavitia::Response
route_schedule(const std::string& filter, const std::string &str_dt,
               uint32_t duration, uint32_t interface_version,
               const uint32_t max_depth, int count, int start_page,
               type::Data &d) {
    RequestHandle handler("ROUTE_SCHEDULE", filter, str_dt, duration, d);

    if(handler.pb_response.has_error()) {
        return handler.pb_response;
    }
    auto now = pt::second_clock::local_time();
    auto pt_datetime = to_posix_time(handler.date_time, d);
    auto pt_max_datetime = to_posix_time(handler.max_datetime, d);
    pt::time_period action_period(pt_datetime, pt_max_datetime);
    Thermometer thermometer(d);
    auto routes_idx = ptref::make_query(type::Type_e::Route, filter, d);
    size_t total_result = routes_idx.size();
    routes_idx = paginate(routes_idx, count, start_page);
    for(type::idx_t route_idx : routes_idx) {
        auto route = d.pt_data.routes[route_idx];
        auto jps = route->get(type::Type_e::JourneyPattern, d.pt_data);
        //On récupère les stop_times
        auto stop_times = get_all_stop_times(jps, handler.date_time,
                                             handler.max_datetime, d);
        std::vector<vector_idx> stop_points;
        for(auto jp_idx : jps) {
            auto jp = d.pt_data.journey_patterns[jp_idx];
            stop_points.push_back(vector_idx());
            for(auto jpp : jp->journey_pattern_point_list) {
                stop_points.back().push_back(jpp->stop_point->idx);
            }
        }
        thermometer.generate_thermometer(stop_points);
        //On génère la matrice
        auto  matrice = make_matrice(stop_times, thermometer, d);
         //On récupère les vehicleJourny de manière unique
        auto vehicle_journy_list = get_vehicle_jorney(stop_times);
        auto schedule = handler.pb_response.add_route_schedules();
        pbnavitia::Table *table = schedule->mutable_table();
        auto m_pt_display_informations = schedule->mutable_pt_display_informations();
        fill_pb_object(route, d, m_pt_display_informations, 0, now, action_period);

        for(type::VehicleJourney* vj : vehicle_journy_list){
            pbnavitia::Header* header = table->add_headers();
            pbnavitia::PtDisplayInfo* vj_display_information = header->mutable_pt_display_informations();
            pbnavitia::addInfoVehicleJourney* add_info_vehicle_journey = header->mutable_add_info_vehicle_journey();
            fill_pb_object(vj, d, vj_display_information, 0, now, action_period);
            fill_pb_object(vj, d, add_info_vehicle_journey, 0, now, action_period);
        }

        for(unsigned int i=0; i < thermometer.get_thermometer().size(); ++i) {
            type::idx_t spidx=thermometer.get_thermometer()[i];
            const type::StopPoint* sp = d.pt_data.stop_points[spidx];
            //version v1
            pbnavitia::RouteScheduleRow* row = table->add_rows();
            fill_pb_object(sp, d, row->mutable_stop_point(), max_depth,
                           now, action_period);
            for(unsigned int j=0; j<stop_times.size(); ++j) {
                datetime_stop_time dt_stop_time  = matrice[i][j];
                if(interface_version == 1) {
                    auto pb_dt = row->add_date_times();
                    fill_pb_object(dt_stop_time.second, d, pb_dt, max_depth,
                                   now, action_period, dt_stop_time.first);
                } else if(interface_version == 0) {
                    row->add_stop_times(navitia::iso_string(dt_stop_time.first, d));
                }
            }
        }
    }
    auto pagination = handler.pb_response.mutable_pagination();
    pagination->set_totalresult(total_result);
    pagination->set_startpage(start_page);
    pagination->set_itemsperpage(count);
    pagination->set_itemsonpage(handler.pb_response.route_schedules_size());
    return handler.pb_response;
}
}}
