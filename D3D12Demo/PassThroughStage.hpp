#pragma once

#include "GPUStage.hpp"

class PassThroughStage : public GPUStage
{
public:
	PassThroughStage();
	virtual ~PassThroughStage();

	void Run(int index, Project* p);
private:

};