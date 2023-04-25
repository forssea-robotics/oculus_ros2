#ifndef OCULUS_SONAR_NODE_H
#define OCULUS_SONAR_NODE_H

#include <iostream>
#include <sstream>
#include <thread>
#include <future>

#include "rclcpp/rclcpp.hpp"

#include "conversions.h"

#include <oculus_driver/AsyncService.h>
#include <oculus_driver/SonarDriver.h>

#include "oculus_interfaces/msg/oculus_status.hpp"
#include "oculus_interfaces/msg/ping.hpp"
#include "sensor_msgs/msg/temperature.hpp"
#include "sensor_msgs/msg/fluid_pressure.hpp"

#include "sonar_viewer.h"

#include "rcl_interfaces/msg/parameter_descriptor.hpp"

typedef struct
{
  std::string frame_id;
  int frequency_mode;
  int ping_rate;
  int data_depth;
  int nbeams;
  bool gain_assist;
  double range;
  int gamma_correction;
  double gain_percent;
  double sound_speed;
  bool use_salinity;
  double salinity;
} rosParameters;

class OculusSonarNode : public rclcpp::Node
{
public:
  OculusSonarNode();
  ~OculusSonarNode();

protected:
  const std::vector<std::string> dynamic_parameters_names{"frequency_mode", "ping_rate", "data_depth", "nbeams", "gain_assist", "range", "gamma_correction", "gain_percent", "sound_speed", "use_salinity", "salinity", "standby"};
  rosParameters currentSonarParameters;
  rosParameters currentRosParameters;
  oculus::SonarDriver::PingConfig currentConfig;

  bool is_in_standby_mode; // Same value as ros paramater "standby"

  mutable std::shared_mutex param_mutex; ///< multithreading protection

private:
  const std::shared_ptr<oculus::SonarDriver> sonar_driver_;
  oculus::AsyncService io_service_;
  // rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_publisher_;
  const SonarViewer sonar_viewer;
  const std::string frame_id;
  double temperature_warn_limit;
  const double temperature_stop_limit;
  rclcpp::Publisher<oculus_interfaces::msg::OculusStatus>::SharedPtr status_publisher_{nullptr};
  rclcpp::Publisher<oculus_interfaces::msg::Ping>::SharedPtr ping_publisher_{nullptr};
  rclcpp::Publisher<sensor_msgs::msg::Temperature>::SharedPtr temperature_publisher_{nullptr};
  rclcpp::Publisher<sensor_msgs::msg::FluidPressure>::SharedPtr pressure_publisher_{nullptr};

  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_cb_{nullptr};

  template <class T>
  void update_ros_config_for_param(T &currentSonar_param, const T &new_param, const std::string &ros_param_name, const std::string &param_name);
  template <class T>
  void update_ros_config_for_param(T &currentSonar_param, const T &new_param, const std::string &param_name);
  void update_ros_config();
  template <class T>
  void handle_feedback_for_param(rcl_interfaces::msg::SetParametersResult &result, const rclcpp::Parameter &param, const T &old_val, const T &new_val, const std::string &param_name, const std::string &param_name_to_display = std::string()) const;
  void update_parameters(rosParameters &parameters, const std::vector<rclcpp::Parameter> &new_parameters);
  void update_parameters(rosParameters &parameters, oculus::SonarDriver::PingConfig feedback);
  void send_param_to_sonar(rclcpp::Parameter param, rcl_interfaces::msg::SetParametersResult result);
  rcl_interfaces::msg::SetParametersResult set_config_callback(const std::vector<rclcpp::Parameter> &parameters);

  void publish_status(const OculusStatusMsg &status) const;
  void publish_ping(const oculus::PingMessage::ConstPtr &pingMetadata);
  void handle_dummy() const;
};

template <class T>
void OculusSonarNode::update_ros_config_for_param(T &currentSonar_param, const T &new_param, const std::string &param_name)
{
  update_ros_config_for_param(currentSonar_param, new_param, param_name, param_name);
}

template <class T>
void OculusSonarNode::update_ros_config_for_param(T &currentSonar_param, const T &new_param, const std::string &ros_param_name, const std::string &param_name)
{
  if (currentSonar_param != new_param)
  {
    this->remove_on_set_parameters_callback(this->param_cb_.get());
    RCLCPP_INFO_STREAM(this->get_logger(), "coucou " << param_name << " : ros = " << currentSonar_param << " topic = " << new_param);
    currentSonar_param = new_param;
    this->set_parameter(rclcpp::Parameter(ros_param_name, new_param));
    this->param_cb_ = this->add_on_set_parameters_callback(std::bind(&OculusSonarNode::set_config_callback, this, std::placeholders::_1));
  }
}

template <class T>
void OculusSonarNode::handle_feedback_for_param(rcl_interfaces::msg::SetParametersResult &result, const rclcpp::Parameter &param, const T &old_val, const T &new_val, const std::string &param_name, const std::string &param_name_to_display) const
{

  if (old_val != new_val)
  {
    std::string param_name_to_display_ = param_name_to_display == "" ? param_name : param_name_to_display;
    if (param.get_name() == param_name)
    {
      // if (parameters.size() == 1)
      result.successful = false;
      RCLCPP_WARN_STREAM(this->get_logger(), "Could not update " << param_name_to_display_);
      result.reason.append("Could not update " + param_name_to_display_ + ".\n");
    }
    else
    {
      RCLCPP_WARN_STREAM(this->get_logger(), param_name_to_display_ << " change from " << old_val << " to " << new_val << " when updating the parameter " << param.get_name());
      result.reason.append(param_name_to_display_ + " change.\n");
    }
  }
}

#endif /* OCULUS_SONAR_NODE_H */
