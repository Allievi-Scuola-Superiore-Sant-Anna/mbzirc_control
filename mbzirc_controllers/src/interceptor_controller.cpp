#include "ros/ros.h"
#include <math.h>
#include<cstdlib>
#include <iostream>
 #include <eigen3/Eigen/Core>
#include <std_msgs/Float64.h>
#include <std_msgs/String.h>
#include<algorithm>
#include "geometry_msgs/Point.h"
#include <geometry_msgs/TwistStamped.h>
#include <geometry_msgs/PoseStamped.h>
#include <mavros_msgs/State.h>
#include <nav_msgs/Odometry.h>

#define NPAST 2


Eigen::VectorXd     currTargetPos(3);
Eigen::VectorXd     lastTargetPos(3); //current target pos=0-2; past:3-5
Eigen::VectorXd     dronePose(7);
Eigen::VectorXd     aux(3);
Eigen::VectorXd     currDronePos(3);
Eigen::VectorXd     droneTwist(6);
Eigen::VectorXd     targetLinearVel(3);
Eigen::VectorXd     targetTimes(2);
bool  searchInit;
bool  chaseInit;
std::string task_id;

mavros_msgs::State current_state;

float globalTime=0.1;

int VEL_CAP=7;
float K=0.2;

float dumb_min(float a, float b){
  if(a<=b) return a;
  return b;
}

void targetPosCallback( const geometry_msgs::Point::ConstPtr& target_pos ) {
  // Update the target state of the AUV
  //# TODO: change point in PointStamped
  lastTargetPos(0)=currTargetPos(0);
  lastTargetPos(1)=currTargetPos(1);
  lastTargetPos(2)=currTargetPos(2);

  currTargetPos(0) = target_pos->x;
  currTargetPos(1) = target_pos->y;
  currTargetPos(2) = target_pos->z; //#TODO: change this in target_pos->point.z

  // #TODO: TARGET TIMES!
  //see http://docs.ros.org/api/std_msgs/html/msg/Header.html for time
  targetTimes(1)=targetTimes(0);
  targetTimes(0)=globalTime;
  globalTime+=0.1;
}

void TaskIDCallback( const std_msgs::String::ConstPtr& task )  {
    task_id = task->data;
    searchInit = 1;
    chaseInit = 1;
}

 void stateCallback(const mavros_msgs::State::ConstPtr& msg){
     current_state = *msg;
 }

  void OdomCallback(const nav_msgs::Odometry::ConstPtr& odometry){
      currDronePos(0)=dronePose(0) = odometry->pose.pose.position.x;
      currDronePos(1)=dronePose(1) = odometry->pose.pose.position.y;
      currDronePos(2)=dronePose(2) = odometry->pose.pose.position.z;
      dronePose(3) = odometry->pose.pose.orientation.x;
      dronePose(4) = odometry->pose.pose.orientation.y;
      dronePose(5) = odometry->pose.pose.orientation.z;
      dronePose(6) = odometry->pose.pose.orientation.w;
      droneTwist(0) = odometry->twist.twist.linear.x;
      droneTwist(1) = odometry->twist.twist.linear.y;
      droneTwist(2) = odometry->twist.twist.linear.z;
      droneTwist(3) = odometry->twist.twist.angular.x;
      droneTwist(4) = odometry->twist.twist.angular.y;
      droneTwist(5) = odometry->twist.twist.angular.z;
  }

int main(int argc, char **argv)
{

  std::cout<< "immalive\n" ;//debug
  ros::init(argc, argv, "interceptor_controller");

  ros::NodeHandle n;

  // CREATE ROS PUBLISH OBJECTS
  ros::Publisher commandVel_pub = n.advertise<geometry_msgs::TwistStamped>("mavros/setpoint_velocity/cmd_vel", 1);
  ros::Publisher commandPos_pub = n.advertise<geometry_msgs::PoseStamped>("mavros/setpoint_position/local", 1);

  // SUBSCRIBE TO TOPICS
  ros::Subscriber targetPosSub = n.subscribe("target_pos",1, targetPosCallback);
  ros::Subscriber state_sub = n.subscribe("mavros/state", 1, stateCallback);
  ros::Subscriber taskID_sub = n.subscribe("/smach/task_id", 1, TaskIDCallback);
  ros::Subscriber odom_sub = n.subscribe("/mavros/local_position/odom", 1, OdomCallback);

  ros::Rate loop_rate(10);

  geometry_msgs::TwistStamped           velCmd;
  geometry_msgs::PoseStamped            posRef;

  currTargetPos << 0,0,0;
  lastTargetPos << 0,0,0;
  currDronePos << 0,0,0;
  targetTimes << 0,0;
  task_id = "IDLE";

  double          secs_start;
  double          secs_fin;
  double          Dt = 0;

  while ( ros::ok() )
  {
    // std::cout<< "ros is still spinning;task type:" ;//debug
    // std::cout<< task_id.c_str()<<std::endl ;//debug
    if( !strcmp(task_id.c_str(),"SEARCH" ) ) {

        if(searchInit)
          secs_start = ros::Time::now().toSec();
        secs_fin = ros::Time::now().toSec();
        Dt = secs_fin - secs_start;
        short search_counter=1;
        //                  DA AGGIUSTARE !!!!!!!!
        // La gestione coi tempi è un po' una porcata, in futuro meglio mettere
        // un altro tipo di parametro magari posizionale o simile
        if( Dt<10 )  {
        velCmd.twist.linear.x = 0;
        velCmd.twist.linear.y = 0;
        velCmd.twist.linear.z = 0;
        velCmd.twist.angular.x = 0;
        velCmd.twist.angular.y = 0;
        velCmd.twist.angular.z = search_counter;
        commandVel_pub.publish(velCmd);
        }
        else  {
            secs_start = ros::Time::now().toSec();
            search_counter=search_counter*-1;
        }
    searchInit=0;
    } //end of search state if

    if( !strcmp(task_id.c_str(),"CHASE" ) ) {

      for(int pippo=0; pippo<3; pippo++){
        targetLinearVel(pippo)=(currTargetPos(pippo)-lastTargetPos(pippo))/
                                (targetTimes(0)-targetTimes(1));
      }
      aux=currTargetPos-currDronePos; //vector from drone to TGT
      float dist=aux.norm();
      float vel=dumb_min(VEL_CAP,K*dist);
      // aux=currTargetPos+ targetLinearVel*(vel/dist); //predicted position of the TGT
      // aux=aux-currDronePos; //vector connecting from drone to the predicted position of tgt
      /*
      vel=vel*aux.norm()/dist;
      dist=aux.norm();
      needs also vel=std::min(VEL_CAP,vel);
      this algorithms has some mathematical fallacies, but it's also simple and working
      so this part is not necessary
      */
      aux=aux*(vel/aux.norm()); //normalization plus velocity vector
      velCmd.twist.linear.x = aux(0);
      velCmd.twist.linear.y = aux(1);
      velCmd.twist.linear.z = aux(2);
      if(chaseInit){
        velCmd.twist.linear.x = 0;
        velCmd.twist.linear.y = 0;
        velCmd.twist.linear.z = 0;
        chaseInit=0;
      }

      velCmd.twist.angular.x = 0;
      velCmd.twist.angular.y = 0;
      velCmd.twist.angular.z = 0;
      commandVel_pub.publish(velCmd);
    }//end of chase if

    //#TODO:failsafe PLEASE NOTE: if no valid task id is given, then the node will publish no commands!
    ros::spinOnce(); //call all the callbacks once
    loop_rate.sleep();
    secs_fin = ros::Time::now().toSec();
  }//end of ros main loop


  return 0;
}
