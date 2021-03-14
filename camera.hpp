#ifndef __CAMERA_H_
#define __CAMERA_H_

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class Camera {
 public:
  Camera(glm::vec3);

  void update(float);
  glm::mat4 getView();
  void setAcceleration(float);
  void setStrafeAcceleration(float);

 private:
  glm::vec3 mPos;
  glm::vec3 mFront;
  glm::vec3 mUp;
  float mVelocity;
  float mStrafeVelocity;
  float mAcceleration;
  float mStrafeAcceleration;

 public:
  float yaw;
  float pitch;

  static constexpr float const MAX_VELOCITY = 2.0f;
  static constexpr float const MAX_STRAFE_VELOCITY = 2.0f;
  static constexpr float const FRICTION = 0.003f;
};

#endif  // __CAMERA_H_
