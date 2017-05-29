#include "input.hpp"
#include "vulkan_events.hpp"
#include "event.hpp"
#include <algorithm>

using namespace Util;
using namespace std;

namespace Granite
{
void InputTracker::orientation_event(quat rot)
{
	EventManager::get_global().dispatch_inline(OrientationEvent{rot});
}

void InputTracker::on_touch_down(unsigned id, float x, float y)
{
	if (touch.active_pointers >= TouchCount)
	{
		LOGE("Touch pointer overflow!\n");
		return;
	}

	unsigned index = touch.active_pointers++;
	auto &pointer = touch.pointers[index];
	pointer.id = id;
	pointer.start_x = x;
	pointer.start_y = y;
	pointer.last_x = x;
	pointer.last_y = y;
	pointer.x = x;
	pointer.y = y;

	EventManager::get_global().dispatch_inline(TouchDownEvent{index, id, x, y});
}

void InputTracker::dispatch_touch_gesture()
{
	EventManager::get_global().dispatch_inline(TouchGestureEvent{touch});
}

void InputTracker::on_touch_move(unsigned id, float x, float y)
{
	auto &pointers = touch.pointers;
	auto itr = find_if(begin(pointers), begin(pointers) + touch.active_pointers, [id](const TouchState::Pointer &pointer) {
		return pointer.id == id;
	});

	if (itr == end(pointers))
	{
		LOGE("Could not find pointer!\n");
		return;
	}

	itr->x = x;
	itr->y = y;
}

void InputTracker::on_touch_up(unsigned id, float x, float y)
{
	auto &pointers = touch.pointers;
	auto itr = find_if(begin(pointers), begin(pointers) + touch.active_pointers, [id](const TouchState::Pointer &pointer) {
		return pointer.id == id;
	});

	if (itr == end(pointers))
	{
		LOGE("Could not find pointer!\n");
		return;
	}

	auto index = itr - begin(pointers);
	EventManager::get_global().dispatch_inline(TouchUpEvent{itr->id, x, y, itr->start_x, itr->start_y});
	memmove(&pointers[index], &pointers[index + 1], (TouchCount - (index + 1)) * sizeof(TouchState::Pointer));
	touch.active_pointers--;
}

void InputTracker::key_event(Key key, KeyState state)
{
	if (state == KeyState::Released)
		key_state &= ~(1ull << ecast(key));
	else if (state == KeyState::Pressed)
		key_state |= 1ull << ecast(key);

	EventManager::get_global().dispatch_inline(KeyboardEvent{key, state});
}

void InputTracker::mouse_button_event(MouseButton button, bool pressed)
{
	if (pressed)
		mouse_button_state |= 1ull << ecast(button);
	else
		mouse_button_state &= ~(1ull << ecast(button));

	EventManager::get_global().dispatch_inline(MouseButtonEvent{button, pressed});
}

void InputTracker::mouse_move_event(double x, double y)
{
	if (mouse_active)
	{
		double delta_x = x - last_mouse_x;
		double delta_y = y - last_mouse_y;
		last_mouse_x = x;
		last_mouse_y = y;
		EventManager::get_global().dispatch_inline(
			MouseMoveEvent{delta_x, delta_y, x, y, key_state, mouse_button_state});
	}
}

void InputTracker::mouse_enter(double x, double y)
{
	mouse_active = true;
	last_mouse_x = x;
	last_mouse_y = y;
}

void InputTracker::mouse_leave()
{
	mouse_active = false;
}

void InputTracker::dispatch_current_state(double delta_time)
{
	EventManager::get_global().dispatch_inline(InputStateEvent{last_mouse_x, last_mouse_y, delta_time, key_state, mouse_button_state, mouse_active});
}

}