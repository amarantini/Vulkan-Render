#pragma once 

#include <string>

#include "mathlib.h"
#include "constants.h"
#include "transform.h"

struct Driver {
    std::string name;
    std::string channel;
    std::vector<double> times;
    std::vector<vec3> values3d; //for translation and scle
    std::vector<qua> values4d; //for rotation
    std::string interpolation;
    std::shared_ptr<Transform> transform;
    size_t frame_idx = 0;
    bool loop = false;
    bool finished = false;
    float frame_time = 0.0f;

    Driver(const std::string& _name, const std::string& _channel, const std::vector<double> _times, 
           const std::string& _interpolation)
        : name(_name), channel(_channel), times(_times), interpolation(_interpolation) {}

    void restart(){
        finished = false;
        frame_time = 0.0f;
        frame_idx = 0;
    }

    bool isFinished(const float t){
        frame_time += t;
        if(frame_time > times.back()){
            if(!loop) {
                finished = true;
                return true;
            } 
            else {
                frame_time -= times.back();
                frame_idx = 0;
            }
        }
        while(times[frame_idx+1]<frame_time){
            frame_idx++;
        }
        return false;
    }

    void linearInterp(){
        float t = (frame_time-times[frame_idx])/(times[frame_idx+1]-times[frame_idx]);
        if(channel==CHANEL_SCALE){
            transform->scale = lerp(values3d[frame_idx], values3d[frame_idx+1], t);
        } else if (channel==CHANEL_TRANSLATION){
            transform->translation = lerp(values3d[frame_idx], values3d[frame_idx+1], t);
        } else if (channel==CHANEL_ROTATION){
            transform->rotation = quaLerp(values4d[frame_idx], values4d[frame_idx+1], t);
        }
    }

    void animate(const float deltaTime){
        if(finished)
            return;
        if(isFinished(deltaTime)){
            std::cout<<name<<" animation finished\n";
            return;
        } else {
            if(interpolation==INTERP_LINEAR){
                linearInterp();
            } // else TODO
        }
    }

    void setPlaybackTime(float time) {
        frame_time = time;
        frame_idx = 0;
        while(times[frame_idx+1]<frame_time){
            frame_idx++;
        }
    }
};