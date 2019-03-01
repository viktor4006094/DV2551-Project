#pragma once

#include "GPUStage.hpp"

class RenderStage : public GPUStage
{
public:
	RenderStage();
	virtual ~RenderStage();

	void Run(int index, Project* p);
private:

};