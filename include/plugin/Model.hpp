#pragma once
#include <common.hpp>
#include <plugin/Plugin.hpp>
#include <jansson.h>
#include <set>


namespace rack {


namespace app {
struct ModuleWidget;
} // namespace app


namespace engine {
struct Module;
} // namespace engine


namespace plugin {


struct Model {
	Plugin* plugin = NULL;

	/** Must be unique. Used for saving patches. Never change this after releasing your module.
	The model slug must be unique within your plugin, but it doesn't need to be unique among different plugins.
	*/
	std::string slug;
	/** Human readable name for your model, e.g. "Voltage Controlled Oscillator" */
	std::string name;
	/** List of tag IDs representing the function(s) of the module.
	Tag IDs are not part of the ABI and may change at any time.
	*/
	std::vector<int> tags;
	/** A one-line summary of the module's purpose */
	std::string description;
	/** The manual of the module. HTML, PDF, or GitHub readme/wiki are fine.
	*/
	std::string manualUrl;

	virtual ~Model() {}
	/** Creates a Module. */
	virtual engine::Module* createModule() {
		return NULL;
	}
	/** Creates a ModuleWidget with a Module attached.
	Module may be NULL.
	*/
	virtual app::ModuleWidget* createModuleWidget(engine::Module* m) {
		return NULL;
	}

	void fromJson(json_t* rootJ);
	/** Returns the branded name of the model, e.g. VCV VCO-1. */
	std::string getFullName();
	std::string getFactoryPresetDir();
	std::string getUserPresetDir();
};


} // namespace plugin
} // namespace rack
