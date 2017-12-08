/***************************************************************************
 *  mongodb_log_desig.cpp - MongoDB Logger for designator_integration
 *
 *  Copyright  2012  Jan Winkler, Universität Bremen,
 *                   Institute for Artificial Intelligence
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version. A runtime exception applies to
 *  this software (see LICENSE.GPL_WRE file mentioned below for details).
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL_WRE file in the doc directory.
 */

// System
#include <iostream>

// ROS
#include <ros/ros.h>

// MongoDB
#include "mongo_interface.h"

// Designator Integration
#include <designator_integration_msgs/DesignatorRequest.h>
#include <designator_integration_msgs/DesignatorResponse.h>
#include <designator_integration_msgs/Designator.h>


DECLARE_MONGO_CONNECTION(dbMongoDB)
std::string strCollection;
std::string strTopic;

unsigned int in_counter;
unsigned int out_counter;
unsigned int qsize;
unsigned int drop_counter;

static pthread_mutex_t in_counter_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t out_counter_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t drop_counter_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t qsize_mutex = PTHREAD_MUTEX_INITIALIZER;


enum OperationMode {
  DESIGNATOR = 0,
  DESIGNATOR_REQUEST = 1,
  DESIGNATOR_RESPONSE = 2
};


DataObject keyValuePairToBSON(designator_integration_msgs::KeyValuePair* kvpPair) {
  DataObjectBuilder bobBuilder;
  
  if(kvpPair->type() == designator_integration_msgs::KeyValuePair::ValueType::STRING) {
    builderAppend(bobBuilder, kvpPair->key(), kvpPair->stringValue());
  } else if(kvpPair->type() == designator_integration_msgs::KeyValuePair::ValueType::FLOAT) {
    builderAppend(bobBuilder, kvpPair->key(), kvpPair->floatValue());
  } else if(kvpPair->type() == designator_integration_msgs::KeyValuePair::ValueType::POSE) {
    geometry_msgs::Pose psPose = kvpPair->poseValue();
    DataObjectBuilder bobTransform;
    builderAppend(bobTransform, "position",
            MAKE_BSON_DATA_OBJECT(   "x" << psPose.position.x
			     << "y" << psPose.position.y
			     << "z" << psPose.position.z));
    builderAppend(bobTransform, "orientation",
            MAKE_BSON_DATA_OBJECT(   "x" << psPose.orientation.x
			     << "y" << psPose.orientation.y
			     << "z" << psPose.orientation.z
			     << "w" << psPose.orientation.w));
    builderAppend(bobBuilder, kvpPair->key(), builderGetObject(bobTransform));
  } else if(kvpPair->type() == designator_integration_msgs::KeyValuePair::ValueType::POSESTAMPED) {
    geometry_msgs::PoseStamped psPoseStamped = kvpPair->poseStampedValue();
    BSONDate stamp = MAKE_BSON_DATE(psPoseStamped.header.stamp.sec * 1000.0 + psPoseStamped.header.stamp.nsec / 1000000.0);
    
    DataObjectBuilder bobTransformStamped;
    DataObjectBuilder bobTransform;
    builderAppend(bobTransformStamped, "header",
                   MAKE_BSON_DATA_OBJECT(   "seq" << ((MongoInt)psPoseStamped.header.seq)
				    << "stamp" << stamp
				    << "frame_id" << psPoseStamped.header.frame_id));
    builderAppend(bobTransform, "position",
                MAKE_BSON_DATA_OBJECT(   "x" << psPoseStamped.pose.position.x
    			     << "y" << psPoseStamped.pose.position.y
    			     << "z" << psPoseStamped.pose.position.z));
    builderAppend(bobTransform, "orientation",
                MAKE_BSON_DATA_OBJECT(   "x" << psPoseStamped.pose.orientation.x
    			     << "y" << psPoseStamped.pose.orientation.y
    			     << "z" << psPoseStamped.pose.orientation.z
    			     << "w" << psPoseStamped.pose.orientation.w));
    builderAppend(bobTransformStamped, "pose", builderGetObject(bobTransform));
    builderAppend(bobBuilder, kvpPair->key(), builderGetObject(bobTransformStamped));
  } else if(kvpPair->type() == designator_integration_msgs::KeyValuePair::ValueType::LIST) {
    DataObjectBuilder bobChildren;
    std::list<designator_integration_msgs::KeyValuePair*> lstChildren = kvpPair->children();
    
    for(designator_integration_msgs::KeyValuePair* kvpChild : lstChildren) {
      builderAppendElements(bobChildren, keyValuePairToBSON(kvpChild));
    }
    
    builderAppend(bobBuilder, kvpPair->key(), builderGetObject(bobChildren));
  }
  
  return builderGetObject(bobBuilder);
}


void logDesignator(designator_integration_msgs::Designator* desigLog) {
  DataObjectBuilder bobDesig;
  std::vector<DataObject> vecChildren;
  
  std::list<designator_integration_msgs::KeyValuePair*> lstChildren = desigLog->children();
  
  for(designator_integration_msgs::KeyValuePair* kvpChild : lstChildren) {
    builderAppendElements(bobDesig, keyValuePairToBSON(kvpChild));
  }
  
  std::string strDesigType;
  
  switch(desigLog->type()) {
  case designator_integration_msgs::Designator::DesignatorType::ACTION:
    strDesigType = "action";
    break;
    
  case designator_integration_msgs::Designator::DesignatorType::OBJECT:
    strDesigType = "object";
    break;
    
  case designator_integration_msgs::Designator::DesignatorType::LOCATION:
    strDesigType = "location";
    break;
    
  default:
    strDesigType = "unknown";
    break;
  }
  
  builderAppend(bobDesig, "_designator_type", strDesigType);

  DataObjectBuilder dbEntry;

  builderAppend(dbEntry, "designator", builderGetObject(bobDesig));
  builderAppend(dbEntry, "__recorded", MAKE_BSON_DATE(time(NULL) * 1000));
  builderAppend(dbEntry, "__topic", strTopic);
  
  insertOne(dbMongoDB, strCollection, builderGetObject(dbEntry));
  
  pthread_mutex_lock(&in_counter_mutex);
  ++in_counter;
  pthread_mutex_unlock(&in_counter_mutex);
  pthread_mutex_lock(&out_counter_mutex);
  ++out_counter;
  pthread_mutex_unlock(&out_counter_mutex);
}


void cbDesignatorResponseMsg(const designator_integration_msgs::DesignatorResponse& msg) {
  std::vector<designator_integration_msgs::Designator> vecDesigs = msg.designators;
  
  for(designator_integration_msgs::Designator desigmsgCurrent : vecDesigs) {
    designator_integration_msgs::Designator* desigLog = new designator_integration_msgs::Designator(desigmsgCurrent);
    logDesignator(desigLog);
    
    delete desigLog;
  }
}


void cbDesignatorRequestMsg(const designator_integration_msgs::DesignatorRequest& msg) {
  designator_integration_msgs::Designator* desigLog = new designator_integration_msgs::Designator(msg.designator);
  logDesignator(desigLog);
  
  delete desigLog;
}


void cbDesignatorMsg(const designator_integration_msgs::Designator& msg) {
  designator_integration_msgs::Designator* desigLog = new designator_integration_msgs::Designator(msg);
  logDesignator(desigLog);
  
  delete desigLog;
}


void printCount(const ros::TimerEvent& te) {
  unsigned int l_in_counter, l_out_counter, l_drop_counter, l_qsize;
  
  pthread_mutex_lock(&in_counter_mutex);
  l_in_counter = in_counter; in_counter = 0;
  pthread_mutex_unlock(&in_counter_mutex);
  
  pthread_mutex_lock(&out_counter_mutex);
  l_out_counter = out_counter; out_counter = 0;
  pthread_mutex_unlock(&out_counter_mutex);
  
  pthread_mutex_lock(&drop_counter_mutex);
  l_drop_counter = drop_counter; drop_counter = 0;
  pthread_mutex_unlock(&drop_counter_mutex);
  
  pthread_mutex_lock(&qsize_mutex);
  l_qsize = qsize; qsize = 0;
  pthread_mutex_unlock(&qsize_mutex);
  
  printf("%u:%u:%u:%u\n", l_in_counter, l_out_counter, l_drop_counter, l_qsize);
  fflush(stdout);
}


void initialize() {
  strCollection = "";
  strTopic = "";
  
  in_counter = 0;
  out_counter = 0;
  drop_counter = 0;
  qsize = 0;
}


int main(int argc, char** argv) {
  int nReturnvalue = 0;
  initialize();
  
  std::string strMongoDBHostname = "localhost";
  std::string strNodename = "";
  
  enum OperationMode enOpMode = DESIGNATOR;
  
  char cToken = '\0';
  while((cToken = getopt(argc, argv, "t:m:n:c:d:")) != -1) {
    if((cToken == '?') || (cToken == ':')) {
      printf("Usage: %s -t topic -m mongodb -n nodename -c collection [-d <designator|list>]\n", argv[0]);
      
      nReturnvalue = -1;
      break;
    } else if(cToken == 't') {
      strTopic = optarg;
    } else if(cToken == 'm') {
      strMongoDBHostname = optarg;
    } else if(cToken == 'n') {
      strNodename = optarg;
    } else if(cToken == 'c') {
      strCollection = optarg;
    } else if(cToken == 'd') {
      std::string strArgument = optarg;
      
      if(strArgument == "designator") {
	enOpMode = DESIGNATOR;
      } else if(strArgument == "designator-request") {
	enOpMode = DESIGNATOR_REQUEST;
      } else if(strArgument == "designator-response") {
	enOpMode = DESIGNATOR_RESPONSE;
      } else {
	// ROS_WARN("Designator mode (-d) was set to `%s' which is invalid.", strArgument.c_str());
	// ROS_WARN("Valid options are: designator, designator-request, designator-response.");
	// ROS_WARN("Assuming the standard value: designator");
      }
    }
  }
  
  if(nReturnvalue == 0) {
    if(strTopic == "") {
      ROS_ERROR("No topic given.");
      nReturnvalue = -2;
    } else if(strNodename == "") {
      ROS_ERROR("No node name given.");
      nReturnvalue = -2;
    } else {
      ros::init(argc, argv, strNodename);
      
      if(ros::ok()) {
	ros::NodeHandle nh;
	
    std::string strError;

    if(ConnectToDatabase(dbMongoDB, strMongoDBHostname, strCollection, strError)) {
	  ros::Subscriber subTopic;
	  
	  switch(enOpMode) {
	  case DESIGNATOR: {
	    subTopic = nh.subscribe(strTopic, 1000, cbDesignatorMsg);
	  } break;
	    
	  case DESIGNATOR_REQUEST: {
	    subTopic = nh.subscribe(strTopic, 1000, cbDesignatorRequestMsg);
	  } break;
	    
	  case DESIGNATOR_RESPONSE: {
	    subTopic = nh.subscribe(strTopic, 1000, cbDesignatorResponseMsg);
	  } break;
	    
	  default: {
	    ROS_ERROR("Invalid topic type selected. This is a serious error which shouldn't be happening.");
	    nReturnvalue = -2;
	  } break;
	  }
	  
	  if(nReturnvalue == 0) {
	    ros::Timer tmrPrintCount = nh.createTimer(ros::Duration(5, 0), printCount);
	    ros::spin();
	  }
	  
	  delete dbMongoDB;
	} else {
	  ROS_ERROR("Failed to connect to MongoDB: %s", strError.c_str());
	  nReturnvalue = -1;
	}
	
	ros::shutdown();
      } else {
	ROS_ERROR("ROS initialization failed.");
      }
    }
  }
  
  return nReturnvalue;
}
