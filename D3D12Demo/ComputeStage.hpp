#pragma once

#include "GPUStage.hpp"

class ComputeStage : public GPUStage
{
public:
	ComputeStage();
	virtual ~ComputeStage();

	void Init(Project* p);

	void Run(int index, Project* p);
private:

};