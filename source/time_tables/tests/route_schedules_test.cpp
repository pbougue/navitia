/* Copyright © 2001-2014, Canal TP and/or its affiliates. All rights reserved.
  
This file is part of Navitia,
    the software to build cool stuff with public transport.
 
Hope you'll enjoy and contribute to this project,
    powered by Canal TP (www.canaltp.fr).
Help us simplify mobility and open public transport:
    a non ending quest to the responsive locomotion way of traveling!
  
LICENCE: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
   
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.
   
You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
  
Stay tuned using
twitter @navitia 
IRC #navitia on freenode
https://groups.google.com/d/forum/navitia
www.navitia.io
*/

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE test_ed
#include <boost/test/unit_test.hpp>
#include "ed/build_helper.h"
#include "type/type.h"
#include "tests/utils_test.h"
#include "time_tables/route_schedules.h"
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/sort.hpp>

namespace ntt = navitia::timetables;

struct logger_initialized {
    logger_initialized()   { init_logger(); }
};
BOOST_GLOBAL_FIXTURE( logger_initialized )

static const std::string& get_vj(const pbnavitia::RouteSchedule& r, int i) {
    return r.table().headers(i).pt_display_informations().uris().vehicle_journey();
}

static void print_route_schedule(const pbnavitia::RouteSchedule& r) {
    std::cout << "\t|";
    for (const auto& row: r.table().headers()) {
        std::cout << row.pt_display_informations().uris().vehicle_journey() << "\t|";
    }
    std::cout << std::endl;
    for (const auto& row: r.table().rows()) {
        std::cout << row.stop_point().uri() << "\t|";
        for (const auto& elt: row.date_times()) {
            if (elt.time() > 48 * 3600) {
                std::cout << "-" << "\t|";
            } else {
                std::cout << elt.time() / 3600 << ":" << (elt.time() % 3600) / 60. << "\t|";
            }
        }
        std::cout << std::endl;
    }
}

//for more concice test
static pt::ptime d(std::string str) {
    return boost::posix_time::from_iso_string(str);
}
/*
    Wanted schedules:
            VJ1    VJ2    VJ3    VJ4
        A          8h00   8h05
        B          8h10   8h20   8h25
        C   8h05
        D   9h30                 9h35

        But the thermometer algorithm is buggy, is doesn't give the shorest common superstring..
        So, actually we have this schedule
            VJ2    VJ1    VJ3    VJ4
        C          8h05
        D          9h30
        A   8h00          8h05
        B   8h10          8h20   8h25
        D                        9h35

With VJ1, and VJ2, we ensure that we are no more comparing the two first jpp of
each VJ, but we have a more human order.
VJ2 and VJ3, is a basic test.
The difficulty with VJ4 comes when we have to compare it to VJ1.
When comparing VJ1 and VJ4, we compare VJ1(stopC) with VJ4(stopD)
and when we compare VJ4 and VJ1, we compare VJ4(stopB) with VJ1(stopC).
*/
struct route_schedule_fixture {
    ed::builder b = {"20120614"};

    route_schedule_fixture() {
        const std::string a_name = "stopA",
                          b_name = "stopB",
                          c_name = "stopC",
                          d_name = "stopD";
        b.vj("A", "1111111", "", true, "1", "1")(c_name, 8*3600 + 5*60)(d_name, 9*3600 + 30*60);
        b.vj("A", "1111111", "", true, "2", "2")(a_name, 8*3600)(b_name, 8*3600 + 10*60);
        b.vj("A", "1111111", "", true, "3", "3")(a_name, 8*3600 + 5*60)(b_name, 8*3600 + 20*60);
        b.vj("A", "1111111", "", true, "4", "4")(b_name, 8*3600+25*60)(d_name, 9*3600 + 35*60);
        b.finish();
        b.data->pt_data->index();
        b.data->build_raptor();

        boost::gregorian::date begin = boost::gregorian::date_from_iso_string("20120613");
        boost::gregorian::date end = boost::gregorian::date_from_iso_string("20120630");

        b.data->meta->production_date = boost::gregorian::date_period(begin, end);
    }
};

BOOST_FIXTURE_TEST_CASE(test1, route_schedule_fixture) {

    navitia::PbCreator pb_creator(*b.data, bt::second_clock::universal_time(), null_time_period, false);
    navitia::timetables::route_schedule(pb_creator, "line.uri=A", {}, {}, d("20120615T070000"), 86400, 100,
                                                                   3, 10, 0, nt::RTLevel::Base);
    pbnavitia::Response resp = pb_creator.get_response();
    BOOST_REQUIRE_EQUAL(resp.route_schedules().size(), 1);
    pbnavitia::RouteSchedule route_schedule = resp.route_schedules(0);
    print_route_schedule(route_schedule);
    BOOST_REQUIRE_EQUAL(get_vj(route_schedule, 0), "2");
    BOOST_REQUIRE_EQUAL(get_vj(route_schedule, 1), "1");
    BOOST_REQUIRE_EQUAL(get_vj(route_schedule, 2), "3");
    BOOST_REQUIRE_EQUAL(get_vj(route_schedule, 3), "4");
}


BOOST_FIXTURE_TEST_CASE(test_max_nb_stop_times, route_schedule_fixture) {

    navitia::PbCreator pb_creator(*b.data, bt::second_clock::universal_time(), null_time_period, false);
    navitia::timetables::route_schedule(pb_creator, "line.uri=A", {}, {}, d("20120615T070000"), 86400, 0,
                                                                   3, 10, 0, nt::RTLevel::Base);
    pbnavitia::Response resp = pb_creator.get_response();
    BOOST_REQUIRE_EQUAL(resp.route_schedules().size(), 1);
    pbnavitia::RouteSchedule route_schedule = resp.route_schedules(0);
    print_route_schedule(route_schedule);
    BOOST_REQUIRE_EQUAL(route_schedule.table().headers().size(), 0);
}

/*
We have 3 vehicle journeys VJ5, VJ6, VJ7 and 3 stops S1, S2, S3 :
     VJ5     VJ6     VJ7
S1  10:00   11:00   13:00
S2  10:30   11:30   13:37
s3  11:00   12:00   14:00

We have 4 calendars, C1, C2, C3, C4 :
- C1 : week (monday to friday),
- C2 : week end,
- C3 : holiday (monday to sunday during holidays),
- C4 : random calendar nobody uses.

Each vehicle_journey has its respective meta_vj : MVJ5, MVJ6, MVJ7.
We have the following associations :
- MVJ5 => C1,C2
- MVJ6 => C2,C3
- MVJ7 => ∅
*/
struct route_schedule_calendar_fixture {
    // we complicate things a bit, we say that the vjs have a utc offset
    ed::builder b = {"20120614", "canal tp", "paris", {{"02:00"_t, {{"20120614"_d, "20130614"_d}}}}};
    navitia::type::Calendar *c1, *c2, *c3, *c4;
    navitia::type::VehicleJourney *vj5, *vj6, *vj7;

    route_schedule_calendar_fixture() {
        const std::string S1_name = "S1",
                          S2_name = "S2",
                          S3_name = "S3";
        boost::gregorian::date begin = boost::gregorian::date_from_iso_string("20120613");
        boost::gregorian::date end = boost::gregorian::date_from_iso_string("20120630");

        vj5 = b.vj("B", "1111111", "", true, "VJ5", "MVJ5")
                (S1_name, "10:00"_t)(S2_name, "10:30"_t)(S3_name, "11:00"_t).make();
        vj6 = b.vj("B", "1111111", "", true, "VJ6", "MVJ6")
                (S1_name, "11:00"_t)(S2_name, "11:30"_t)(S3_name, "12:00"_t).make();
        vj7 = b.vj("B", "1111111", "", true, "VJ7", "MVJ7")
                (S1_name, "13:00"_t)(S2_name, "13:37"_t)(S3_name, "14:00"_t).make();

        auto save_cal = [&](navitia::type::Calendar* cal) {
            b.data->pt_data->calendars.push_back(cal);
            b.data->pt_data->calendars_map[cal->uri] = cal;
        };

        c1 = new navitia::type::Calendar(begin);
        c1->uri = "C1";
        c1->active_periods.push_back({begin, end});
        c1->week_pattern = std::bitset<7>("1111100");
        save_cal(c1);
        c2 = new navitia::type::Calendar(begin);
        c2->uri = "C2";
        c2->active_periods.push_back({begin, end});
        c2->week_pattern = std::bitset<7>("0000011");
        save_cal(c2);
        c3 = new navitia::type::Calendar(begin);
        c3->uri = "C3";
        c3->active_periods.push_back({begin, end});
        c3->week_pattern = std::bitset<7>("1111111");
        save_cal(c3);
        c4 = new navitia::type::Calendar(begin);
        c4->uri = "C4";
        c4->active_periods.push_back({begin, end});
        c4->week_pattern = std::bitset<7>("0000000");
        save_cal(c4);

        auto a1 = new navitia::type::AssociatedCalendar;
        a1->calendar = c1;
        b.data->pt_data->associated_calendars.push_back(a1);
        auto a2 = new navitia::type::AssociatedCalendar;
        a2->calendar = c2;
        b.data->pt_data->associated_calendars.push_back(a2);
        auto a3 = new navitia::type::AssociatedCalendar;
        a3->calendar = c3;
        b.data->pt_data->associated_calendars.push_back(a3);

        b.data->pt_data->meta_vjs.get_mut("MVJ5")->associated_calendars.insert({c1->uri, a1});
        b.data->pt_data->meta_vjs.get_mut("MVJ5")->associated_calendars.insert({c2->uri, a2});
        b.data->pt_data->meta_vjs.get_mut("MVJ6")->associated_calendars.insert({c2->uri, a2});
        b.data->pt_data->meta_vjs.get_mut("MVJ6")->associated_calendars.insert({c3->uri, a3});

        b.finish();
        b.data->pt_data->index();
        b.data->build_raptor();

        b.data->meta->production_date = boost::gregorian::date_period(begin, end);
    }

    void check_calendar_results(boost::optional<const std::string> calendar, std::vector<std::string> expected_vjs) {

        navitia::PbCreator pb_creator(*(b.data), bt::second_clock::universal_time(), null_time_period, false);
        navitia::timetables::route_schedule(pb_creator, "line.uri=B", calendar, {},
                                            d("20120615T070000"), 86400, 100,
                                            3, 10, 0, nt::RTLevel::Base);

        pbnavitia::Response resp = pb_creator.get_response();
        BOOST_REQUIRE_EQUAL(resp.route_schedules().size(), 1);
        pbnavitia::RouteSchedule route_schedule = resp.route_schedules(0);
        print_route_schedule(route_schedule);
        BOOST_REQUIRE_EQUAL(route_schedule.table().headers_size(), expected_vjs.size());
        for(int i = 0 ; i < route_schedule.table().headers_size() ; i++) {
            BOOST_REQUIRE_EQUAL(get_vj(route_schedule, i), expected_vjs[i]);
        }
    }
};

BOOST_FIXTURE_TEST_CASE(test_calendar_filter, route_schedule_calendar_fixture) {
    // No filter, all 3 VJs expected
    check_calendar_results({}, {vj5->uri, vj6->uri, vj7->uri});
    // For each calendar only the VJs explicitly linked to it expected
    check_calendar_results(c1->uri, {vj5->uri});
    check_calendar_results(c2->uri, {vj5->uri, vj6->uri});
    check_calendar_results(c3->uri, {vj6->uri});
    // No results for calendar C4
    check_calendar_results(c4->uri, {});
}

namespace ba = boost::adaptors;
using vec_dt = std::vector<navitia::DateTime>;
static navitia::DateTime get_dt(const navitia::routing::datetime_stop_time& p) { return p.first; }

/*
 * Test get_all_route_stop_times with a calendar
 *
 * wich the C2 calendar, we should have the vj5 and vj6, thus we should have this schedule:
 *        VJ5     VJ6
 *    S1  10:00   11:00
 *    S2  10:30   11:30
 *    S3  11:00   12:00
 * NOTE: this is in UTC and since we ask with a calendar we want local time
 * Thus the schedule is:
 *        VJ5     VJ6
 *    S1  12:00   13:00
 *    S2  12:30   13:30
 *    S3  13:00   14:00
*/
BOOST_FIXTURE_TEST_CASE(test_get_all_route_stop_times_with_cal, route_schedule_calendar_fixture) {
    const auto* route = b.data->pt_data->routes_map.at("B:0");

    auto res = navitia::timetables::get_all_route_stop_times(route,
                                                             "00:00"_t,
                                                             "00:00"_t + "24:00"_t,
                                                             std::numeric_limits<size_t>::max(),
                                                             *b.data, nt::RTLevel::Base,
                                                             boost::optional<const std::string>(c2->uri));

    BOOST_REQUIRE_EQUAL(res.size(), 2);
    boost::sort(res);
    BOOST_CHECK_EQUAL_RANGE(res[0] | ba::transformed(get_dt), vec_dt({"12:00"_t, "12:30"_t, "13:00"_t}));
    BOOST_CHECK_EQUAL_RANGE(res[1] | ba::transformed(get_dt), vec_dt({"13:00"_t, "13:30"_t, "14:00"_t}));
}

/*
 * Test get_all_route_stop_times with a calendar and with a custom time (used only for the sort)
 * Schedule should be
 *        VJ5        VJ6
 *    S1  12:00+1   13:00
 *    S2  12:30+1   13:30
 *    s3  13:00+1   14:00
 *
 * the +1 day is used so the following sort will sort the result in a correct way
 */
BOOST_FIXTURE_TEST_CASE(test_get_all_route_stop_times_with_cal_and_time, route_schedule_calendar_fixture) {
    const auto* route = b.data->pt_data->routes_map.at("B:0");

    auto res = navitia::timetables::get_all_route_stop_times(route,
                                                             "12:37"_t,
                                                             "12:37"_t + "24:00"_t,
                                                             std::numeric_limits<size_t>::max(),
                                                             *b.data, nt::RTLevel::Base,
                                                             boost::optional<const std::string>(c2->uri));

    BOOST_REQUIRE_EQUAL(res.size(), 2);

    auto one_day = "24:00"_t;
    boost::sort(res);
    BOOST_CHECK_EQUAL_RANGE(res[0] | ba::transformed(get_dt),
            vec_dt({"13:00"_t, "13:30"_t, "14:00"_t}));
    BOOST_CHECK_EQUAL_RANGE(res[1] | ba::transformed(get_dt),
            vec_dt({"12:00"_t + one_day, "12:30"_t + one_day, "13:00"_t + one_day}));
}

/*
 * Test get_all_route_stop_times with not a calendar but only a dt (the classic route_schedule)
 *
 * Note: the returned dt should be in UTC, not local time
 *
 * thus the schedule after 11h37 should be:
 *
 *      VJ5       VJ6       VJ7
 * S1  10:00+1   11:00+1   13:00
 * S2  10:30+1   11:30+1   13:37
 * s3  11:00+1   12:00+1   14:00
 *
 */
BOOST_FIXTURE_TEST_CASE(test_get_all_route_stop_times_with_time, route_schedule_calendar_fixture) {
    const auto* route = b.data->pt_data->routes_map.at("B:0");

    auto res = navitia::timetables::get_all_route_stop_times(route,
                                                             "11:37"_t,
                                                             "11:37"_t + "24:00"_t,
                                                             std::numeric_limits<size_t>::max(),
                                                             *b.data, nt::RTLevel::Base, {});

    BOOST_REQUIRE_EQUAL(res.size(), 3);

    auto one_day = "24:00"_t;
    boost::sort(res);
    BOOST_CHECK_EQUAL_RANGE(res[0] | ba::transformed(get_dt),
            vec_dt({"13:00"_t, "13:37"_t, "14:00"_t}));
    BOOST_CHECK_EQUAL_RANGE(res[1] | ba::transformed(get_dt),
            vec_dt({"10:00"_t + one_day, "10:30"_t + one_day, "11:00"_t + one_day}));
    BOOST_CHECK_EQUAL_RANGE(res[2] | ba::transformed(get_dt),
            vec_dt({"11:00"_t + one_day, "11:30"_t + one_day, "12:00"_t + one_day}));
}

/*
 * Test with a dataset close to https://github.com/CanalTP/navitia/issues/1161
 *
 * The dataset in LOCAL TIME (france) is:
 *
 *      vj1    vj2    vj3
 * S1  00:50  01:50  02:50
 * S2  01:05  02:05  03:05
 * S3  01:15  02:15  03:15
 *
 * The small catch is that there is 2 hours UTC shift, thus vj1 and vj2 validity pattern's are shifted the day before:
 *
 *      vj1    vj2    vj3
 * S1  22:50  23:50  00:50
 * S2  23:05  00:05  01:05
 * S3  23:15  00:15  01:15
 *
 */
struct CalWithDSTFixture {
    ed::builder b = {"20150614", "canal tp", "paris", {{"02:00"_t, {{"20150614"_d, "20160614"_d}}}}};

    CalWithDSTFixture() {
        auto normal_vp = "111111";
        auto shifted_vp = "111110";
        b.vj("B", shifted_vp)("S1", "22:50"_t)("S2", "23:05"_t)("S3", "23:15"_t);
        b.vj("B", shifted_vp)("S1", "23:50"_t)("S2", "00:05"_t)("S3", "00:15"_t);
        b.vj("B", normal_vp)("S1", "00:50"_t)("S2", "01:05"_t)("S3", "01:15"_t);

        auto cal = new navitia::type::Calendar();
        cal->uri = "cal";
        b.data->pt_data->calendars.push_back(cal);
        b.data->pt_data->calendars_map[cal->uri] = cal;

        auto a1 = new navitia::type::AssociatedCalendar;
        a1->calendar = cal;
        b.data->pt_data->associated_calendars.push_back(a1);

        for (auto& mvj: b.data->pt_data->meta_vjs) {
            mvj->associated_calendars.insert({cal->uri, a1});
        }

        b.finish();
        b.data->pt_data->index();
        b.data->build_raptor();
    }
};

BOOST_FIXTURE_TEST_CASE(test_get_all_route_stop_times_with_different_vp, CalWithDSTFixture) {
    const auto* route = b.data->pt_data->routes.at(0);

    auto res = navitia::timetables::get_all_route_stop_times(route,
                                                             "00:00"_t,
                                                             "00:00"_t + "24:00"_t,
                                                             std::numeric_limits<size_t>::max(),
                                                             *b.data, nt::RTLevel::Base, std::string("cal"));

    BOOST_REQUIRE_EQUAL(res.size(), 3);

    boost::sort(res);
    BOOST_CHECK_EQUAL_RANGE(res[0] | ba::transformed(get_dt), vec_dt({"00:50"_t, "01:05"_t, "01:15"_t}));
    BOOST_CHECK_EQUAL_RANGE(res[1] | ba::transformed(get_dt), vec_dt({"01:50"_t, "02:05"_t, "02:15"_t}));
    BOOST_CHECK_EQUAL_RANGE(res[2] | ba::transformed(get_dt), vec_dt({"02:50"_t, "03:05"_t, "03:15"_t}));
}

BOOST_FIXTURE_TEST_CASE(test_get_all_route_stop_times_with_different_vp_and_hour, CalWithDSTFixture) {
    const auto* route = b.data->pt_data->routes.at(0);

    auto res = navitia::timetables::get_all_route_stop_times(route,
                                                             "01:00"_t,
                                                             "01:00"_t + "24:00"_t,
                                                             std::numeric_limits<size_t>::max(),
                                                             *b.data, nt::RTLevel::Base, std::string("cal"));

    BOOST_REQUIRE_EQUAL(res.size(), 3);

    auto one_day = "24:00"_t;
    boost::sort(res);
    //both are after the asked time (1h), so they are on the following day
    BOOST_CHECK_EQUAL_RANGE(res[0] | ba::transformed(get_dt), vec_dt({"01:50"_t, "02:05"_t, "02:15"_t}));
    BOOST_CHECK_EQUAL_RANGE(res[1] | ba::transformed(get_dt), vec_dt({"02:50"_t, "03:05"_t, "03:15"_t}));
    BOOST_CHECK_EQUAL_RANGE(res[2] | ba::transformed(get_dt),
            vec_dt({"00:50"_t + one_day, "01:05"_t + one_day, "01:15"_t + one_day}));
}

// We want: (for a end of service at 2h00)
//      A      B      C
// S1 23:30  23:50  00:10
// S2 23:40  00:00  00:20
// S3 23:50  00:10  00:30
//
// Detail in associated PR https://github.com/CanalTP/navitia/pull/1304
BOOST_AUTO_TEST_CASE(test_route_schedule_with_different_vp_over_midnight) {
    ed::builder b = {"20151127"};
    navitia::type::Calendar *c1;

    boost::gregorian::date begin = boost::gregorian::date_from_iso_string("20150101");
    boost::gregorian::date end = boost::gregorian::date_from_iso_string("20160101");

    b.vj("L", "111111", "", true, "A", "A")
        ("st1", "23:30"_t)
        ("st2", "23:40"_t)
        ("st3", "23:50"_t);
    b.vj("L", "111111", "", true, "B", "B")
        ("st1", "23:50"_t)
        ("st2", "24:00"_t)
        ("st3", "24:10"_t);
    b.vj("L", "111110", "", true, "C", "C")
        ("st1", "24:10"_t)
        ("st2", "24:20"_t)
        ("st3", "24:30"_t);

    auto save_cal = [&](navitia::type::Calendar* cal) {
        b.data->pt_data->calendars.push_back(cal);
        b.data->pt_data->calendars_map[cal->uri] = cal;
    };

    c1 = new navitia::type::Calendar(begin);
    c1->uri = "C1";
    c1->active_periods.push_back({begin, end});
    c1->week_pattern = std::bitset<7>("1111111");
    save_cal(c1);

    auto a1 = new navitia::type::AssociatedCalendar;
    a1->calendar = c1;
    b.data->pt_data->associated_calendars.push_back(a1);

    b.data->pt_data->meta_vjs.get_mut("A")->associated_calendars.insert({c1->uri, a1});
    b.data->pt_data->meta_vjs.get_mut("B")->associated_calendars.insert({c1->uri, a1});
    b.data->pt_data->meta_vjs.get_mut("C")->associated_calendars.insert({c1->uri, a1});

    b.finish();
    b.data->pt_data->index();
    b.data->build_raptor();

    navitia::PbCreator pb_creator(*(b.data), bt::second_clock::universal_time(), null_time_period, false);

    navitia::timetables::route_schedule(pb_creator, "line.uri=L", c1->uri, {}, d("20151201T020000"), 86400, 100,
                                        3, 10, 0, nt::RTLevel::Base);
    pbnavitia::Response resp = pb_creator.get_response();
    BOOST_REQUIRE_EQUAL(resp.route_schedules().size(), 1);
    pbnavitia::RouteSchedule route_schedule = resp.route_schedules(0);
    print_route_schedule(route_schedule);
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 0), "A");
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 1), "B");
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 2), "C");
}

// We want:
//     C A B
// st1 2   1
// st2     3
// st3     5
// st4 3 5 6
// st5 4 6 7
// st6 5 7 8
BOOST_AUTO_TEST_CASE(complicated_order_1) {
    ed::builder b = {"20120614"};
    b.vj("L", "1111111", "", true, "A", "A")
        ("st4", "5:00"_t)
        ("st5", "6:00"_t)
        ("st6", "7:00"_t);
    b.vj("L", "1111111", "", true, "B", "B")
        ("st1", "1:00"_t)
        ("st2", "3:00"_t)
        ("st3", "5:00"_t)
        ("st4", "6:00"_t)
        ("st5", "7:00"_t)
        ("st6", "8:00"_t);
    b.vj("L", "1111111", "", true, "C", "C")
        ("st1", "2:00"_t)
        ("st4", "3:00"_t)
        ("st5", "4:00"_t)
        ("st6", "5:00"_t);

    b.finish();
    b.data->pt_data->index();
    b.data->build_raptor();

    navitia::PbCreator pb_creator(*(b.data), bt::second_clock::universal_time(), null_time_period, false);
    navitia::timetables::route_schedule(pb_creator, "line.uri=L", {}, {}, d("20120615T000000"), 86400, 100,
                                        3, 10, 0, nt::RTLevel::Base);
    pbnavitia::Response resp = pb_creator.get_response();
    BOOST_REQUIRE_EQUAL(resp.route_schedules().size(), 1);
    pbnavitia::RouteSchedule route_schedule = resp.route_schedules(0);
    print_route_schedule(route_schedule);
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 0), "C");
    BOOST_CHECK_EQUAL(route_schedule.table().rows(3).date_times(0).time(), "3:00"_t);
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 1), "A");
    BOOST_CHECK_EQUAL(route_schedule.table().rows(3).date_times(1).time(), "5:00"_t);
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 2), "B");
    BOOST_CHECK_EQUAL(route_schedule.table().rows(3).date_times(2).time(), "6:00"_t);
}

// We want:
//      C  A B
// st1     1 2
// st2     2 3
// st3     3 4
// st4     5
// st5     7
// st6  8  9 7
// st7  9 10
// st8 10 11
BOOST_AUTO_TEST_CASE(complicated_order_2) {
    ed::builder b = {"20120614"};
    b.vj("L", "1111111", "", true, "A", "A")
        ("st1", "1:00"_t)
        ("st2", "2:00"_t)
        ("st3", "3:00"_t)
        ("st4", "5:00"_t)
        ("st5", "7:00"_t)
        ("st6", "9:00"_t)
        ("st7", "10:00"_t)
        ("st8", "11:00"_t);
    b.vj("L", "1111111", "", true, "B", "B")
        ("st1", "2:00"_t)
        ("st2", "3:00"_t)
        ("st3", "4:00"_t)
        ("st6", "7:00"_t);
    b.vj("L", "1111111", "", true, "C", "C")
        ("st6", "8:00"_t)
        ("st7", "9:00"_t)
        ("st8", "10:00"_t);

    b.finish();
    b.data->pt_data->index();
    b.data->build_raptor();

    navitia::PbCreator pb_creator(*(b.data), bt::second_clock::universal_time(), null_time_period, false);
    navitia::timetables::route_schedule(pb_creator, "line.uri=L", {}, {}, d("20120615T000000"), 86400, 100,
                                        3, 10, 0, nt::RTLevel::Base);
    pbnavitia::Response resp = pb_creator.get_response();
    BOOST_REQUIRE_EQUAL(resp.route_schedules().size(), 1);
    pbnavitia::RouteSchedule route_schedule = resp.route_schedules(0);
    print_route_schedule(route_schedule);
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 0), "C");
    BOOST_CHECK_EQUAL(route_schedule.table().rows(5).date_times(0).time(), "8:00"_t);
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 1), "A");
    BOOST_CHECK_EQUAL(route_schedule.table().rows(5).date_times(1).time(), "9:00"_t);
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 2), "B");
    BOOST_CHECK_EQUAL(route_schedule.table().rows(5).date_times(2).time(), "7:00"_t);
}

// We want:
//     A B C D E
// st1   1 2 3 4
// st2   2 3 4 5
// st3 6 7
// st4 7 8
BOOST_AUTO_TEST_CASE(complicated_order_3) {
    ed::builder b = {"20120614"};
    b.vj("L", "1111111", "", true, "A", "A")
        ("st3", "6:00"_t)
        ("st4", "7:00"_t);
    b.vj("L", "1111111", "", true, "B", "B")
        ("st1", "1:00"_t)
        ("st2", "2:00"_t)
        ("st3", "7:00"_t)
        ("st4", "8:00"_t);
    b.vj("L", "1111111", "", true, "C", "C")
        ("st1", "2:00"_t)
        ("st2", "3:00"_t);
    b.vj("L", "1111111", "", true, "D", "D")
        ("st1", "3:00"_t)
        ("st2", "4:00"_t);
    b.vj("L", "1111111", "", true, "E", "E")
        ("st1", "4:00"_t)
        ("st2", "5:00"_t);

    b.finish();
    b.data->pt_data->index();
    b.data->build_raptor();

    navitia::PbCreator pb_creator(*(b.data), bt::second_clock::universal_time(), null_time_period, false);
    navitia::timetables::route_schedule(pb_creator, "line.uri=L", {}, {}, d("20120615T000000"), 86400, 100,
                                        3, 10, 0, nt::RTLevel::Base);

    pbnavitia::Response resp = pb_creator.get_response();
    BOOST_REQUIRE_EQUAL(resp.route_schedules().size(), 1);
    pbnavitia::RouteSchedule route_schedule = resp.route_schedules(0);
    print_route_schedule(route_schedule);
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 0), "A");
    BOOST_CHECK_EQUAL(route_schedule.table().rows(2).date_times(0).time(), "6:00"_t);
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 1), "B");
    BOOST_CHECK_EQUAL(route_schedule.table().rows(0).date_times(1).time(), "1:00"_t);
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 2), "C");
    BOOST_CHECK_EQUAL(route_schedule.table().rows(0).date_times(2).time(), "2:00"_t);
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 3), "D");
    BOOST_CHECK_EQUAL(route_schedule.table().rows(0).date_times(3).time(), "3:00"_t);
    BOOST_CHECK_EQUAL(get_vj(route_schedule, 4), "E");
    BOOST_CHECK_EQUAL(route_schedule.table().rows(0).date_times(4).time(), "4:00"_t);
}
