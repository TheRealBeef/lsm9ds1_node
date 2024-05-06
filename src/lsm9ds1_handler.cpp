#include "lsm9ds1_handler/lsm9ds1_handler.hpp"
#include <cmath> // For std::isnan

namespace lsm9ds1
{
LSM9DS1::LSM9DS1(const std::string &name_) : imu_name_(name_)
{

    declare_ROS_params();
    initialize();
}

LSM9DS1::~LSM9DS1()
{
    node_.reset();
    lsm9ds1_device_.reset();
}

void LSM9DS1::declare_ROS_params()
{
    RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "initializing lsm9ds1 handler");

    node_ = std::make_shared<rclcpp::Node>(imu_name_);
    node_->declare_parameter<int>("frequency", 100);
    node_->declare_parameter<uint8_t>("i2c_interface", 1);
    node_->declare_parameter<uint8_t>("i2c_address_mag", 0x1e);
    node_->declare_parameter<uint8_t>("i2c_address_accelgyro", 0x6b);

    node_->declare_parameter<uint8_t>("accel_rate", 0);
    node_->declare_parameter<uint8_t>("accel_scale", 0);

    node_->declare_parameter<uint8_t>("gyro_rate", 0);
    node_->declare_parameter<uint8_t>("gyro_scale", 0);

    node_->declare_parameter<uint8_t>("mag_rate", 0);
    node_->declare_parameter<uint8_t>("mag_scale", 0);
}

void LSM9DS1::initialize()
{
    int frequency = node_->get_parameter("frequency").get_parameter_value().get<int>();
    uint8_t bus_index = node_->get_parameter("i2c_interface").get_parameter_value().get<uint8_t>();
    uint8_t i2c_address_accelgyro = node_->get_parameter("i2c_address_accelgyro").get_parameter_value().get<uint8_t>();
    uint8_t i2c_address_mag = node_->get_parameter("i2c_address_mag").get_parameter_value().get<uint8_t>();
    uint8_t accel_rate = node_->get_parameter("accel_rate").get_parameter_value().get<uint8_t>();
    uint8_t accel_scale = node_->get_parameter("accel_scale").get_parameter_value().get<uint8_t>();
    uint8_t gyro_rate = node_->get_parameter("gyro_rate").get_parameter_value().get<uint8_t>();
    uint8_t gyro_scale = node_->get_parameter("gyro_scale").get_parameter_value().get<uint8_t>();
    uint8_t mag_rate = node_->get_parameter("mag_rate").get_parameter_value().get<uint8_t>();
    uint8_t mag_scale = node_->get_parameter("mag_scale").get_parameter_value().get<uint8_t>();

    RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "bus_index_ %d", bus_index);
    RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "i2c_address_mag %d", i2c_address_mag);
    RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "i2c_address_accelgyro %d", i2c_address_accelgyro);
    RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "accel_rate %d", accel_rate);
    RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "accel_scale %d", accel_scale);
    RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "gyro_rate %d", gyro_rate);
    RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "gyro_scale %d", gyro_scale);
    RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "mag_rate %d", mag_rate);
    RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "mag_scale %d", mag_scale);

    lsm9ds1_device_ = std::make_shared<LSM9DS1_Device>(bus_index, i2c_address_accelgyro, i2c_address_mag);
    lsm9ds1_device_->configure_accel(accel_scale, accel_rate);
    lsm9ds1_device_->configure_gyro(gyro_scale, gyro_rate);
    lsm9ds1_device_->configure_mag(mag_scale, mag_rate, true);



    lsm9ds1_device_->calibrate_accelgyro();

    if (frequency == 0)
    {
        throw std::runtime_error("error: frequency cannot be 0\n");
    }

    publisher_ = node_->create_publisher<sensor_msgs::msg::Imu>(imu_name_ + "/imu", 10);
    timer_ = node_->create_wall_timer(std::chrono::milliseconds(1000 / frequency), std::bind(&LSM9DS1::read_IMU, this));
}

void LSM9DS1::read_IMU()
{
    IMURecord imu_record = lsm9ds1_device_->read_all();

    // Check for NaN values
    if (std::isnan(imu_record.raw_linear_acceleration.x) || std::isnan(imu_record.raw_linear_acceleration.y) || std::isnan(imu_record.raw_linear_acceleration.z) ||
        std::isnan(imu_record.raw_angular_velocity.x) || std::isnan(imu_record.raw_angular_velocity.y) || std::isnan(imu_record.raw_angular_velocity.z)) {

        RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "Invalid sensor readings detected. Using previous values.");

        if (lastGoodIMURecord.has_value()) {
            imu_record = lastGoodIMURecord.value();
        } else {
            RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "No valid previous sensor readings available.");
            return; // Skip publishing if no valid data is available
        }
    } else {
        // Update last known good values
        lastGoodIMURecord = imu_record;
    }

    telemetry_msg_.linear_acceleration.x = imu_record.raw_linear_acceleration.x;
    telemetry_msg_.linear_acceleration.y = imu_record.raw_linear_acceleration.y;
    telemetry_msg_.linear_acceleration.z = imu_record.raw_linear_acceleration.z;

    double deg_to_rad = M_PI / 180.0;

    telemetry_msg_.angular_velocity.x = imu_record.raw_angular_velocity.x * deg_to_rad;
    telemetry_msg_.angular_velocity.y = imu_record.raw_angular_velocity.y * deg_to_rad;
    telemetry_msg_.angular_velocity.z = imu_record.raw_angular_velocity.z * deg_to_rad;

     // Set the header stamp and frame_id
    telemetry_msg_.header.stamp = this->node_->get_clock()->now();
    telemetry_msg_.header.frame_id = "imu_link";  // Set this to the appropriate frame ID

    publish();
}


void LSM9DS1::publish()
{
    publisher_->publish(telemetry_msg_);
}
} // namespace lsm9ds1
