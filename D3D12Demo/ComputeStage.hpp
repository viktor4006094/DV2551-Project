#pragma once

#include "GPUStage.hpp"

class ComputeStage : public GPUStage
{
public:
	ComputeStage();
	virtual ~ComputeStage();

	void Run(int index, Project* p);
private:

};