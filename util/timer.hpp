#pragma once

#include <stdint.h>

namespace Granite
{
class FrameTimer
{
public:
	FrameTimer();

	double frame();
	double get_elapsed() const;
	double get_frame_time() const;

private:
	int64_t start;
	int64_t last;
	int64_t last_period;
	int64_t get_time();
};
}