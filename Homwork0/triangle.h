#pragma once

#include "base/vulkanexamplebase.h"

class VulkanTriangle : public VulkanExampleBase
{
public:
	virtual void render() override;
	virtual void buildCommandBuffers() override;
};