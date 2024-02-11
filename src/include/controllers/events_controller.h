#pragma once

#include <iostream>
#include <stdio.h>
#include <string>
#include <fstream>
#include <vector>

#include "constants.h"

enum EventType{AVAILABLE, PLAY, SAVE, MARK}; 

struct Event {
    EventType type;
    float ts;
    // PLAY
    float time;
    float rate;
    // SAVE
    std::string filename; //filename.ppm
    // MARK
    std::string description_words; 
};

class EventsController {
private:
    std::vector<Event> events;
    size_t idx = 0; 

public:
    EventsController() = default;

    bool isFinished(){
        return idx >= events.size();
    }

    Event& nextEvent(){
        return events[idx++];
    }

    void load(std::string events_file_name){
        std::ifstream file(events_file_name);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        std::string line;
        while (getline(file, line)) {
            // std::vector<std::string> strs = parseString(line);
            Event e = {};
            size_t start = line.find(' ');
            e.ts = stoi(line.substr(0, start));
            start += 1;
            size_t end = line.find(' ', start);
            std::string type = line.substr(start, end - start);
            if (type == "AVAILABLE") {
                e.type = EventType::AVAILABLE;
            } else if (type == "PLAY") {
                e.type = EventType::PLAY;
                start = end+1;
                end = line.find(' ', start);
                e.time = stoi(line.substr(start, end - start));
                start = end+1;
                end = line.find(' ', start);
                e.rate = stoi(line.substr(start, end - start));
            } else if (type == "SAVE") {
                e.type = EventType::SAVE;
                e.filename = line.substr(end+1);
            } else if (type == "MARK") {
                e.type = EventType::MARK;
                e.description_words = line.substr(end+1);
            }
            
            events.push_back(e);
        }
        file.close();
    }

    void printEvents(){
        for(auto& e: events){
            if(e.type == EventType::AVAILABLE) {
                std::cout<<e.ts<<" AVAILABLE\n";
            } else if (e.type == EventType::PLAY) {
                std::cout<<e.ts<<" PLAY "<<e.time<<" "<<e.rate<<"\n";
            } else if (e.type == EventType::SAVE) {
                std::cout<<e.ts<<" SAVE "<<e.filename<<"\n";
            } else if (e.type == EventType::MARK) {
                std::cout<<e.ts<<" MARK "<<e.description_words<<"\n";
            }
        }
    }
    
};