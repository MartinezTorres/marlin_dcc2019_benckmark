#include <marlin/inc/marlin.h>

#include <codecs/marlin2019.hpp>
#include <util/distribution.hpp>

#include <functional>
#include <string>
#include <iostream>
#include <cassert>
#include <cstring>
#include <stack>
#include <queue>
#include <map>
#include <bitset>
#include <unordered_map>
#include <algorithm>
#include <memory>

struct Marlin2019Pimpl : public CODEC8Z {
	
	std::vector<std::shared_ptr<MarlinDictionary>> dictionaries;
	
	std::string coderName;
	std::string name() const { return coderName; }
	
	
	
	Marlin2019Pimpl(Distribution::Type distType, std::map<std::string, double> conf) {

		conf.emplace("numDict", 11);

		{
			std::ostringstream oss;
			oss << "Marlin2019";
			for (auto &&c : conf)
				oss << "_" << c.first << ":" << c.second;
			coderName = oss.str();
		}
		

		std::vector<std::shared_ptr<MarlinDictionary>> builtDictionaries(conf["numDict"]);

//		#pragma omp parallel for
		for (size_t p=0; p<builtDictionaries.size(); p++) {
			
			std::vector<double> pdf(256,0.);
			for (double i=0.05; i<0.99; i+=0.1) {
				auto pdf0 = Distribution::pdf(distType, (p+i)/builtDictionaries.size());
				for (size_t j=0; j<pdf.size(); j++)
					pdf[j] += pdf0[j]/10.;
			}
			builtDictionaries[p] = std::make_shared<MarlinDictionary>(pdf,conf);
		}
		
		dictionaries.resize(256);
		
//		#pragma omp parallel for
		for (size_t h=0; h<256; h+=4) {
			
			auto testData = Distribution::getResiduals(Distribution::pdf(distType, (h+2)/256.), 1<<16);
			
			double lowestSize = testData.size()*0.99; // If efficiency is not enough to compress 1%, skip compression
			for (auto &&dict : builtDictionaries) {
				std::vector<uint8_t> out(testData.size());
				out.resize(dict->compress(out.data(), out.capacity(), testData.data(), testData.size()));
				
				if (out.size() < lowestSize) {
					lowestSize = out.size();
					for (size_t hh = 0; hh<4; hh++)
						dictionaries[h+hh] = dict;
				}
			}	
		}
	}

	
	void   compress(
		const std::vector<std::reference_wrapper<const AlignedArray8>> &in,
		      std::vector<std::reference_wrapper<      AlignedArray8>> &out,
		      std::vector<std::reference_wrapper<      uint8_t      >> &entropy) const { 
		
		for (size_t i=0; i<in.size(); i++)
			if (dictionaries[entropy[i]])
				out[i].get().resize(dictionaries[entropy[i]]->compress(out[i].get().data(), out[i].get().capacity(), in[i].get().data(), in[i].get().size()));
			else
				out[i].get().resize(in[i].get().size());
	}

	void uncompress(
		const std::vector<std::reference_wrapper<const AlignedArray8>> &in,
		      std::vector<std::reference_wrapper<      AlignedArray8>> &out,
		      std::vector<std::reference_wrapper<const uint8_t      >> &entropy) const {
		
		for (size_t i=0; i<in.size(); i++)
			if (dictionaries[entropy[i]])
				out[i].get().resize(dictionaries[entropy[i]]->decompress(out[i].get().data(), out[i].get().size(), in[i].get().data(), in[i].get().size()));
			else
				out[i].get().resize(in[i].get().size());
	}

};


	
Marlin2019::Marlin2019(Distribution::Type distType, std::map<std::string, double> conf) 
	: CODEC8withPimpl( new Marlin2019Pimpl(distType, conf) ) {}

