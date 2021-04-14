#include "camera.hpp"

#include <iostream>

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_projection.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"

Camera::Camera(glm::vec3 pos)
    : mPos{pos},
      mFront{-2.0f, -2.0f, -2.0f},
      mUp{0.0f, 1.0f, 0.0f},
      yaw{-90.0f},
      pitch{0.0f} {}

void Camera::update(float deltaTime) {
  if (mAcceleration > 0.0f) {
    mVelocity = fmin(mVelocity + mAcceleration, MAX_VELOCITY);
  } else if (mAcceleration < 0.0f) {
    mVelocity = fmax(mVelocity + mAcceleration, -MAX_VELOCITY);
  } else {
    mVelocity = mVelocity > 0.0f ? fmax(mVelocity - FRICTION, 0.0f)
                                 : fmin(mVelocity + FRICTION, 0.0f);
  }

  if (mStrafeAcceleration > 0.0f) {
    mStrafeVelocity =
        fmin(mStrafeVelocity + mStrafeAcceleration, MAX_STRAFE_VELOCITY);
  } else if (mStrafeAcceleration < 0.0f) {
    mStrafeVelocity =
        fmax(mStrafeVelocity + mStrafeAcceleration, -MAX_STRAFE_VELOCITY);
  } else {
    mStrafeVelocity = fmax(mStrafeVelocity - FRICTION, 0.0f);
  }

  mPos += mVelocity * mFront * deltaTime;
  mPos += mStrafeVelocity * glm::normalize(glm::cross(mFront, mUp)) * deltaTime;
  mAcceleration = 0.0f;
  mStrafeAcceleration = 0.0f;

  if (pitch > 89.0f) {
    pitch = 89.0f;
  }
  if (pitch < -89.0f) {
    pitch = -89.0f;
  }

  glm::vec3 direction{cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
                      sin(glm::radians(pitch)),
                      sin(glm::radians(yaw)) * cos(glm::radians(pitch))};

  mFront = glm::normalize(direction);
}

void Camera::setAcceleration(float acc) { mAcceleration = acc; }

void Camera::setStrafeAcceleration(float acc) { mStrafeAcceleration = acc; }

glm::mat4 Camera::getView() { return glm::lookAt(mPos, mPos + mFront, mUp); }
