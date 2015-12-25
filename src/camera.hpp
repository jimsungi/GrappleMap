#ifndef JIUJITSUMAPPER_CAMERA_HPP
#define JIUJITSUMAPPER_CAMERA_HPP

#include "math.hpp"

class Camera
{
	V2 viewportSize;
	V3 offset{0,0,0};
	V3 orientation{0, -0.7, 1.7};
		// x used for rotation over y axis, y used for rotation over x axis, z used for zoom
	M proj, mv, full_;

	void computeMv()
	{
		mv = translate({0, 0, -orientation.z}) * xrot(orientation.y) * yrot(orientation.x) * translate(-offset);
		full_ = proj * mv;
	}

public:

	Camera() { setViewportSize(90, 640, 480); computeMv(); }

	M const & projection() const { return proj; }
	M const & model_view() const { return mv; }
	M const & full() const { return full_; }

	void setViewportSize(double fov, int x, int y)
	{
		viewportSize.x = x;
		viewportSize.y = y;
		proj = perspective(fov, viewportSize.x / viewportSize.y, 0.01, 6);
		full_ = proj * mv;
	}

	void hardSetOffset(V3 o) { offset = o; computeMv(); }

	void setOffset(V2 o)
	{
		offset = offset * 0.99 + V3{o.x, offset.y, o.y} * 0.01;
		computeMv();
	}

	void rotateHorizontal(double radians)
	{
		orientation.x += radians;
		computeMv();
	}

	void rotateVertical(double radians)
	{
		orientation.y += radians;
		orientation.y = std::max(-M_PI/2, orientation.y);
		orientation.y = std::min(M_PI/2, orientation.y);
		computeMv();
	}

	void zoom(double z)
	{
		orientation.z += z;
		computeMv();
	}

	double getHorizontalRotation() const { return orientation.x; }
};

inline V2 world2xy(Camera const & camera, V3 v)
{
	auto cs = camera.full() * V4(v, 1);
	return xy(cs) / cs.w;
}

#endif
