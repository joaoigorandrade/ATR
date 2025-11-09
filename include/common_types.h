#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

/**
 * @brief Truck operation states
 *
 * These states are determined by the Command Logic task based on
 * sensor readings and operator commands.
 */
struct TruckState {
    bool fault;        // e_defeito: true if fault present, false if OK
    bool automatic;    // e_automatico: true if automatic mode, false if manual
};

/**
 * @brief Operator commands
 *
 * Commands issued by the local operator through the Local Interface.
 * These are processed by the Command Logic task.
 */
struct OperatorCommand {
    bool auto_mode;    // c_automatico: switch to automatic mode
    bool manual_mode;  // c_man: switch to manual mode
    bool rearm;        // c_rearme: acknowledge and clear fault
    int accelerate;    // c_acelera: acceleration command (-100 to 100)
    int steer_left;    // c_esquerda: turn left (increases angle)
    int steer_right;   // c_direita: turn right (decreases angle)

    // Default constructor
    OperatorCommand()
        : auto_mode(false), manual_mode(false), rearm(false),
          accelerate(0), steer_left(0), steer_right(0) {}
};

/**
 * @brief Actuator outputs
 *
 * Determined by Command Logic and Navigation Control tasks.
 * These control the truck's physical actuators.
 */
struct ActuatorOutput {
    int acceleration;  // o_aceleracao: -100 to 100 (%)
    int steering;      // o_direcao: -180 to 180 (degrees)

    // Default constructor
    ActuatorOutput() : acceleration(0), steering(0) {}
};

/**
 * @brief Navigation setpoints
 *
 * Target values set by Route Planning for Navigation Control.
 */
struct NavigationSetpoint {
    int target_position_x;  // Target X coordinate
    int target_position_y;  // Target Y coordinate
    int target_speed;       // Target speed (percentage)
    int target_angle;       // Target heading angle (degrees)

    // Default constructor
    NavigationSetpoint()
        : target_position_x(0), target_position_y(0),
          target_speed(0), target_angle(0) {}
};

/**
 * @brief Fault types for monitoring
 */
enum class FaultType {
    NONE,
    TEMPERATURE_ALERT,    // T > 95°C
    TEMPERATURE_CRITICAL, // T > 120°C
    ELECTRICAL,           // Electrical system fault
    HYDRAULIC             // Hydraulic system fault
};

#endif // COMMON_TYPES_H
