// Example to connect multiple vehicles and make them follow the plan file. Also saves the telemetry information to a csv file
//./multiple_drones udp://:14540 udp://:14541
//
// Author: Julian Oes <julian@oes.ch>
// Author: Shayaan Haider (via Slack)
// Shusil Shapkota

#include <dronecode_sdk/dronecode_sdk.h>
#include <dronecode_sdk/plugins/action/action.h>
#include <dronecode_sdk/plugins/mission/mission.h>
#include <dronecode_sdk/plugins/telemetry/telemetry.h>

#include <cstdint>
#include <stdlib.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <bits/stdc++.h>
#include <ctime>
#include <fstream>

#include "gtkmm_buttons.hpp"

#include <gtkmm-3.0/gtkmm.h>

#include <gtkmm-3.0/gtkmm/window.h>

#include <gtkmm-3.0/gtkmm/button.h>

#include <gtkmm-3.0/gtkmm/box.h>

#include <gtkmm-3.0/gtkmm/application.h>

using namespace dronecode_sdk;
using namespace std::this_thread;
using namespace std::chrono;

static void complete_mission(std::string qgc_plan, System &system, char* str1);

int stopTheDrone = 0;
int continueTheDrone = 0;

#define ERROR_CONSOLE_TEXT "\033[31m" // Turn text on console red
#define TELEMETRY_CONSOLE_TEXT "\033[34m" // Turn text on console blue
#define NORMAL_CONSOLE_TEXT "\033[0m" // Restore normal console colour

// Handles Action's result
inline void handle_action_err_exit(Action::Result result, const std::string &message);
// Handles Mission's result
inline void handle_mission_err_exit(Mission::Result result, const std::string &message);
// Handles Connection result
inline void handle_connection_err_exit(ConnectionResult result, const std::string &message);

void usage(std::string bin_name)
{
    std::cout << NORMAL_CONSOLE_TEXT << "Usage : " << bin_name
              << " <connection_url> [path of QGC Mission plan]" << std::endl
              << "Connection URL format should be :" << std::endl
              << " For TCP : tcp://[server_host][:server_port]" << std::endl
              << " For UDP : udp://[bind_host][:bind_port]" << std::endl
              << " For Serial : serial:///path/to/serial/dev[:baudrate]" << std::endl
              << "For example, to connect to the simulator use URL: udp://:14540" << std::endl;
}

std::string getTimeStr(){
    time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::string s(30, '\0');
    strftime(&s[0], s.size(), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return s;
}


void pause_and_resume(std::shared_ptr <Mission> mission) {
    {
        // We pause inside the callback and hope not to get blocked.
        auto prom = std::make_shared < std::promise < void >> ();
        auto future_result = prom -> get_future();
        mission -> pause_mission_async([prom](Mission::Result result) {
            prom -> set_value();
            std::cout << "Paused mission. (5)" << std::endl;
        });
        future_result.get();
    }

    while (continueTheDrone != 1) {
        std::cout << "Waiting for the signal to continue.. " << std::endl;
    }

    if (continueTheDrone == 1) {
        // Then continue.
        {
            auto prom = std::make_shared < std::promise < void >> ();
            auto future_result = prom -> get_future();
            mission -> start_mission_async(
                [prom](Mission::Result result) {
                    prom -> set_value();
                });
            future_result.get();
            std::cout << "Resumed mission. (5)" << std::endl;
        }
        continueTheDrone = 0;
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
    std::cout << "Aldo, you sleepy? " << std::endl;
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
    if (stopTheDrone == 1)
        continueTheDrone = 1;
    std::cout << "Continue changed to " << continueTheDrone << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc == 1) {
        std::cerr << ERROR_CONSOLE_TEXT << "Please specify connection" << NORMAL_CONSOLE_TEXT
                  << std::endl;
        return 1;
    }

    DronecodeSDK dc;
    int total_ports = argc/2 + 1;       // There must be half udp ports and half paths to plan files

    // the loop below adds the number of ports the sdk monitors.
    for (int i = 1; i < total_ports; ++i) {
        ConnectionResult connection_result = dc.add_any_connection(argv[i]);
        if (connection_result != ConnectionResult::SUCCESS) {
            std::cerr << ERROR_CONSOLE_TEXT
                      << "Connection error: " << connection_result_str(connection_result)
                      << NORMAL_CONSOLE_TEXT << std::endl;
            return 1;
        }
    }

    bool discovered_system = false;

    std::cout << "Waiting to discover system..." << std::endl;
    dc.register_on_discover([&discovered_system](uint64_t uuid) {
        std::cout << "Discovered system with UUID: " << uuid << std::endl;
        discovered_system = true;
    });

    // We usually receive heartbeats at 1Hz, therefore we should find a system after around 2
    // seconds.
    sleep_for(seconds(2));

    if (!discovered_system) {
        std::cerr << ERROR_CONSOLE_TEXT << "No system found, exiting." << NORMAL_CONSOLE_TEXT
                  << std::endl;
        return 1;
    }
    // Locate path of QGC Sample plan
    //std::string qgc_plan = "../../../plugins/mission/test.plan";
    //std::cout << "Importing mission from mission plan: " << qgc_plan << std::endl;

    std::vector<std::thread> threads;

    int numberOfPaths = total_ports;
    for (auto uuid : dc.system_uuids()) {
        System &system = dc.system(uuid);
        // complete_mission(std::ref(qgc_plan), std::ref(system));
        std::thread t(&complete_mission, argv[numberOfPaths], std::ref(system), argv[0]);
        threads.push_back(std::move(t));        // Instead of copying, move t into the vector (less expensive)
        numberOfPaths += 1;
    }

    for (auto &t : threads) {
        t.join();
    }
    return 0;
}

void complete_mission(std::string qgc_plan, System &system, char* str1)
{
    char ** tempArgv;
    tempArgv = & str1;
    int argc = 1;

    std::cout << argc << std::endl;

    auto tempArgc = argc;

    std::cout << " thread id: " << std::this_thread::get_id() << std::endl;

    std::future <void> fn = std::async (std::launch::async, startGUI, tempArgc, tempArgv);

    auto telemetry = std::make_shared<Telemetry>(system);
    auto action = std::make_shared<Action>(system);
    auto mission = std::make_shared<Mission>(system);

    // We want to listen to the altitude of the drone at 1 Hz.
    const Telemetry::Result set_rate_result = telemetry->set_rate_position(1.0);

    if (set_rate_result != Telemetry::Result::SUCCESS) {
        std::cerr << ERROR_CONSOLE_TEXT
                  << "Setting rate failed:" << Telemetry::result_str(set_rate_result)
                  << NORMAL_CONSOLE_TEXT << std::endl;
        return;
    }
    
    std::cout << "Importing mission from mission plan: " << qgc_plan << std::endl;

    std::ofstream myFile; 
    myFile.open((std::to_string(system.get_uuid()%100000) + ".csv"));
    myFile << "Time, Vehicle_ID, Altitude, Latitude, Longitude, Absolute_Altitude, \n";
    // int countTelemetry = 0;

    // Setting up the callback to monitor lat and longitude
    telemetry->position_async([&](Telemetry::Position position){
        myFile << getTimeStr() << "," << (system.get_uuid())%100000 << "," << position.relative_altitude_m << "," << position.latitude_deg << "," << position.longitude_deg << "," << position.absolute_altitude_m << ", \n"; 
        // std::string runFile = "python ./testingPython.py " + std::to_string((system.get_uuid())%100000) + " " + std::to_string(countTelemetry) + " " + std::to_string(position.latitude_deg) + " " + std::to_string(position.longitude_deg);
        // int n = runFile.length();
        // char char_array[n+1];
        // strcpy(char_array, runFile.c_str());
        // std::system(char_array);
        // countTelemetry += 1;
    });

    // Check if vehicle is ready to arm
    while (telemetry->health_all_ok() != true) {
        std::cout << "Vehicle is getting ready to arm" << std::endl;
        sleep_for(seconds(1));
    }

    // Import Mission items from QGC plan
    Mission::mission_items_t mission_items;
    Mission::Result import_res = Mission::import_qgroundcontrol_mission(mission_items, qgc_plan);
    handle_mission_err_exit(import_res, "Failed to import mission items: ");

    if (mission_items.size() == 0) {
        std::cerr << "No missions! Exiting..." << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Found " << mission_items.size() << " mission items in the given QGC plan."
              << std::endl;

    {
        std::cout << "Uploading mission..." << std::endl;
        // Wrap the asynchronous upload_mission function using std::future.
        auto prom = std::make_shared<std::promise<Mission::Result>>();
        auto future_result = prom->get_future();
        mission->upload_mission_async(mission_items,
                                      [prom](Mission::Result result) { prom->set_value(result); });

        const Mission::Result result = future_result.get();
        handle_mission_err_exit(result, "Mission upload failed: ");
        std::cout << "Mission uploaded." << std::endl;
    }

    std::cout << "Arming..." << std::endl;
    const Action::Result arm_result = action->arm();
    handle_action_err_exit(arm_result, "Arm failed: ");
    std::cout << "Armed." << std::endl;

    // Before starting the mission subscribe to the mission progress.
    mission->subscribe_progress([](int current, int total) {
        std::cout << "Mission status update: " << current << " / " << total << std::endl;

        if (stopTheDrone == 1) {
            pause_and_resume(mission);
            stopTheDrone = 0;
            std::cout << "This is after running pauseMission(mission)" << std::endl;
        }

    });

    {
        std::cout << "Starting mission." << std::endl;
        auto prom = std::make_shared<std::promise<Mission::Result>>();
        auto future_result = prom->get_future();
        mission->start_mission_async([prom](Mission::Result result) {
            prom->set_value(result);
            std::cout << "Started mission." << std::endl;
        });

        const Mission::Result result = future_result.get();
        handle_mission_err_exit(result, "Mission start failed: ");
    }

    while (!mission->mission_finished()) {
        sleep_for(seconds(1));
    }

    // Wait for some time.
    sleep_for(seconds(5));

    // Mission complete. Landing now
      std::cout << "Landing at last node..." << std::endl;

    myFile.close();
}

inline void handle_action_err_exit(Action::Result result, const std::string &message)
{
    if (result != Action::Result::SUCCESS) {
        std::cerr << ERROR_CONSOLE_TEXT << message << Action::result_str(result)
                  << NORMAL_CONSOLE_TEXT << std::endl;
        exit(EXIT_FAILURE);
    }
}

inline void handle_mission_err_exit(Mission::Result result, const std::string &message)
{
    if (result != Mission::Result::SUCCESS) {
        std::cerr << ERROR_CONSOLE_TEXT << message << Mission::result_str(result)
                  << NORMAL_CONSOLE_TEXT << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Handles connection result
inline void handle_connection_err_exit(ConnectionResult result, const std::string &message)
{
    if (result != ConnectionResult::SUCCESS) {
        std::cerr << ERROR_CONSOLE_TEXT << message << connection_result_str(result)
                  << NORMAL_CONSOLE_TEXT << std::endl;
        exit(EXIT_FAILURE);
    }
}

