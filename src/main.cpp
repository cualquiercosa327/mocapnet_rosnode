#include <stdexcept>
#include <image_transport/image_transport.h>

#include <message_filters/subscriber.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <message_filters/time_synchronizer.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/CameraInfo.h>

#include <std_msgs/Float32MultiArray.h>

#include <tf2/LinearMath/Quaternion.h>
#include <tf2_ros/transform_broadcaster.h>
#include <geometry_msgs/TransformStamped.h>
//#include <tf/transform_broadcaster.h>
#include <std_srvs/Empty.h>
#include "mocapnet_rosnode/singleFloat.h"


#include <math.h>
#include <iostream>
#include <unistd.h>
#include <ros/ros.h>
#include <ros/spinner.h>


#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>


//MocapNET includes..
//-----------------------------------------------------------------
#include "../dependencies/MocapNET/src/JointEstimator2D/cameraControl.hpp"
#include "../dependencies/MocapNET/src/JointEstimator2D/jointEstimator2D.hpp"
#include "../dependencies/MocapNET/src/JointEstimator2D/visualization.hpp"
//-----------------------------------------------------------------
#include "../dependencies/MocapNET/src/MocapNET2/MocapNETLib2/mocapnet2.hpp"
#include "../dependencies/MocapNET/src/MocapNET2/MocapNETLib2/applicationLogic/parseCommandlineOptions.hpp"
#include "../dependencies/MocapNET/src/MocapNET2/MocapNETLib2/IO/commonSkeleton.hpp"
#include "../dependencies/MocapNET/src/MocapNET2/MocapNETLib2/IO/conversions.hpp"
#include "../dependencies/MocapNET/src/MocapNET2/MocapNETLib2/IO/bvh.hpp"
#include "../dependencies/MocapNET/src/MocapNET2/MocapNETLib2/IO/csvRead.hpp"
#include "../dependencies/MocapNET/src/MocapNET2/MocapNETLib2/IO/csvWrite.hpp"
#include "../dependencies/MocapNET/src/MocapNET2/MocapNETLib2/IO/skeletonAbstraction.hpp"
//-----------------------------------------------------------------
#include "../dependencies/MocapNET/src/MocapNET2/MocapNETLib2/visualization/visualization.hpp"
#include "../dependencies/MocapNET/src/MocapNET2/MocapNETLib2/visualization/map.hpp"
//-----------------------------------------------------------------
#include "../dependencies/MocapNET/src/MocapNET2/MocapNETLib2/tools.hpp"

//BVH Specific stuff..
#include "../dependencies/MocapNET/dependencies/RGBDAcquisition/tools/AmMatrix/matrixTools.h"
#include "../dependencies/MocapNET/dependencies/RGBDAcquisition/tools/AmMatrix/matrix4x4Tools.h"
#include "../dependencies/MocapNET/dependencies/RGBDAcquisition/tools/AmMatrix/quaternions.h"
#include "../dependencies/MocapNET/dependencies/RGBDAcquisition/opengl_acquisition_shared_library/opengl_depth_and_color_renderer/src/Library/MotionCaptureLoader/bvh_loader.h"
#include "../dependencies/MocapNET/dependencies/RGBDAcquisition/opengl_acquisition_shared_library/opengl_depth_and_color_renderer/src/Library/MotionCaptureLoader/calculate/bvh_project.h"
#include "../dependencies/MocapNET/dependencies/RGBDAcquisition/opengl_acquisition_shared_library/opengl_depth_and_color_renderer/src/Library/MotionCaptureLoader/calculate/bvh_transform.h"

struct MocapNET2Options options= {0};
struct MocapNET2 mnet= {0};
struct JointEstimator2D jointEstimator;

struct BVH_MotionCapture bvhMotion = {0};
struct BVH_Transform bvhTransform  = {0};

unsigned int frameID=0;
  
struct calibration
{
    /* CAMERA INTRINSIC PARAMETERS */
    char intrinsicParametersSet;
    float intrinsic[9];
    float k1,k2,p1,p2,k3;

    float cx,cy,fx,fy;
};

#define camRGBRaw "/camera/rgb/image_rect_color"
#define camRGBInfo "/camera/rgb/camera_info"
#define DEFAULT_TF_ROOT "map"
unsigned int width=640;
unsigned int height=480;

int useSimpleBroadcaster=0;
    
//Configuration
char tfRoot[512]={DEFAULT_TF_ROOT}; //This is the string of the root node on the TF tree

int publishCameraTF=1;
float cameraXPosition=0.0,cameraYPosition=0.0,cameraZPosition=0.0;
float cameraRoll=90.0, cameraPitch=0.0, cameraYaw=0.0;
unsigned int startTime=0, currentTime=0;


sensor_msgs::CameraInfo camInfo;

volatile bool startTrackingSwitch = false;
volatile bool stopTrackingSwitch = false;
volatile int  key=0;

//ROS
typedef message_filters::sync_policies::ApproximateTime<sensor_msgs::Image, sensor_msgs::CameraInfo> RgbSyncPolicy;


ros::NodeHandle * nhPtr=0;
//RGB Subscribers
message_filters::Subscriber<sensor_msgs::Image> *rgb_img_sub;
message_filters::Subscriber<sensor_msgs::CameraInfo> *rgb_cam_info_sub;


bool setCameraXPosition(mocapnet_rosnode::singleFloat::Request  &req,mocapnet_rosnode::singleFloat::Response &res)
    {
      cameraXPosition = req.value;
      res.ok = 1;
      ROS_INFO("Camera X Position set to %f",cameraXPosition); 
      return true;
   }

bool setCameraYPosition(mocapnet_rosnode::singleFloat::Request  &req,mocapnet_rosnode::singleFloat::Response &res)
    {
      cameraYPosition = req.value;
      res.ok = 1;
      ROS_INFO("Camera Y Position set to %f",cameraYPosition); 
      return true;
   }

bool setCameraZPosition(mocapnet_rosnode::singleFloat::Request  &req,mocapnet_rosnode::singleFloat::Response &res)
    {
      cameraZPosition = req.value;
      res.ok = 1;
      ROS_INFO("Camera Z Position set to %f",cameraZPosition); 
      return true;
   }

bool setCameraRoll(mocapnet_rosnode::singleFloat::Request  &req,mocapnet_rosnode::singleFloat::Response &res)
    {
      cameraRoll = req.value;
      res.ok = 1;
      ROS_INFO("TF Roll Offset set to %f",cameraRoll); 
      return true;
   }

bool setCameraPitch(mocapnet_rosnode::singleFloat::Request  &req,mocapnet_rosnode::singleFloat::Response &res)
    {
      cameraPitch = req.value;
      res.ok = 1;
      ROS_INFO("TF Pitch Offset set to %f",cameraPitch); 
      return true;
   }
   
bool setCameraYaw(mocapnet_rosnode::singleFloat::Request  &req,mocapnet_rosnode::singleFloat::Response &res)
    {
      cameraYaw = req.value;
      res.ok = 1;
      ROS_INFO("TF Yaw Offset set to %f",cameraYaw); 
      return true;
   }
   
   
bool visualizeAngles(std_srvs::Empty::Request& request,std_srvs::Empty::Response& response)
{
    ROS_INFO("Visualize Angles called");
    options.visualizationType=1;
    return true;
}

bool visualizeMain(std_srvs::Empty::Request& request,std_srvs::Empty::Response& response)
{
    ROS_INFO("Visualize Main called");
    options.visualizationType=0;
    return true;
}

bool visualizeOverlay(std_srvs::Empty::Request& request,std_srvs::Empty::Response& response)
{
    ROS_INFO("Visualize Overlay called");
    options.visualizationType=3;
    return true;
}

bool visualizeOff(std_srvs::Empty::Request& request,std_srvs::Empty::Response& response)
{
    ROS_INFO("Visualize Off called");
    return true;
}

bool terminate(std_srvs::Empty::Request& request,std_srvs::Empty::Response& response)
{
    ROS_INFO("Terminating MocapNET");
    exit(0);
    return true;
}


void mocapNETProcessImage(cv::Mat frame)
{
    
    std::vector<float> inputValues;
    std::vector<std::vector<float> > exactMocapNET2DOutput;
    std::vector<std::vector<float> > output3DPositions;
    std::vector<float> points3DFlatOutput;
                    unsigned int skippedFramesInARow=0;

    struct skeletonSerialized resultAsSkeletonSerialized= {0};
     
    float frameRateSummary = 0.0;
    unsigned int frameSamples=0;

    std::vector<float> result;
    std::vector<float> previousResult;

    float totalTime=0.0;
    unsigned int totalSamples=0;
    
    std::vector<std::vector<float> > bvhFrames;
    struct Skeletons2DDetected skeleton2DEstimations= {0};
    struct skeletonSerialized skeleton= {0};
                    
                    
    cv::Mat viewMat = Mat(Size(jointEstimator.inputWidth2DJointDetector,jointEstimator.inputHeight2DJointDetector),CV_8UC3, Scalar(0,0,0));
    cv::Mat frameCentered;
    frame.copyTo(frameCentered);
    if ( (frameCentered.size().width>0) && (frameCentered.size().height>0) )
                                {
                                    if (
                                        !cropAndResizeCVMatToMatchSkeleton(
                                            &jointEstimator,
                                            frameCentered,
                                            &skeleton2DEstimations
                                          )
                                        )
                                        {
                                            fprintf(stderr,"Failed to crop input video\n");
                                        }
                                    //imshow("Video Input Feed", frameCentered);

                                    frameCentered.copyTo(viewMat);
                                    // viewMat.setTo(Scalar(0,0,0));

                                    //Tensorflow works with Floating point input so we need to convert our buffer..
                                    frameCentered.convertTo(frameCentered,CV_32FC3); 
                                    
                                    //At this point we are ready to execute the neural network
                                    long startTime2D = GetTickCountMicrosecondsMN();
                                    
                                    //We count the framerate of our acquisition
                                    options.fpsAcquisition = convertStartEndTimeFromMicrosecondsToFPS(options.loopStartTime,startTime2D);
                                    
                                    
                                    std::vector<std::vector<float> >  heatmaps = getHeatmaps(
                                                &jointEstimator,
                                                frameCentered.data,
                                                jointEstimator.inputWidth2DJointDetector,
                                                jointEstimator.inputHeight2DJointDetector
                                            );
                                    if (heatmaps.size()>0)
                                        {
                                            //This will spam with small heatmap windows
                                            //visualizeHeatmaps(&jointEstimator,heatmaps,frameID);

                                            estimate2DSkeletonsFromHeatmaps(&jointEstimator,&skeleton2DEstimations,heatmaps);

                                            long endTime2D = GetTickCountMicrosecondsMN();

                                            options.fps2DEstimator = convertStartEndTimeFromMicrosecondsToFPS(startTime2D,endTime2D);
                                            
                                            if (options.visualize)
                                            {
                                               dj_drawExtractedSkeletons(
                                                viewMat,
                                                &skeleton2DEstimations,
                                                jointEstimator.inputWidth2DJointDetector,
                                                jointEstimator.inputHeight2DJointDetector
                                                );
                                            }

                                            float percentageOf2DPointsMissing = percentOf2DPointsMissing(&skeleton2DEstimations);
                                            if (  percentageOf2DPointsMissing  < 50.0 ) //only work when less than 50% of information missing..
                                                {
                                                    skippedFramesInARow=0;
                                                    //We want to go from the original normalized values of skeleton2DEstimations to the original
                                                    //Resolution we grabbed our initial frame @ before cropping..
                                                    restore2DJointsToInputFrameCoordinates(&jointEstimator,&skeleton2DEstimations);

                                                    //Now that our points have their initial size let's perform a conversion to the internal
                                                    //serialized skeleton data structure that will prepare them for use in MocapNET
                                                    convertSkeletons2DDetectedToSkeletonsSerialized(
                                                        &skeleton,
                                                        &skeleton2DEstimations,
                                                        frameID,
                                                        jointEstimator.crop.frameWidth,
                                                        jointEstimator.crop.frameHeight
                                                    );

                                                    takeCareOfScalingInputAndAddingNoiseAccordingToOptions(&options,&skeleton);

                                                    unsigned int feetAreMissing=areFeetMissing(&skeleton);


                                                    long startTime = GetTickCountMicrosecondsMN();
                                                    //--------------------------------------------------------
                                                    previousResult = result;
                                                    result = runMocapNET2(
                                                                 &mnet,
                                                                 &skeleton,
                                                                 ( (options.doLowerBody) && (!feetAreMissing) ),
                                                                 options.doHands,
                                                                 options.doFace,
                                                                 options.doGestureDetection,
                                                                 options.useInverseKinematics,
                                                                 options.doOutputFiltering
                                                             );
                                                    bvhFrames.push_back(result);
                                                    //--------------------------------------------------------
                                                    long endTime = GetTickCountMicrosecondsMN();
                                                    options.fpsMocapNET = convertStartEndTimeFromMicrosecondsToFPS(startTime,endTime);
                                                    frameRateSummary += options.fpsMocapNET; 
                                                    ++frameSamples;
                                                    //--------------------------------------------------------
                                                    

                                                    options.numberOfMissingJoints = upperbodyCountMissingNSDMElements(mnet.upperBody.NSDM,0 /*Dont spam */);
                                                    //Don't spam with missing joints..
                                                    //fprintf(stderr,"Number of missing joints for UpperBody %u\n",options.numberOfMissingJoints);

                                                    //Convert BVH frame to 2D points to show on screen
                                                    exactMocapNET2DOutput = convertBVHFrameTo2DPoints(result);//,MocapNETTrainingWidth,MocapNETTrainingHeight);

                                                    if (options.saveCSV3DFile)
                                                        {
                                                            //Convert BVH frame to 3D points to output on a file
                                                            points3DFlatOutput=convertBVHFrameToFlat3DPoints(result);//,MocapNETTrainingWidth,MocapNETTrainingHeight);
                                                            output3DPositions.push_back(points3DFlatOutput); //3d Input
                                                        }

                                                    resultAsSkeletonSerialized.skeletonHeaderElements = skeleton.skeletonHeaderElements;
                                                    resultAsSkeletonSerialized.skeletonBodyElements   = skeleton.skeletonBodyElements;
                                                    if (
                                                        convertMocapNET2OutputToSkeletonSerialized(
                                                            &mnet,
                                                            &resultAsSkeletonSerialized,
                                                            exactMocapNET2DOutput,
                                                            frameID,
                                                            MocapNETTrainingWidth,
                                                            MocapNETTrainingHeight
                                                         )
                                                       )
                                                        {
                                                            //TODO : Compare resultAsSkeletonSerialized and skeleton
                                                            doReprojectionCheck(&skeleton,&resultAsSkeletonSerialized);
                                                        }
 
                                                }
                                            else
                                                {
                                                    if (skippedFramesInARow%30==0) { fprintf(stderr,"."); }
                                                    ++skippedFramesInARow;
                                                }

                                            if (options.visualize)
                                                {
                                                    visualizationCommander(
                                                        &mnet,
                                                        &options,
                                                        &skeleton,
                                                        &frame,
                                                        result,
                                                        exactMocapNET2DOutput,
                                                        frameID,
                                                        1// We will do the waitKey call ourselves
                                                    );
 
                                                    imshow("Skeletons", viewMat);
                                                }
                                        }
                                }

}


float * mallocVectorR(std::vector<float> bvhFrame)
{
   if (bvhFrame.size()==0)
   {
       fprintf(stderr,"mallocVector given an empty vector..\n");
       //Empty bvh frame means no vector 
       return 0;
   }
   
    float * newVector = (float*) malloc(sizeof(float) * bvhFrame.size());
    if (newVector!=0)
        {
            for (int i=0; i<bvhFrame.size(); i++)
                {
                    newVector[i]=(float) bvhFrame[i];
                }
        }
    return newVector;
}


int postPoseTransform(const char * parentName, const char * jointName, float x, float y, float z, float qX, float qY, float qZ,float qW)
{
    //TF1 code
    //static tf::TransformBroadcaster br;
    //tf::Transform transform;
    //transform.setOrigin(tf::Vector3(x, y, z));
    //transform.setRotation(tf::createQuaternionFromRPY(roll,pitch,yaw) );  // if we just have xyz we dont have a rotation
    //transform.setRotation(tf::Quaternion(qX,qY,qZ,qW));  // if we just have xyz we dont have a rotation
    //br.sendTransform(tf::StampedTransform(transform, ros::Time::now(), parentName , jointName));
    
    static tf2_ros::TransformBroadcaster br;
    geometry_msgs::TransformStamped transformStamped;
    transformStamped.header.stamp = ros::Time::now();
    transformStamped.header.frame_id = parentName;
    transformStamped.child_frame_id = jointName;
    transformStamped.transform.translation.x = x;
    transformStamped.transform.translation.y = y;
    transformStamped.transform.translation.z = z;
    
    tf2::Quaternion q;
    q.setRPY(0, 0,0);
    
    transformStamped.transform.rotation.x = (double) qX;//q.x();
    transformStamped.transform.rotation.y = (double) qY;//q.y();
    transformStamped.transform.rotation.z = (double) qZ;//q.z();
    transformStamped.transform.rotation.w = (double) qW;//q.w();
    
    br.sendTransform(transformStamped);
    return 1;
}


//RGB Callback is called every time we get a new frame, it is synchronized to the main thread
void rgbCallback(const sensor_msgs::Image::ConstPtr rgb_img_msg,const sensor_msgs::CameraInfo::ConstPtr camera_info_msg)
{
    struct calibration intrinsics= {0};
     
    //ROS_INFO("New frame received..");
    //Using Intrinsic camera matrix for the raw (distorted) input images.

    //Focal lengths presented here are not the same with our calculations ( using zppd zpps )
    //https://github.com/ros-drivers/openni_camera/blob/groovy-devel/src/openni_device.cpp#L197
    intrinsics.fx = camera_info_msg->K[0];
    intrinsics.fy = camera_info_msg->K[4];
    intrinsics.cx = camera_info_msg->K[2];
    intrinsics.cy = camera_info_msg->K[5];

    intrinsics.k1 = camera_info_msg->D[0];
    intrinsics.k2 = camera_info_msg->D[1];
    intrinsics.p1 = camera_info_msg->D[2];
    intrinsics.p2 = camera_info_msg->D[3];
    intrinsics.k3 = camera_info_msg->D[4];

    width = camera_info_msg->width;
    height = camera_info_msg->height;

    //printf("We received an initial frame with the following metrics :  width = %u , height = %u , fx %0.2f , fy %0.2f , cx %0.2f , cy %0.2f \n",width,height,intrinsics.fx,intrinsics.fy,intrinsics.cx,intrinsics.cy);

    //cv::Mat rgb = cv::Mat(width,height,cv::CV_8UC3);
    cv::Mat rgb = cv::Mat::zeros(width,height,CV_8UC3);
    //A new pair of frames has arrived , copy and convert them so that they are ready
    cv_bridge::CvImageConstPtr orig_rgb_img;
    cv_bridge::CvImageConstPtr orig_depth_img;
    orig_rgb_img = cv_bridge::toCvCopy(rgb_img_msg, "rgb8");
    orig_rgb_img->image.copyTo(rgb);

    //printf("Passing Frame width %u height %u fx %0.2f fy %0.2f cx %0.2f cy %0.2f\n",width,height,intrinsics.fx,intrinsics.fy,intrinsics.cx,intrinsics.cy);
    //ROS_INFO("Passing New Frames to Synergies");
    //passNewFrames((unsigned char*) rgb.data,width , height , &intrinsics , &extrinsics);
    //ROS_INFO("Synergies should now have the new frames");

    camInfo = sensor_msgs::CameraInfo(*camera_info_msg);



    cv::Mat rgbTmp = rgb.clone();
    //Take care of drawing stuff as visual output
    cv::Mat bgrMat,rgbMat(height,width,CV_8UC3,rgbTmp.data,3*width);
    cv::cvtColor(rgbMat,bgrMat, cv::COLOR_RGB2BGR);// opencv expects the image in BGR format
    //cv::imshow("MocapNET - RGB input",bgrMat);
    
    //All MocapNET stuff in one call.. :P
    mocapNETProcessImage(bgrMat);
    
    //Set to 1 to broadcast skeleton with the left hand as t-pose and the right hand in front to make sense of the coordinate system
    #define STATIC_SKELETON_TO_UNDERSTAND_COORDINATE_SYSTEM 0
    
    #if STATIC_SKELETON_TO_UNDERSTAND_COORDINATE_SYSTEM
      std::vector<float> bvhFrame;
      for (unsigned int i=0; i<bvhMotion.numberOfValuesPerFrame; i++)
      {
        bvhFrame.push_back(0.0);
      }
      bvhFrame[2]=-130;
      bvhFrame[237]=74;
      bvhFrame[239]=74;
    #else
      std::vector<float> bvhFrame = mnet.currentSolution;
    #endif
    
    
    
     if (publishCameraTF)
               {
                 tf2::Quaternion cameraQuaternion;
                 //Everything is relative to mocapnetCamera and we are in charge of publishing it..
                 cameraQuaternion.setRPY(cameraRoll,cameraPitch,cameraYaw);
                 
                 postPoseTransform(
                                   tfRoot,
                                   "mocapnetCamera",
                                   cameraXPosition,
                                   cameraYPosition,
                                   cameraZPosition,
                                   cameraQuaternion.x(),//qX
                                   cameraQuaternion.y(),//qW
                                   cameraQuaternion.z(),//qZ
                                   cameraQuaternion.w() //qW
                                  );
                //------------------------------------ 
              }
    
    
    
    
    if (useSimpleBroadcaster)
    {
       //Simple 3D point broadcaster where every frame is in reference to TFRoot and there are no rotations
       std::vector<float> points3D = convertBVHFrameToFlat3DPoints(bvhFrame); 
       for (unsigned int pointID=0; pointID<points3D.size()/3; pointID++)
        {
         char pubName[512];
         snprintf(pubName,512,"%s",getBVHJointName(pointID)); 
         
         postPoseTransform(
                           "mocapnetCamera",
                           pubName,
                           points3D[pointID*3+0]/100,
                           points3D[pointID*3+1]/100,
                           points3D[pointID*3+2]/100,
                           0.0,0.0,0.0,1.0
                          ); 
        }  
    } else
    {
       if (bvhFrame.size()==bvhMotion.numberOfValuesPerFrame)
       {
        float * motionBuffer= mallocVectorR(bvhFrame);
        
        
        if (motionBuffer!=0)
        {
          bvh_cleanTransform(&bvhMotion,&bvhTransform);
          if (
               bvh_loadTransformForMotionBuffer(
                                                 &bvhMotion,
                                                 motionBuffer,
                                                 &bvhTransform,
                                                 0//Dont need extra information
                                                )
              )
              {
                float euler[3]={0};
                float quaternion[4]={0};
                float x,y,z,xRotation,yRotation,zRotation;
                char parentName[512];  
                char jointName[512];  
                tf2::Quaternion rX,rY,rZ,qXYZW;
       
                //getBVHNumberOfValuesPerFrame
                for (BVHJointID jointID=0; jointID<=bvhMotion.jointHierarchySize; jointID++)
                 {
                  if (jointID!=0)
                  {
                   //Normal joints ZXY
                   unsigned int parentJointID=getBVHParentJoint(jointID);
                   snprintf(parentName,512,"%s",bvhMotion.jointHierarchy[parentJointID].jointName); 
                   snprintf(jointName,512,"%s",bvhMotion.jointHierarchy[jointID].jointName); 
                   //------------------------------------------------------------------------------
                   x=bvhMotion.jointHierarchy[jointID].offset[0]/100;
                   y=bvhMotion.jointHierarchy[jointID].offset[1]/100;
                   z=bvhMotion.jointHierarchy[jointID].offset[2]/100;
                   
                   xRotation = bvh_getJointRotationXAtMotionBuffer(&bvhMotion,jointID,motionBuffer);
                   yRotation = bvh_getJointRotationYAtMotionBuffer(&bvhMotion,jointID,motionBuffer);
                   zRotation = bvh_getJointRotationZAtMotionBuffer(&bvhMotion,jointID,motionBuffer); 
                   rX = tf2::Quaternion(tf2::Vector3(-1,0,0),degreesToRadians(-xRotation));
                   rY = tf2::Quaternion(tf2::Vector3(0,-1,0),degreesToRadians(-yRotation));
                   rZ = tf2::Quaternion(tf2::Vector3(0,0,-1),degreesToRadians(-zRotation)); 
                   qXYZW = rZ * rX * rY;
                  } else
                  {
                    //Special handling for hip its ZYX
                   snprintf(parentName,512,"mocapnetCamera"); 
                   snprintf(jointName,512,"%s",bvhMotion.jointHierarchy[jointID].jointName); 
                   x=bvhFrame[0]/100;
                   y=bvhFrame[1]/100;
                   z=bvhFrame[2]/100;
                   zRotation = bvhFrame[3];
                   yRotation = bvhFrame[4]; 
                   xRotation = bvhFrame[5];
                   rX = tf2::Quaternion(tf2::Vector3(-1,0,0),degreesToRadians(-xRotation));
                   rY = tf2::Quaternion(tf2::Vector3(0,-1,0),degreesToRadians(-yRotation));
                   rZ = tf2::Quaternion(tf2::Vector3(0,0,-1),degreesToRadians(-zRotation)); 
                   qXYZW = rZ * rY * rX;
                  }
         
                 quaternion[0]=qXYZW.x();
                 quaternion[1]=qXYZW.y();
                 quaternion[2]=qXYZW.z();
                 quaternion[3]=qXYZW.w();
        
                 postPoseTransform(
                                   parentName,
                                   jointName,
                                   x,//points3D[pointID*3+0]/100,
                                   y,//points3D[pointID*3+1]/100,
                                   z,//points3D[pointID*3+2]/100
                                   quaternion[0],//qX
                                   quaternion[1],//qW
                                   quaternion[2],//qZ
                                   quaternion[3] //qW
                                  );
        
                 }  
            }
        free(motionBuffer);
       }
      } else
      {
          fprintf(stderr,"Wrong number of values in bvh frame, expected %u and got %lu.. \n",bvhMotion.numberOfValuesPerFrame,bvhFrame.size());
      }
    }
    
    cv::waitKey(1);

    return;
}
  
  
  
  
int main(int argc, char **argv)
{    
    //https://github.com/AmmarkoV/RGBDAcquisition/tree/master/3dparty/ROS/rgbd_acquisition
    //==================================================================================================================================
    //roslaunch rgbd_acquisition rgb_acquisition.launch deviceID:=sven.mp4-data moduleID:=TEMPLATE width:=1920 height:=1080 framerate:=1
    //roslaunch mocapnet_rosnode mocapnet_rosnode.launch
    //rviz
    //==================================================================================================================================

    ROS_INFO("Initializing MocapNET ROS Wrapper");
    try
    {
        ros::init(argc, argv, "MocapNET");
        ros::start();

        ros::NodeHandle nh;
        ros::NodeHandle private_node_handle("~");
        nhPtr = &nh;
 
        float rate = 30;
        std::string joint2DEstimatorName;
        std::string name;
        std::string fromRGBTopic;
        std::string fromRGBTopicInfo;
        std::string tfRootName;
        std::string tfTargetBVHFilename;

        ROS_INFO("Initializing Parameters..");
        
        private_node_handle.param("tfTargetBVHFilename", tfTargetBVHFilename, std::string("dataset/headerWithHeadAndOneMotion.bvh"));
        private_node_handle.param("fromRGBTopic", fromRGBTopic, std::string(camRGBRaw));
        private_node_handle.param("fromRGBTopicInfo", fromRGBTopicInfo, std::string(camRGBInfo));
        private_node_handle.param("name", name, std::string("mocapnet"));
        private_node_handle.param("joint2DEstimator", joint2DEstimatorName, std::string("forth"));
        private_node_handle.param("rate",rate);
        private_node_handle.param("publishCameraTF",publishCameraTF,1); 
        private_node_handle.param("useSimple3DPointTF",useSimpleBroadcaster,1);
        private_node_handle.param("cameraXPosition",cameraXPosition);
        private_node_handle.param("cameraYPosition",cameraYPosition);
        private_node_handle.param("cameraZPosition",cameraZPosition);
        private_node_handle.param("cameraRoll",cameraRoll);
        private_node_handle.param("cameraPitch",cameraPitch);
        private_node_handle.param("cameraYaw",cameraYaw);
        
        private_node_handle.param("tfRoot",tfRootName, std::string(DEFAULT_TF_ROOT));
        snprintf(tfRoot,510,"%s",tfRootName.c_str());
        fprintf(stderr,"TFRoot Name = %s ",tfRoot);
        
        
        rgb_img_sub = new  message_filters::Subscriber<sensor_msgs::Image>(nh,fromRGBTopic, 1);
        rgb_cam_info_sub = new message_filters::Subscriber<sensor_msgs::CameraInfo>(nh,fromRGBTopicInfo,1);
        message_filters::Synchronizer<RgbSyncPolicy> *sync = new message_filters::Synchronizer<RgbSyncPolicy>(RgbSyncPolicy(5), *rgb_img_sub,*rgb_cam_info_sub);
        //rosrun rqt_graph rqt_graph to test out what is going on
 
        ros::ServiceServer setCameraRollService      = nh.advertiseService(name + "/setCameraRoll", setCameraRoll);
        ros::ServiceServer setCameraPitchService     = nh.advertiseService(name + "/setCameraPitch", setCameraPitch);
        ros::ServiceServer setCameraYawService       = nh.advertiseService(name + "/setCameraYaw", setCameraYaw);
        ros::ServiceServer setCameraXPositionService = nh.advertiseService(name + "/setCameraXPosition", setCameraXPosition);
        ros::ServiceServer setCameraYPositionService = nh.advertiseService(name + "/setCameraYPosition", setCameraYPosition);
        ros::ServiceServer setCameraZPositionService = nh.advertiseService(name + "/setCameraZPosition", setCameraZPosition);
        ros::ServiceServer visualizeAnglesService    = nh.advertiseService(name + "/visualize_angles",&visualizeAngles);
        ros::ServiceServer visualizeMainService      = nh.advertiseService(name + "/visualize_main",&visualizeMain);
        ros::ServiceServer visualizeOverlayService   = nh.advertiseService(name + "/visualize_overlay",&visualizeOverlay);
        ros::ServiceServer visualizeOffService       = nh.advertiseService(name + "/visualize_off", &visualizeOff);
        ros::ServiceServer terminateService          = nh.advertiseService(name + "/terminate", terminate);

        //registerResultCallback((void*) sampleResultingSynergies);
        //registerUpdateLoopCallback((void *) loopEvent);
        //registerROSSpinner( (void *) frameSpinner );
        
        unsigned int lastBroadcastedFrame= 0;
        ros::Publisher pub = nh.advertise<std_msgs::Float32MultiArray>(name + "/bvhFrame", 1000);
        std_msgs::Float32MultiArray bvhFrame;
        bvhFrame.data.resize(MOCAPNET_OUTPUT_NUMBER);
 
        ROS_INFO("Done with ROS initialization!");


    //MocapNET stuff
    
    mnet.options = & options;
    defaultMocapNET2Options(&options);
    
   /*
    *  Force effortless IK configuration 
    */
    //Be unconstrained by default 
     options.constrainPositionRotation=0;
     //Use IK  ========
     options.useInverseKinematics=1;
     options.learningRate=0.01;
     options.iterations=5;
     options.epochs=30.0;
     options.spring=1.0;
     //==============
     options.visualizationType=1;
     
    //640x480 should be a high framerate compatible resolution
    //for most webcams, you can change this using --size X Y commandline parameter
    options.width = 640;
    options.height = 480;
    
    //We might want to load a special bvh file based on our options..! 
    loadOptionsAfterBVHLoadFromCommandlineOptions(&options,argc,argv);

    //If the initialization didnt happen inside the previous call lets do it now
    if (!options.hasInit)
            { 
               if (initializeBVHConverter(0,options.visWidth,options.visHeight,0))
                 {
                   fprintf(stderr,"BVH code initalization successfull..\n");
                   options.hasInit=1;                   
                 }
            }
            //--------------------------------------------------------------------------
    
    
    if (!bvh_loadBVH(tfTargetBVHFilename.c_str(),&bvhMotion,1.0) ) // This is the new armature that includes the head
        {
              ROS_ERROR("Failed to initialize MocapNET reading master BVH file.."); 
              exit(0);
              return 0;
        }
    
    
    //MocapNET things that get their options through ROS parameters
    int value; 
    //Performance options
    private_node_handle.param("useCPUOnlyForMocapNET",value,1);                                      options.useCPUOnlyForMocapNET=(unsigned int) value; 
    private_node_handle.param("useCPUOnlyFor2DEstimator",value,0);                                   options.useCPUOnlyFor2DEstimator=(unsigned int) value; 
    private_node_handle.param("multithreaded",value,0);                                              options.doMultiThreadedIK=(unsigned int) value; 
    //MocapNET options
    private_node_handle.param("MocapNETMode",value,5);                                               options.mocapNETMode=(unsigned int) value; 
    private_node_handle.param("useHierarchicalCoordinateDescent",value,1);                           options.useInverseKinematics=(unsigned int) value; 
    private_node_handle.param("hierarchicalCoordinateDescentLearningRate",options.learningRate);
    private_node_handle.param("hierarchicalCoordinateDescentSpring",options.spring);
    private_node_handle.param("hierarchicalCoordinateDescentIterations",value,5);                    options.iterations=(unsigned int) value;
    private_node_handle.param("hierarchicalCoordinateDescentEpochs",value,30);                       options.epochs=(unsigned int) value;  
    
    if (joint2DEstimatorName.compare("forth")==0)    { options.jointEstimatorUsed=JOINT_2D_ESTIMATOR_FORTH; }
    if (joint2DEstimatorName.compare("vnect")==0)    { options.jointEstimatorUsed=JOINT_2D_ESTIMATOR_VNECT; }
    if (joint2DEstimatorName.compare("openpose")==0) { options.jointEstimatorUsed=JOINT_2D_ESTIMATOR_OPENPOSE; } 
    
    ROS_INFO("Initializing 2D joint estimator");
    if (loadJointEstimator2D(
                             &jointEstimator,
                             options.jointEstimatorUsed,
                             1,
                             options.useCPUOnlyFor2DEstimator
                            ))
        {
            ROS_INFO("Initializing MocapNET");
            if ( loadMocapNET2(&mnet,"MocapNET ROS Node") )
                {
                   //Last check for inconsistencies..
                   //Since the tfTargetBVHFilename can load a different BVH file than the internal model
                   //We need to check that the two BVH armatures have joint parity..
                   //A stricter test could also make sure joint names are the same, however someone can essencially alter the Frame names using this file
                   //so we will only check for the number of joints..!
                   if  (
                          (bvhMotion.numberOfValuesPerFrame!=getBVHNumberOfValuesPerFrame()) ||
                          (bvhMotion.jointHierarchySize!=getBVHNumberOfJoints())
                       )
                       {
                         ROS_ERROR("Fatal error: tfTargetBVHFilename given doesn't have parity with internal MocapNET model.."); 
                         ROS_ERROR("Consider switching the tfTargetBVHFilename back to dataset/headerWithHeadAndOneMotion.bvh"); 
                         ROS_ERROR("or using it as your starting point for modifying it.."); 
                         exit(0); 
                       }
    
                    
                  sync->registerCallback(rgbCallback);
                  ROS_INFO("Registered ROS services, we should be able to process incoming messages now!");
                  
                  //Lets go..
                  //////////////////////////////////////////////////////////////////////////
                  unsigned long i = 0;
                  // startTime=cvGetTickCount();
                  while ( ( key!='q' ) && (ros::ok()) )
                  {
                      ros::spinOnce();
                      
                      if  ( 
                            (MOCAPNET_OUTPUT_NUMBER==mnet.currentSolution.size()) &&
                            (MOCAPNET_OUTPUT_NUMBER==bvhFrame.data.size()) && 
                            (lastBroadcastedFrame != mnet.framesReceived)
                          )
                          {
                             for (unsigned int i=0; i<MOCAPNET_OUTPUT_NUMBER; i++)
                             {
                                 bvhFrame.data[i] = mnet.currentSolution[i];
                             }
                             lastBroadcastedFrame = mnet.framesReceived;
                             pub.publish(bvhFrame);
                          }
                      
                      if (i%1000==0)
                         {
                           fprintf(stderr,".");
                           //loopEvent(); //<- this keeps our ros node messages handled up until synergies take control of the main thread
                         }
                      ++i;

                      usleep(1000);
                  } 
                 //////////////////////////////////////////////////////////////////////////
                }else
                { ROS_ERROR("Failed to initialize MocapNET 3D pose estimation.."); }
        } else
        { ROS_ERROR("Failed to initialize MocapNET built-in 2D joint estimation.."); }

    }
    catch(std::exception &e) {
        ROS_ERROR("Exception: %s", e.what());
        return 1;
    }
    catch(...)               {
        ROS_ERROR("Unknown Error");
        return 1;
    }
 
    ROS_INFO("Shutdown complete");
    return 0;
}
