/* Copyright (c) 2017-2022 Hans-Kristian Arntzen
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "atomic_append_buffer.hpp"
#include "global_managers.hpp"
#include "filesystem.hpp"
#include "object_pool.hpp"
#include <vector>
#include <mutex>
#include <memory>

namespace Granite
{
struct ImageAssetID
{
	uint32_t id = uint32_t(-1);
	explicit inline operator bool() const { return id != uint32_t(-1); }
};

class AssetManager;

class AssetInstantiatorInterface
{
public:
	virtual ~AssetInstantiatorInterface() = default;

	// This estimate should be an upper bound.
	virtual uint64_t estimate_cost_image_resource(ImageAssetID id, FileHandle &mapping) = 0;

	// When instantiation completes, manager.update_cost() must be called with the real cost.
	// The real cost may only be known after async parsing of the file.
	virtual void instantiate_image_resource(AssetManager &manager, ImageAssetID id, FileHandle &mapping) = 0;

	// Will only be called after an upload completes through manager.update_cost().
	virtual void release_image_resource(ImageAssetID id) = 0;
	virtual void set_id_bounds(uint32_t bound) = 0;

	// Called in AssetManager::iterate().
	virtual void latch_handles() = 0;
};

class ThreadGroup;
struct TaskSignal;

class AssetManager final : public AssetManagerInterface
{
public:
	AssetManager();
	~AssetManager() override;

	void set_asset_instantiator_interface(AssetInstantiatorInterface *iface);
	void set_image_budget(uint64_t cost);
	void set_image_budget_per_iteration(uint64_t cost);

	// FileHandle is intended to be used with FileSlice or similar here so that we don't need
	// a ton of open files at once.
	ImageAssetID register_image_resource(FileHandle file);
	// Prio 0: Not resident, resource may not exist.
	bool set_image_residency_priority(ImageAssetID id, int prio);

	// Intended to be called in Application::post_frame(). Not thread safe.
	// This function updates internal state.
	void iterate(ThreadGroup *group);

	// Always thread safe, used by AssetInstantiatorInterfaces to update cost estimates.
	void update_cost(ImageAssetID id, uint64_t cost);

	// May be called concurrently, except when calling iterate().
	uint64_t get_current_total_consumed() const;

	// May be called concurrently, except when calling iterate().
	// Intended to be called by asset instantiator interface or similar.
	// When a resource is actually accessed, this is called.
	void mark_used_resource(ImageAssetID id);

private:
	struct AssetInfo
	{
		uint64_t pending_consumed = 0;
		uint64_t consumed = 0;
		uint64_t last_used = 0;
		FileHandle handle;
		ImageAssetID id = {};
		int prio = 0;
	};

	std::vector<AssetInfo *> sorted_assets;
	std::vector<AssetInfo *> asset_bank;
	Util::ObjectPool<AssetInfo> pool;
	Util::AtomicAppendBuffer<ImageAssetID> lru_append;

	AssetInstantiatorInterface *iface = nullptr;
	uint32_t id_count = 0;
	uint64_t total_consumed = 0;
	uint64_t image_budget = 0;
	uint64_t image_budget_per_iteration = 0;
	uint64_t timestamp = 0;

	struct CostUpdate
	{
		ImageAssetID id;
		uint64_t cost = 0;
	};
	std::mutex cost_update_lock;
	std::vector<CostUpdate> thread_cost_updates;
	std::vector<CostUpdate> cost_updates;

	void adjust_update(const CostUpdate &update);
	std::unique_ptr<TaskSignal> signal;
};
}
