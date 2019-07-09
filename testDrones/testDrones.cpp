// Example to connect multiple vehicles and make them follow the plan file. Also saves the telemetry information to a csv file
//./multiple_drones udp://:14540 udp://:14541
//
// Author: Julian Oes <julian@oes.ch>
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
#include <vector>
#include "gtkmm_buttons.hpp"
#include <gtkmm-3.0/gtkmm.h>
#include <gtkmm-3.0/gtkmm/window.h>
#include <gtkmm-3.0/gtkmm/button.h>
#include <gtkmm-3.0/gtkmm/box.h>
#include <gtkmm-3.0/gtkmm/application.h>

using namespace dronecode_sdk;
using namespace std::this_thread;
using namespace std::chrono;

static void complete_mission(std::string qgc_plan, System & system, char * str1);

/*
Stop the Drones is what determines whether to stop the drone or not
Continue the Drone determines whether the drone can continue moving

Both of these are vectors because we can have more than one drones

They are made global because it is just for testing and because it makes easy to implement the functionality.
    - Easy way to communicate between the buttons and the main function
*/
std::vector < int > stopTheDrones;
std::vector < int > continueTheDrones;
std::vector < int > returnToLaunch;
std::vector < int > landDrones;

# define ERROR_CONSOLE_TEXT "\033[31m" // Turn text on console red
# define TELEMETRY_CONSOLE_TEXT "\033[34m" // Turn text on console blue
# define NORMAL_CONSOLE_TEXT "\033[0m" // Restore normal console colour

// Handles Action's result
inline void handle_action_err_exit(Action::Result result,
    const std::string & message);
// Handles Mission's result
inline void handle_mission_err_exit(Mission::Result result,
    const std::string & message);
// Handles Connection result
inline void handle_connection_err_exit(ConnectionResult result,
    const std::string & message);

void usage(std::string bin_name) {
    std::cout << NORMAL_CONSOLE_TEXT << "Usage : " << bin_name <<
        " <connection_url> [path of QGC Mission plan]" << std::endl <<
        "Connection URL format should be :" << std::endl <<
        " For TCP : tcp://[server_host][:server_port]" << std::endl <<
        " For UDP : udp://[bind_host][:bind_port]" << std::endl <<
        " For Serial : serial:///path/to/serial/dev[:baudrate]" << std::endl <<
        "For example, to connect to the simulator use URL: udp://:14540" << std::endl;
}

// This function is just to return the current time. This is used when storing telemetry data

std::string getTimeStr() {
    time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::string s(30, '\0');
    strftime( & s[0], s.size(), "%Y-%m-%d %H:%M:%S", std::localtime( & now));
    return s;
}

/*
This function pauses and resumes the mission of the drones. 
This function takes in mission and vehicle ID as the parameters to identify which vehicle's mission to pause

This function is running independent of main thread and is just a while loop that continuously checks if the buttons are pressed
This is not an efficient way of checking if the buttons are pressed but is easy to implement (Again, it is just for testing)
*/
void pause_and_resume(std::shared_ptr < Mission > mission, std::shared_ptr < Action > action , long vehicleID) {

    // This is to get the last digit of uuid number, which in our case starts with 1 
    // Because the uuid's last digit starts with 1 and the vectors start with 0, when accessing certain drone's property, we need to 
    // access it by: (id-1)
    int id = vehicleID % 5283920058631409230; 

    while (true) {
        if (stopTheDrones[id - 1] == 1) {
            // We pause
            auto prom = std::make_shared < std::promise < Mission::Result >> ();
            auto future_result = prom -> get_future();
            mission -> pause_mission_async([prom](Mission::Result result) {
                prom -> set_value(result);
                std::cout << "Paused mission. (5)" << std::endl;
            });
            future_result.get();
            stopTheDrones[id - 1] = 0;
        }

        if (continueTheDrones[id - 1] == 1) {
            // Then continue.
            auto prom = std::make_shared < std::promise < Mission::Result >> ();
            auto future_result = prom -> get_future();
            mission -> start_mission_async(
                [prom](Mission::Result result) {
                    prom -> set_value(result);
                });
            future_result.get();
            std::cout << "Resumed mission. (5)" << std::endl;
            continueTheDrones[id - 1] = 0;
        }

        if (returnToLaunch[id - 1] == 1) {
            const Action::Result land_result = action->return_to_launch();
            const Action::Result arm_result = action->set_return_to_launch_return_altitude(10.0);
            if (land_result != Action::Result::SUCCESS){
                // RTL failed
                std::cout << "RTL Failed " << std::endl;
            }
        returnToLaunch[id - 1] = 0;
        }

        if (landDrones[id - 1] == 1){
            const Action::Result land_result = action->land();
            if (land_result != Action::Result::SUCCESS) {
                // std::cout << "Land failed:" << Action::result_str(land_result) << std::endl;
            }
        landDrones[id - 1] = 0;
        }
    }
}

// This is just making the buttons and numbering them 
Buttons::Buttons(int nums) {

    button = new Gtk::Button[nums];

    if (nums % 4 == 0) {
        for (int i = 0; i < nums; i += 4) {
            button[i].add_pixlabel("info.xpm", ("Pause Drone: " + std::to_string(i / 2)));
            button[i + 1].add_pixlabel("info.xpm", ("Continue Drone: " + std::to_string(i / 2)));
            button[i + 2].add_pixlabel("info.xpm", ("Return To Launch: " + std::to_string(i / 2)));
            button[i + 3].add_pixlabel("info.xpm", ("Land Drone: " + std::to_string(i / 2)));
        }
    }

    set_title("AirMatrix");
    set_border_width(10);
    add(box1);

    for (int i = 0; i < nums; i++) {
        box1.pack_start(button[i]);
    }

    for (int i = 0; i < nums; i += 4) {
        button[i].signal_clicked().connect(sigc::bind < Glib::ustring > (sigc::mem_fun( * this, & Buttons::when_paused), std::to_string(i / 2)));
        button[i + 1].signal_clicked().connect(sigc::bind < Glib::ustring > (sigc::mem_fun( * this, & Buttons::when_continued), std::to_string(i / 2)));
        button[i + 2].signal_clicked().connect(sigc::bind < Glib::ustring > (sigc::mem_fun(*this, &Buttons::when_returning), std::to_string(i / 2)));
        button[i + 3].signal_clicked().connect(sigc::bind < Glib::ustring > (sigc::mem_fun(*this, &Buttons::when_landing), std::to_string(i / 2)));
    }
    show_all_children();
}

Buttons::~Buttons() {}

// This is what makes the window and the application where we find all those buttons
void startGUI(int argc, char ** argv, int numOfButtons) {
    std::cout << "Async Thread: " << std::this_thread::get_id() << std::endl;
    auto app = Gtk::Application::create(argc, argv, "org.gtkmm.test");
    //Shows the window and returns when it is closed.
    Buttons buttons(numOfButtons);
    app -> run(buttons);
    std::cout << "GUI STOPPED" << std::endl;
}

/* 
This function gets called when paused button is pressed
This function also takes a string value that gives the information of which drone is getting paused
*/
void Buttons::when_paused(Glib::ustring data) {
    std::cout << "Paused Pressed " << data << std::endl;
    stopTheDrones[std::stoi(data)] = 1;
}

/* 
This function gets called when continue button is pressed
This function also takes a string value that gives the information of which drone is getting continued
*/
void Buttons::when_continued(Glib::ustring data) {
    std::cout << "Continue Pressed " << data << std::endl;
    continueTheDrones[std::stoi(data)] = 1;
}

void Buttons::when_returning(Glib::ustring data){
    std::cout << "RTL pressed " << data << std::endl;
    returnToLaunch[std::stoi(data)] =  1;
}

void Buttons::when_landing(Glib::ustring data){
    std::cout << "Landing pressed " << data << std::endl;
    landDrones[std::stoi(data)] =  1;
}

int main(int argc, char * argv[]) {
    if (argc == 1) {
        std::cerr << ERROR_CONSOLE_TEXT << "Please specify connection" << NORMAL_CONSOLE_TEXT <<
            std::endl;
        return 1;
    }

/*
All this code until the startGUI function is launched asynchronously is to activate and start the GUI and the buttons
Application opens up when we run ./testDrones. The application will open up even if the drone is not ready to fly or 
even if the mission hasn't been uploaded. This can be changed and made it so it only opens up when at least one udp connection is 
specified and at least one mission file is sent in by changing above if statement (for example: changing the if statement's condition to 

if(argc == 2){
    std::cerr << ERROR_CONSOLE_TEXT << "Make sure you've specified connection and have typed the path of mission pile " << NORMAL_CONSOLE_TEXT << std::endl;
    return 1;
}
*/
    char * str1;
    str1 = argv[0];
    char ** tempArgv;
    tempArgv = & str1;
    auto tempArgc = argc;
    tempArgc = 1;

    DronecodeSDK dc;
    int total_ports = argc / 2 + 1;         // There must be half udp ports and half paths to plan files

    // Launching GUI asynchronously
    std::future < void > fn = std::async (std::launch::async, startGUI, 1, tempArgv, (argc / 2) * 4);

    // Initialize the vectors "stop and continue the array" with 0
    for (int i = 0; i < total_ports; i++) {
        stopTheDrones.push_back(0);
        continueTheDrones.push_back(0);
        returnToLaunch.push_back(0);
        landDrones.push_back(0);
    }

    // the loop below adds the number of ports the sdk monitors.
    for (int i = 1; i < total_ports; ++i) {
        ConnectionResult connection_result = dc.add_any_connection(argv[i]);
        if (connection_result != ConnectionResult::SUCCESS) {
            std::cerr << ERROR_CONSOLE_TEXT <<
                "Connection error: " << connection_result_str(connection_result) <<
                NORMAL_CONSOLE_TEXT << std::endl;
            return 1;
        }
    }

    bool discovered_system = false;

    std::cout << "Waiting to discover system..." << std::endl;
    dc.register_on_discover([ & discovered_system](uint64_t uuid) {
        std::cout << "Discovered system with UUID: " << uuid << std::endl;
        discovered_system = true;
    });

    // We usually receive heartbeats at 1Hz, therefore we should find a system after around 2
    // seconds.
    sleep_for(seconds(2));

    if (!discovered_system) {
        std::cerr << ERROR_CONSOLE_TEXT << "No system found, exiting." << NORMAL_CONSOLE_TEXT <<
            std::endl;
        return 1;
    }

    std::vector < std::thread > threads;

    int numberOfPaths = total_ports;
    for (auto uuid: dc.system_uuids()) {
        System & system = dc.system(uuid);
        // complete_mission(std::ref(qgc_plan), std::ref(system));
        std::thread t( & complete_mission, argv[numberOfPaths], std::ref(system), argv[0]);
        threads.push_back(std::move(t)); // Instead of copying, move t into the vector (less expensive)
        numberOfPaths += 1;
    }

    for (auto & t: threads) {
        t.join();
    }
    return 0;
}

void complete_mission(std::string qgc_plan, System & system, char * str1) {
    auto telemetry = std::make_shared < Telemetry > (system);
    auto action = std::make_shared < Action > (system);
    auto mission = std::make_shared < Mission > (system);

    // We want to listen to the altitude of the drone at 1 Hz.
    const Telemetry::Result set_rate_result = telemetry -> set_rate_position(1.0);

    if (set_rate_result != Telemetry::Result::SUCCESS) {
        std::cerr << ERROR_CONSOLE_TEXT <<
            "Setting rate failed:" << Telemetry::result_str(set_rate_result) <<
            NORMAL_CONSOLE_TEXT << std::endl;
        return;
    }

    std::cout << "Importing mission from mission plan: " << qgc_plan << std::endl;

    // This makes a csv file and stores the telemetry data. A different csv file is made for different drones
    std::ofstream myFile;
    myFile.open((std::to_string(system.get_uuid() % 100000) + ".csv"));
    myFile << "Time, Vehicle_ID, Altitude, Latitude, Longitude, Absolute_Altitude, \n";
 
    // Setting up the callback to monitor lat and longitude
    telemetry -> position_async([ & ](Telemetry::Position position) {
        myFile << getTimeStr() << "," << (system.get_uuid()) % 100000 << "," << position.relative_altitude_m << "," << position.latitude_deg << "," << position.longitude_deg << "," << position.absolute_altitude_m << ", \n";
    });

    // Check if vehicle is ready to arm
    while (telemetry -> health_all_ok() != true) {
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
    mission -> subscribe_progress([](int current, int total) {
        std::cout << "Mission status update: " << current << " / " << total << std::endl;
    });

    // std::string threadNum = std::to_string((system.get_uuid())%100000);
    // Checking if buttons have been pressed independently from main thread
    auto t1 = std::async (std::launch::async, pause_and_resume, mission, action, (system.get_uuid()));

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

    // Mission complete. Landing now
    std::cout << "Landing at last node..." << std::endl;

    myFile.close();
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