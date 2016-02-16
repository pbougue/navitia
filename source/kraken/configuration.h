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

#pragma once
#include <boost/program_options.hpp>
#include <boost/optional.hpp>

namespace navitia { namespace kraken{

    class Configuration{
            boost::program_options::variables_map vm;
        public:
            void load(const std::string& file);
            std::vector<std::string> load_from_command_line(const boost::program_options::options_description&,
                                        int argc, const char* const argv[]);

            std::string databases_path() const;
            std::string zmq_socket_path() const;
            std::string instance_name() const;
            boost::optional<std::string> chaos_database() const;
            int nb_threads() const;

            std::string broker_host() const;
            int broker_port() const;
            std::string broker_username() const;
            std::string broker_password() const;
            std::string broker_vhost() const;
            std::string broker_exchange() const;
            int broker_timeout() const;
            int broker_sleeptime() const;
            bool is_realtime_enabled() const;
            int kirin_timeout() const;
            int kirin_retry_timeout() const;
            bool display_contributors() const;
            size_t raptor_cache_size() const;

            std::vector<std::string> rt_topics() const;
    };

    boost::program_options::options_description get_options_description(
            const boost::optional<std::string> name = {},
            const boost::optional<std::string> zmq = {},
            const boost::optional<bool> display_contributors = {});

}}//namespace
