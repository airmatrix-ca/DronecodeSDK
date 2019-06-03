/**
 * @file fly_qgc_mission.cpp
 *
 * @brief Demonstrates how to import mission items from QGroundControl plan,
 * and fly them using the Dronecode SDK.
 *
 * Steps to run this example:
 * 1. (a) Create a Mission in QGroundControl and save them to a file (.plan) (OR)
 *    (b) Use a pre-created sample mission plan in "plugins/mission/qgroundcontrol_sample.plan".
 *    Click
 * [here](https://user-images.githubusercontent.com/26615772/31763673-972c5bb6-b4dc-11e7-8ff0-f8b39b6b88c3.png)
 * to see what sample mission plan in QGroundControl looks like.
 * 2. Run the example by passing path of the QGC mission plan as argument (By default, sample
 * mission plan is imported).
 *
 * Example description:
 * 1. Imports QGC mission items from .plan file.
 * 2. Uploads mission items to vehicle.
 * 3. Starts mission from first mission item.
 * 4. Commands RTL once QGC Mission is accomplished.
 *
 * @author Shakthi Prashanth M <shakthi.prashanth.m@intel.com>,
 *         Julian Oes <julian@oes.ch>
 * @date 2018-02-04
 */

#include <dronecode_sdk/dronecode_sdk.h>
#include <dronecode_sdk/plugins/action/action.h>
#include <dronecode_sdk/plugins/mission/mission.h>
#include <dronecode_sdk/plugins/telemetry/telemetry.h>
#include <functional>
#include <stdlib.h>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <ctime>
#include <fstream>
#include <bits/stdc++.h>
#include "gtkmm_buttons.hpp"
#include <gtkmm-3.0/gtkmm.h>
#include <gtkmm-3.0/gtkmm/window.h>
#include <gtkmm-3.0/gtkmm/button.h>
#include <gtkmm-3.0/gtkmm/box.h>
#include <gtkmm-3.0/gtkmm/application.h>

# define ERROR_CONSOLE_TEXT "\033[31m" // Turn text on console red
# define TELEMETRY_CONSOLE_TEXT "\033[34m" // Turn text on console blue
# define NORMAL_CONSOLE_TEXT "\033[0m" // Restore normal console colour

using namespace dronecode_sdk;
using namespace std::chrono; // for seconds(), milliseconds()
using namespace std::this_thread; // for sleep_for()

// Handles Action's result
inline void handle_action_err_exit(Action::Result result,
    const std::string & message);
// Handles Mission's result
inline void handle_mission_err_exit(Mission::Result result,
    const std::string & message);
// Handles Connection result
inline void handle_connection_err_exit(ConnectionResult result,
    const std::string & message);

int stopTheDrone = 0;
int continueTheDrone = 0;

void usage(std::string bin_name) {
    std::cout << NORMAL_CONSOLE_TEXT << "Usage : " << bin_name <<
        " <connection_url> [path of QGC Mission plan]" << std::endl <<
        "Connection URL format should be :" << std::endl <<
        " For TCP : tcp://[server_host][:server_port]" << std::endl <<
        " For UDP : udp://[bind_host][:bind_port]" << std::endl <<
        " For Serial : serial:///path/to/serial/dev[:baudrate]" << std::endl <<
        "For example, to connect to the simulator use URL: udp://:14540" << std::endl;
}

std::string getTimeStr() {
    time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::string s(30, '\0');
    strftime( & s[0], s.size(), "%Y-%m-%d %H:%M:%S", std::localtime( & now));
    return s;
}

void pause_and_resume(std::shared_ptr <Mission> mission) {
    while(true){
        if(stopTheDrone == 1){
        // We pause
            auto prom = std::make_shared < std::promise < void >> ();
            auto future_result = prom -> get_future();
            mission -> pause_mission_async([prom](Mission::Result result) {
                prom -> set_value();
                std::cout << "Paused mission. (5)" << std::endl;
            });
            future_result.get();
            stopTheDrone = 0;
        }

        if (continueTheDrone == 1) {
        // Then continue.
            auto prom = std::make_shared < std::promise < void >> ();
            auto future_result = prom -> get_future();
            mission -> start_mission_async(
                [prom](Mission::Result result) {
                    prom -> set_value();
                });
            future_result.get();
            std::cout << "Resumed mission. (5)" << std::endl;
            continueTheDrone = 0;
        }
    }
}

Buttons::Buttons() {
    button1.add_pixlabel("info.xpm", "Pause");
    button2.add_pixlabel("info.xpm", "Continue");

    set_title("AirMatrix");
    set_border_width(10);
    add(box1);

    box1.pack_start(button1);
    box1.pack_start(button2);

    button1.signal_clicked().connect(sigc::mem_fun( * this, & Buttons::when_paused));
    button2.signal_clicked().connect(sigc::mem_fun( * this, & Buttons::when_continued));

    show_all_children();
}

Buttons::~Buttons() {}

void Buttons::on_button_clicked() {}

void startGUI(int argc, char ** argv) {
    std::cout << "Async Thread: " << std::this_thread::get_id() << std::endl;
    auto app = Gtk::Application::create(argc, argv, "org.gtkmm.test");
    //Shows the window and returns when it is closed.

    Buttons buttons;
    app -> run(buttons);
    std::cout << "GUI STOPPED" << std::endl;
}

void Buttons::when_paused() {
    std::cout << "Paused Pressed before " << stopTheDrone << std::endl;
    stopTheDrone = 1;
    std::cout << "Paused changed to " << stopTheDrone << std::endl;
}
void Buttons::when_continued() {
    std::cout << "Continue Pressed before " << continueTheDrone << std::endl;
    continueTheDrone = 1;
    std::cout << "Continue changed to " << continueTheDrone << std::endl;
}

int main(int argc, char ** argv) {

    char * str1;
    str1 = argv[0];
    char ** tempArgv;
    tempArgv = & str1;

    std::cout << argc << std::endl;

    auto tempArgc = argc;
    tempArgc = 1;

    std::cout << "Main thread: " << std::this_thread::get_id() << std::endl;

    std::future < void > fn = std::async (std::launch::async, startGUI, 1, tempArgv);

    DronecodeSDK dc;
    std::string connection_url;
    ConnectionResult connection_result;

    // Locate path of QGC Sample plan
    std::string qgc_plan = "../../../plugins/mission/north.plan";

    if (argc != 2 && argc != 3) {
        usage(argv[0]);
        return 1;
    }

    connection_url = argv[1];
    if (argc == 3) {
        qgc_plan = argv[2];
    }

    std::cout << "Connection URL: " << connection_url << std::endl;
    std::cout << "Importing mission from mission plan: " << qgc_plan << std::endl;

    {
        auto prom = std::make_shared < std::promise < void >> ();
        auto future_result = prom -> get_future();

        std::cout << "Waiting to discover system..." << std::endl;
        dc.register_on_discover([prom](uint64_t uuid) {
            std::cout << "Discovered system with UUID: " << uuid << std::endl;
            prom -> set_value();
        });

        connection_result = dc.add_any_connection(connection_url);
        handle_connection_err_exit(connection_result, "Connection failed: ");

        future_result.get();
    }

    dc.register_on_timeout([](uint64_t uuid) {
        std::cout << "System with UUID timed out: " << uuid << std::endl;
        std::cout << "Exiting." << std::endl;
        exit(0);
    });

    // We don't need to specify the UUID if it's only one system anyway.
    // If there were multiple, we could specify it with:
    // dc.system(uint64_t uuid);
    System & system = dc.system();
    auto action = std::make_shared < Action > (system);
    auto mission = std::make_shared < Mission > (system);
    auto telemetry = std::make_shared < Telemetry > (system);

    const Telemetry::Result set_rate_result = telemetry -> set_rate_position(1.0);
    if (set_rate_result != Telemetry::Result::SUCCESS) {
        std::cout << "Setting rate failed: " << std::endl;
        return 1;
    }

    std::ofstream myFile;
    myFile.open("testData.csv");
    myFile << "Time, Altitude, Latitude, Longitude, Absolute_Altitude, \n";
    // int countTelemetry = 0;
    // Setting up the callback to monitor lat and longitude
    telemetry -> position_async([ & ](Telemetry::Position position) {
        myFile << getTimeStr() << "," << (system.get_uuid()) % 100000 << "," << position.relative_altitude_m << "," << position.latitude_deg << "," << position.longitude_deg << "," << position.absolute_altitude_m << ", \n";
        std::cout << "stopTheDrone " << stopTheDrone << std::endl;
        std::cout << "continueTheDrone " << continueTheDrone << std::endl;
    });

    while (!telemetry -> health_all_ok()) {
        std::cout << "Waiting for system to be ready" << std::endl;
        sleep_for(seconds(1));
    }

    std::cout << "System ready" << std::endl;

    

    // Import Mission items from QGC plan
    Mission::mission_items_t mission_items;
    Mission::Result import_res = Mission::import_qgroundcontrol_mission(mission_items, qgc_plan);
    handle_mission_err_exit(import_res, "Failed to import mission items: ");

    if (mission_items.size() == 0) {
        std::cerr << "No missions! Exiting..." << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Found " << mission_items.size() << " mission items in the given QGC plan." <<
        std::endl;

    {
        std::cout << "Uploading mission..." << std::endl;
        // Wrap the asynchronous upload_mission function using std::future.
        auto prom = std::make_shared < std::promise < Mission::Result >> ();
        auto future_result = prom -> get_future();
        mission -> upload_mission_async(mission_items,
            [prom](Mission::Result result) {
                prom -> set_value(result);
            });

        const Mission::Result result = future_result.get();
        handle_mission_err_exit(result, "Mission upload failed: ");
        std::cout << "Mission uploaded." << std::endl;
    }

    std::cout << "Arming..." << std::endl;
    const Action::Result arm_result = action -> arm();
    handle_action_err_exit(arm_result, "Arm failed: ");
    std::cout << "Armed." << std::endl;

    // Before starting the mission subscribe to the mission progress.
    mission -> subscribe_progress([ & ](int current, int total) {
        std::cout << "Mission status update: " << current << " / " << total << std::endl;
        // if (stopTheDrone == 1) {
        //     pause_and_resume(mission);
        //     stopTheDrone = 0;
        //     std::cout << "This is after running pauseMission(mission)" << std::endl;
        // }
    });

    auto t1 = std::async(std::launch::async, pause_and_resume, mission);

    {
        std::cout << "Starting mission." << std::endl;
        auto prom = std::make_shared < std::promise < Mission::Result >> ();
        auto future_result = prom -> get_future();
        mission -> start_mission_async([prom](Mission::Result result) {
            prom -> set_value(result);
            std::cout << "Started mission." << std::endl;
        });

        const Mission::Result result = future_result.get();
        handle_mission_err_exit(result, "Mission start failed: ");
    }

    while (!mission -> mission_finished()) {
        sleep_for(seconds(1));
    }

    // Wait for some time.
    sleep_for(seconds(5));

    {
        // Mission complete. Command RTL to go home.
        std::cout << "Commanding RTL..." << std::endl;
        const Action::Result result = action -> return_to_launch();
        if (result != Action::Result::SUCCESS) {
            std::cout << "Failed to command RTL (" << Action::result_str(result) << ")" <<
                std::endl;
        } else {
            std::cout << "Commanded RTL." << std::endl;
        }
    }

    return 0;
}

inline void handle_action_err_exit(Action::Result result,
    const std::string & message) {
    if (result != Action::Result::SUCCESS) {
        std::cerr << ERROR_CONSOLE_TEXT << message << Action::result_str(result) <<
            NORMAL_CONSOLE_TEXT << std::endl;
        exit(EXIT_FAILURE);
    }
}

inline void handle_mission_err_exit(Mission::Result result,
    const std::string & message) {
    if (result != Mission::Result::SUCCESS) {
        std::cerr << ERROR_CONSOLE_TEXT << message << Mission::result_str(result) <<
            NORMAL_CONSOLE_TEXT << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Handles connection result
inline void handle_connection_err_exit(ConnectionResult result,
    const std::string & message) {
    if (result != ConnectionResult::SUCCESS) {
        std::cerr << ERROR_CONSOLE_TEXT << message << connection_result_str(result) <<
            NORMAL_CONSOLE_TEXT << std::endl;
        exit(EXIT_FAILURE);
    }
}