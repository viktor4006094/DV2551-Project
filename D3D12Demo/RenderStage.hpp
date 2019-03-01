#pragma once

#include "GPUStage.hpp"

class RenderStage : public GPUStage
{
public:
	RenderStage();
	virtual ~RenderStage();
	void Init(Project* p);

	void Run(int index, Project* p);
private:

};