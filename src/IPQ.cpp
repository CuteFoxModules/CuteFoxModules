#include "plugin.hpp"
#include <dsp/digital.hpp>
#include "app/LedDisplay.hpp"

struct CuteFoxBigKnob : SvgKnob{
	CuteFoxBigKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/CuteFoxBigKnob.svg")));
		shadow->opacity = 0.0f;
		minAngle = -0.80 * M_PI;
		maxAngle = 0.80 * M_PI;

	}
};

struct CuteFoxScrew : SvgScrew{
	CuteFoxScrew() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/CuteFoxScrew.svg")));
	}
};

struct CuteFoxPort : PJ301MPort{
	CuteFoxPort() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/CuteFoxPort.svg")));
		shadow->opacity = 0.0f;
	}
};

struct IPQ : Module {
	enum ParamId {
		INTERVAL1_PARAM,
		INTERVAL2_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		INPUT_INPUT,
		CV1_INPUT,
		CV2_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUT_OUTPUT,
		TRIGGER_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	dsp::PulseGenerator pulseGenerator;

	IPQ() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(INTERVAL1_PARAM, 0.f, 11.f, 0.f, "Interval 1 (in semitones)");
		configParam(INTERVAL2_PARAM, 0.f, 11.f, 0.f, "Interval 2 (in semitones)");
		configInput(INPUT_INPUT, "1V/Octave pitch");
		configInput(CV1_INPUT, "Interval 1 CV");
		configInput(CV2_INPUT, "Interval 2 CV");
		configOutput(OUTPUT_OUTPUT, "1V/Octave pitch");
		configOutput(TRIGGER_OUTPUT, "Trigger on note change");
		paramQuantities[INTERVAL1_PARAM]->snapEnabled = true;
		paramQuantities[INTERVAL2_PARAM]->snapEnabled = true;


	}
	float minDiff = 10;
	float lastNote = 0;
	float intervals[2]{0,0};
	int currentInterval = 0;
	float bestNote = 0;
	float voltsOut = 0;

	void process(const ProcessArgs& args) override {
		if (inputs[CV1_INPUT].isConnected()){
			paramQuantities[INTERVAL1_PARAM]->setValue(((inputs[CV1_INPUT].getVoltage() / 10) * 11));
		}
		if (inputs[CV2_INPUT].isConnected()){
			paramQuantities[INTERVAL2_PARAM]->setValue(((inputs[CV2_INPUT].getVoltage() / 10) * 11));
		}
		intervals[0] = {params[INTERVAL1_PARAM].getValue()};
		intervals[1] = {params[INTERVAL2_PARAM].getValue()};
		float vDiff;
		float vNote;
		minDiff = 1 / 12.0f;
		if (inputs[INPUT_INPUT].isConnected() && outputs[OUTPUT_OUTPUT].isConnected()){
			float voltsIn = inputs[INPUT_INPUT].getVoltage();
			int vOctave = int(floorf(voltsIn));
			float vPitch = voltsIn - vOctave;
			if (fabs(voltsIn - lastNote) >= (intervals[currentInterval] / 12.0f)){
				for (int i = 0; i <= 12; i++)
				{
					vNote = i / 12.0f;
					vDiff = fabs(vPitch - vNote);
					if (vDiff < minDiff)
						{
							bestNote = vNote;
							minDiff = vDiff;
						}
				}
				voltsOut = bestNote + vOctave;
				lastNote = voltsOut;
				currentInterval = (currentInterval == 0 ? 1: 0);
				pulseGenerator.trigger(1e-3f);
			}
			bool pulse = pulseGenerator.process(args.sampleTime);
			outputs[TRIGGER_OUTPUT].setVoltage(pulse ? 10.f : 0.f);
		}
		else {
			voltsOut = 0;
		}
		outputs[OUTPUT_OUTPUT].setVoltage(voltsOut);
	}
};

struct IntervalDisplay : LedDisplay {
	IPQ* module;
	int displayInterval = 0;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Cardo-Regular.ttf"));
			std::string interval1 = std::to_string((int)(module->intervals[displayInterval]));
			std::string intervalNames[12]{"Unison","Minor 2nd","Major 2nd","Minor 3rd","Major 3rd","Perfect 4th", "Tritone", "Perfect 5th", "Minor 6th", "Major 6th", "Minor 7th", "Major 7th"};
			int interval1int = (int)(module->intervals[displayInterval]);
			std::string fullText = interval1 + " / " + intervalNames[interval1int];
			if (!font)
				return;	
			nvgSave(args.vg);
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 10);
			nvgTextLetterSpacing(args.vg, 0.0);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT);
			nvgFillColor(args.vg, nvgRGB(00, 00, 00));
			nvgText(args.vg, 0, 0, "|", NULL);
			nvgText(args.vg, -16, 0, interval1.c_str(), NULL);
			nvgText(args.vg, 9, 0, (intervalNames[interval1int]).c_str(), NULL);
			nvgRestore(args.vg);
		}
		LedDisplay::drawLayer(args, layer);
	}
};





struct IPQWidget : ModuleWidget {
	IPQWidget(IPQ* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/IPQ.svg")));

		addChild(createWidget<CuteFoxScrew>(Vec(RACK_GRID_WIDTH, 2)));
		addChild(createWidget<CuteFoxScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 2)));
		addChild(createWidget<CuteFoxScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<CuteFoxScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<CuteFoxBigKnob>(mm2px(Vec(20.32, 31.544)), module, IPQ::INTERVAL1_PARAM));
		addParam(createParamCentered<CuteFoxBigKnob>(mm2px(Vec(20.32, 70.937)), module, IPQ::INTERVAL2_PARAM));

		addInput(createInputCentered<CuteFoxPort>(mm2px(Vec(9.184, 115.784)), module, IPQ::INPUT_INPUT));
		addInput(createInputCentered<CuteFoxPort>(mm2px(Vec(20.32, 48.721)), module, IPQ::CV1_INPUT));
		addInput(createInputCentered<CuteFoxPort>(mm2px(Vec(20.32, 88.307)), module, IPQ::CV2_INPUT));

		addOutput(createOutputCentered<CuteFoxPort>(mm2px(Vec(31.75, 102.912)), module, IPQ::TRIGGER_OUTPUT));
		addOutput(createOutputCentered<CuteFoxPort>(mm2px(Vec(31.748, 115.797)), module, IPQ::OUTPUT_OUTPUT));
		
		IntervalDisplay* display1 = createWidget<IntervalDisplay>(mm2px(Vec(20.32, 19)));
		IntervalDisplay* display2 = createWidget<IntervalDisplay>(mm2px(Vec(20.32, 58.393)));
		if(module) {
    		display1->module = module;
			display2->module = module;
			display1->displayInterval = 0;
			display2->displayInterval = 1;
			addChild(display1);
			addChild(display2);
		}
		else {
    		display1->module = nullptr;
			display2->module = nullptr;
		}
	}
};


Model* modelIPQ = createModel<IPQ, IPQWidget>("IPQ");