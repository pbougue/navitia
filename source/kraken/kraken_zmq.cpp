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

#include "conf.h"
#include "type/type.pb.h"
#include <google/protobuf/descriptor.h>

#include <boost/thread.hpp>
#include <functional>
#include <string>
#include <iostream>
#include "utils/init.h"
#include "kraken_zmq.h"
#include "utils/zmq_compat.h"


int main(int argn, char** argv){
    navitia::init_app();
    navitia::kraken::Configuration conf;

    std::string::size_type posSlash = std::string(argv[0]).find_last_of( "\\/" );
    std::string application = std::string(argv[0]).substr(posSlash+1);
    std::string path;
    char buf[256];
    if(getcwd(buf, sizeof(buf))){
        path = std::string(buf) + "/";
    }else{
        perror("getcwd");
        return 1;
    }
    std::string conf_file;
    if(argn > 1){
        // The first argument is the path to the configuration file
        conf_file = argv[1];
    }else{
        conf_file = path + application + ".ini";
    }
    conf.load(conf_file);
    init_logger(conf_file);

    DataManager<navitia::type::Data> data_manager;

    auto logger = log4cplus::Logger::getInstance("startup");
    LOG4CPLUS_INFO(logger, "starting kraken: " << navitia::config::project_version);
    boost::thread_group threads;
    // Prepare our context and sockets
    zmq::context_t context(1);
    zmq::socket_t clients(context, ZMQ_ROUTER);
    std::string zmq_socket = conf.zmq_socket_path();
    zmq::socket_t workers(context, ZMQ_DEALER);
    // Catch startup exceptions; without this, startup errors are on stdout
    try{
        clients.bind(zmq_socket.c_str());
        workers.bind("inproc://workers");
    }catch(zmq::error_t& e){
        LOG4CPLUS_ERROR(logger, "zmq::socket_t::bind() failure: " << e.what());
        return 1;
    }

    threads.create_thread(navitia::MaintenanceWorker(data_manager, conf));

    int nb_threads = conf.nb_threads();
    // Launch pool of worker threads
    LOG4CPLUS_INFO(logger, "starting workers threads");
    for(int thread_nbr = 0; thread_nbr < nb_threads; ++thread_nbr) {
        threads.create_thread(std::bind(&doWork, std::ref(context), std::ref(data_manager), conf));
    }

    // Connect work threads to client threads via a queue
    do{
        try{
            zmq::device(ZMQ_QUEUE, clients, workers);
        }catch(zmq::error_t){}//lors d'un SIGHUP on restore la queue
    }while(true);
}

