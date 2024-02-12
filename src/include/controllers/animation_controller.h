#pragma once

#include <iostream>
#include <vector>
#include "scene.h"

class AnimationController {
private:
    std::vector<std::shared_ptr<Driver> > drivers;
    bool paused = false;

public:
    AnimationController(std::vector<std::shared_ptr<Driver> >& drivers_)
    {
        for(auto& driver: drivers_){
            drivers.push_back(std::move(driver));
        }
    }


    void driveAnimation(float deltaTime){
        if(!paused) {
            for(auto& driver: drivers){
                driver->animate(deltaTime);
            }
        }
        
    }

    void pauseOrResume(){
        paused = !paused;
        if(paused){
            std::cout<<"Animation paused\n";
        } else {
            std::cout<<"Animation resumed\n";
        }
    }

    void restart() {
        std::cout<<"Restart animation\n";
        for(auto& driver: drivers){
            driver->restart();
        }
    }

    void activateLoop() {
        for(auto& driver: drivers){
            driver->loop = true;
        }
    }

    void disableLoop() {
        for(auto& driver: drivers){
            driver->loop = false;
        }
    }

    void setPlaybackTimeRate(float time, float rate){
        if(rate == 0)
            paused = true;
        else if (rate == 1)
            paused = false;

        for(auto& driver: drivers){
            driver->setPlaybackTime(time);
        }
    }

};