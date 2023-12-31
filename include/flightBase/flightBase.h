/*
	FILE: flightBase.h
	-------------------
	base implementation for autonomous flight
*/

#ifndef TARE_FLIGHTBASE_H
#define TARE_FLIGHTBASE_H
#include <ros/ros.h>
#include <std_msgs/Empty.h>
#include <nav_msgs/Path.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/TwistStamped.h>
#include <gazebo_msgs/SetModelState.h>
#include <thread>
#include <mutex>

#include <tf2/LinearMath/Quaternion.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>

using std::cout; using std::endl;
namespace sensor_coverage_planner_3d_ns{

	struct trajData{
		nav_msgs::Path trajectory;
		nav_msgs::Path currTrajectory;
		ros::Time startTime;
		double tCurr;
		double duration;
		double timestep;
		bool init = false;
		int forwardIdx = 10;
		int minIdx = 20;

		// void updateTrajectory(const nav_msgs::Path& _trajectory, double _duration){
		// 	this->trajectory = _trajectory;
		// 	this->currTrajectory = _trajectory;
		// 	this->duration = _duration;
		// 	this->tCurr = 0.0;
		// 	this->startTime = ros::Time::now();
		// 	this->timestep = this->duration/(this->trajectory.poses.size()-1);
		// 	if (not this->init){
		// 		this->init = true;
		// 	}
		// }

		void updateTrajectory(const nav_msgs::Path& _trajectory){
			this->trajectory = _trajectory;
			this->currTrajectory = _trajectory;
			this->tCurr = 0.0;
			this->startTime = ros::Time::now();
			// this->timestep = this->duration/(this->trajectory.poses.size()-1);
			if (not this->init){
				this->init = true;
			}
		}


		void updateTrajectoryTimestep(const nav_msgs::Path& _trajectory, double _timestep){
			this->trajectory = _trajectory;
			this->currTrajectory = _trajectory;
			this->timestep = _timestep;
			this->duration = (this->trajectory.poses.size()-1) * this->timestep;
			this->tCurr = 0.0;
			this->startTime = ros::Time::now();
			if (not this->init){
				this->init = true;
			}
		}

		int getCurrIdx(){
			ros::Time currTime = ros::Time::now();
			this->tCurr = (currTime - this->startTime).toSec() + this->timestep;
			if (this->tCurr > this->duration){
				this->tCurr = this->duration;
			}

			int idx = floor(this->tCurr/this->timestep);
			return idx;
		}

		geometry_msgs::PoseStamped getPose(){
			int idx = this->getCurrIdx();
			idx = std::max(idx+forwardIdx, minIdx);
			int newIdx = std::min(idx, int(this->trajectory.poses.size()-1));


			std::vector<geometry_msgs::PoseStamped> pathVec;
			for (size_t i=idx; i<this->trajectory.poses.size(); ++i){
				pathVec.push_back(this->trajectory.poses[i]);
			}
			this->currTrajectory.poses = pathVec;
			return this->trajectory.poses[newIdx];
		}

		geometry_msgs::PoseStamped getPose(const geometry_msgs::Pose& psCurr){
			int idx = this->getCurrIdx();
			idx = std::max(idx+forwardIdx, minIdx);
			int newIdx = std::min(idx, int(this->trajectory.poses.size()-1));
			// int di = 0;
			// int newIdx = std::min(idx+di, int(this->trajectory.poses.size()-1));


			std::vector<geometry_msgs::PoseStamped> pathVec;
			geometry_msgs::PoseStamped psFirst;
			psFirst.pose = psCurr;
			pathVec.push_back(psFirst);
			for (size_t i=idx; i<this->trajectory.poses.size(); ++i){
				pathVec.push_back(this->trajectory.poses[i]);
			}
			this->currTrajectory.poses = pathVec;
			return this->trajectory.poses[newIdx];
		}

		geometry_msgs::PoseStamped getPoseWithoutYaw(const geometry_msgs::Pose& psCurr){
			int idx = this->getCurrIdx();
			idx = std::max(idx+forwardIdx, minIdx);
			int newIdx = std::min(idx, int(this->trajectory.poses.size()-1));

			std::vector<geometry_msgs::PoseStamped> pathVec;
			geometry_msgs::PoseStamped psFirst;
			psFirst.pose = psCurr;
			pathVec.push_back(psFirst);
			for (size_t i=idx; i<this->trajectory.poses.size(); ++i){
				pathVec.push_back(this->trajectory.poses[i]);
			}
			this->currTrajectory.poses = pathVec;
			geometry_msgs::PoseStamped psTarget = this->trajectory.poses[newIdx];
			psTarget.pose.orientation = psCurr.orientation;
			return psTarget;	
		}

		void stop(const geometry_msgs::Pose& psCurr){
			std::vector<geometry_msgs::PoseStamped> pathVec;
			geometry_msgs::PoseStamped ps;
			ps.pose = psCurr;
			pathVec.push_back(ps);
			this->trajectory.poses = pathVec;
			this->currTrajectory = this->trajectory;
			this->tCurr = 0.0;
			this->startTime = ros::Time::now();
			this->duration = this->timestep; 
		}

		double getRemainTime(){
			ros::Time currTime = ros::Time::now();
			double tCurr = (currTime - this->startTime).toSec() + this->timestep;
			return this->duration - tCurr;
		}

		bool needReplan(double factor){
			if (this->getRemainTime() <= this->duration * (1 - factor)){
				return true;
			}
			else{
				return false;
			}
		}
	};

	class flightBase{
	protected:
		// ROS
		ros::NodeHandle nh_;
		ros::Subscriber odomSub_;
		ros::Subscriber clickSub_;
		ros::Publisher takeoffPub_;
		ros::Publisher posePub_;

		nav_msgs::Odometry odom_;
		geometry_msgs::PoseStamped poseTgt_; // target pose
		geometry_msgs::PoseStamped goal_;

		// status
		bool odomReceived_ = false;
		bool hasTakeoff_ = false;
		bool firstGoal_ = false;
		bool goalReceived_ = false;

		double takeoffHgt_ = 1.0;


	public:
		flightBase(const ros::NodeHandle& nh);
		~flightBase();
		void takeoff();
		void updateTarget(const geometry_msgs::PoseStamped& ps);
		void run();
		void odomCB(const nav_msgs::OdometryConstPtr& odom); 
		void clickCB(const geometry_msgs::PoseStamped::ConstPtr& cp);
		bool isReach(const geometry_msgs::PoseStamped& poseTgt, bool useYaw=true);

		inline void setOdomReceived(const bool &received){
			this->odomReceived_ = received;
		}

		// utils
		inline double rpy_from_quaternion(const geometry_msgs::Quaternion& quat){
			// return is [0, 2pi]
			tf2::Quaternion tf_quat;
			tf2::convert(quat, tf_quat);
			double roll, pitch, yaw;
			tf2::Matrix3x3(tf_quat).getRPY(roll, pitch, yaw);
			return yaw;
		}

		inline void rpy_from_quaternion(const geometry_msgs::Quaternion& quat, double &roll, double &pitch, double &yaw){
			tf2::Quaternion tf_quat;
			tf2::convert(quat, tf_quat);
			tf2::Matrix3x3(tf_quat).getRPY(roll, pitch, yaw);
		}
	};
}
	// struct trajData{
	// 	nav_msgs::Path trajectory;
	// 	nav_msgs::Path currTrajectory;
	// 	ros::Time startTime;
	// 	double tCurr;
	// 	double duration;
	// 	double timestep;
	// 	bool init = false;
	// 	int forwardIdx = 3;
	// 	int minIdx = 5;

	// 	// void updateTrajectory(const nav_msgs::Path& _trajectory, double _duration){
	// 	// 	this->trajectory = _trajectory;
	// 	// 	this->currTrajectory = _trajectory;
	// 	// 	this->duration = _duration;
	// 	// 	this->tCurr = 0.0;
	// 	// 	this->startTime = ros::Time::now();
	// 	// 	this->timestep = this->duration/(this->trajectory.poses.size()-1);
	// 	// 	if (not this->init){
	// 	// 		this->init = true;
	// 	// 	}
	// 	// }

	// 	void updateTrajectory(const nav_msgs::Path& _trajectory){
	// 		this->trajectory = _trajectory;
	// 		this->currTrajectory = _trajectory;
	// 		this->tCurr = 0.0;
	// 		this->startTime = ros::Time::now();
	// 		// this->timestep = this->duration/(this->trajectory.poses.size()-1);
	// 		if (not this->init){
	// 			this->init = true;
	// 		}
	// 	}

	// 	int getCurrIdx(){
	// 		ros::Time currTime = ros::Time::now();
	// 		this->tCurr = (currTime - this->startTime).toSec() + this->timestep;
	// 		if (this->tCurr > this->duration){
	// 			this->tCurr = this->duration;
	// 		}

	// 		int idx = floor(this->tCurr/this->timestep);
	// 		return idx;
	// 	}

	// 	geometry_msgs::PoseStamped getPose(){
	// 		int idx = this->getCurrIdx();
	// 		idx = std::max(idx+forwardIdx, minIdx);
	// 		int newIdx = std::min(idx, int(this->trajectory.poses.size()-1));

	// 		std::vector<geometry_msgs::PoseStamped> pathVec;
	// 		for (size_t i=idx; i<this->trajectory.poses.size(); ++i){
	// 			pathVec.push_back(this->trajectory.poses[i]);
	// 		}
	// 		this->currTrajectory.poses = pathVec;
	// 		return this->trajectory.poses[newIdx];
	// 	}

	// 	geometry_msgs::PoseStamped getPose(const geometry_msgs::Pose& psCurr){
	// 		int idx = this->getCurrIdx();
	// 		idx = std::max(idx+forwardIdx, minIdx);
	// 		int newIdx = std::min(idx, int(this->trajectory.poses.size()-1));

	// 		std::vector<geometry_msgs::PoseStamped> pathVec;
	// 		geometry_msgs::PoseStamped psFirst;
	// 		psFirst.pose = psCurr;
	// 		pathVec.push_back(psFirst);
	// 		for (size_t i=idx; i<this->trajectory.poses.size(); ++i){
	// 			pathVec.push_back(this->trajectory.poses[i]);
	// 		}
	// 		this->currTrajectory.poses = pathVec;
	// 		return this->trajectory.poses[newIdx];
	// 	}

	// 	geometry_msgs::PoseStamped getPoseWithoutYaw(const geometry_msgs::Pose& psCurr){
	// 		int idx = this->getCurrIdx();
	// 		idx = std::max(idx+forwardIdx, minIdx);
	// 		int newIdx = std::min(idx, int(this->trajectory.poses.size()-1));

	// 		std::vector<geometry_msgs::PoseStamped> pathVec;
	// 		geometry_msgs::PoseStamped psFirst;
	// 		psFirst.pose = psCurr;
	// 		pathVec.push_back(psFirst);
	// 		for (size_t i=idx; i<this->trajectory.poses.size(); ++i){
	// 			pathVec.push_back(this->trajectory.poses[i]);
	// 		}
	// 		this->currTrajectory.poses = pathVec;
	// 		geometry_msgs::PoseStamped psTarget = this->trajectory.poses[newIdx];
	// 		psTarget.pose.orientation = psCurr.orientation;
	// 		return psTarget;	
	// 	}

	// 	void stop(const geometry_msgs::Pose& psCurr){
	// 		std::vector<geometry_msgs::PoseStamped> pathVec;
	// 		geometry_msgs::PoseStamped ps;
	// 		ps.pose = psCurr;
	// 		pathVec.push_back(ps);
	// 		this->trajectory.poses = pathVec;
	// 		this->currTrajectory = this->trajectory;
	// 		this->tCurr = 0.0;
	// 		this->startTime = ros::Time::now();
	// 		this->duration = this->timestep; 
	// 	}

	// 	double getRemainTime(){
	// 		ros::Time currTime = ros::Time::now();
	// 		double tCurr = (currTime - this->startTime).toSec() + this->timestep;
	// 		return this->duration - tCurr;
	// 	}

	// 	bool needReplan(double factor){
	// 		if (this->getRemainTime() <= this->duration * (1 - factor)){
	// 			return true;
	// 		}
	// 		else{
	// 			return false;
	// 		}
	// 	}
	// };


	// class flightBase{
	// private:

	// protected:
	// 	ros::NodeHandle nh_;
	// 	ros::Subscriber stateSub_;
	// 	ros::Subscriber odomSub_;
	// 	ros::Subscriber clickSub_;
	// 	ros::Publisher posePub_;
	// 	ros::ServiceClient armClient_;
	// 	ros::ServiceClient setModeClient_;
		
	// 	nav_msgs::Odometry odom_;
	// 	mavros_msgs::State mavrosState_;
	// 	geometry_msgs::PoseStamped poseTgt_;
	// 	geometry_msgs::PoseStamped goal_;
		
	// 	// parameters
	// 	double sampleTime_;
	// 	double takeoffHgt_;

	// 	// status
	// 	bool odomReceived_;
	// 	bool mavrosStateReceived_;
	// 	bool firstGoal_ = false;
	// 	bool goalReceived_ = false;


	// public:
	// 	std::thread posePubWorker_;

	// 	flightBase(ros::NodeHandle& nh);
	// 	~flightBase();
	// 	void takeoff();
	// 	void updateTarget(const geometry_msgs::PoseStamped& ps);
	// 	void run();
	// 	void pubPose();
	// 	void stateCB(const mavros_msgs::State::ConstPtr& state);
	// 	void odomCB(const nav_msgs::Odometry::ConstPtr& odom);
	// 	void clickCB(const geometry_msgs::PoseStamped::ConstPtr& cp);
	// 	bool isReach(const geometry_msgs::PoseStamped& poseTgt, bool useYaw=true);


	// 	inline void setOdomReceived(const bool &received){
	// 		this->odomReceived_ = received;
	// 	}

	// 	// utils
	// 	inline double rpy_from_quaternion(const geometry_msgs::Quaternion& quat){
	// 		// return is [0, 2pi]
	// 		tf2::Quaternion tf_quat;
	// 		tf2::convert(quat, tf_quat);
	// 		double roll, pitch, yaw;
	// 		tf2::Matrix3x3(tf_quat).getRPY(roll, pitch, yaw);
	// 		return yaw;
	// 	}

	// 	inline void rpy_from_quaternion(const geometry_msgs::Quaternion& quat, double &roll, double &pitch, double &yaw){
	// 		tf2::Quaternion tf_quat;
	// 		tf2::convert(quat, tf_quat);
	// 		tf2::Matrix3x3(tf_quat).getRPY(roll, pitch, yaw);
	// 	}
	// };

#endif
