# tare_planner
tare planner for cerlab uav simulator.
Please use [this version](https://github.com/Zhefan-Xu/map_manager/tree/tare) of map_manager(it add "intensity" field for published map cloud)

Before starting the exploration, take off the drone manually and tilt a bit so that there are some obstacles in the fov.
with uav simulator running, run 
```
roslaunch tare_planner explore.launch 
```
After exploring the whole map, end the exploration manually

