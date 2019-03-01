#pragma once

#include "GPUStage.hpp"

class PassThroughStage : public GPUStage
{
public:
	PassThroughStage();
	virtual ~PassThroughStage();
	void Init(Project* p);

	void Run(int index, Project* p);
private:

};