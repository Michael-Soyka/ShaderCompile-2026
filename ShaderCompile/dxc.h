//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: DXC backend, HLSL compute shaders to SPIR-V
//
//=============================================================================//

#pragma once

#include "cmdsink.h"

namespace CfgProcessor
{
	struct ComboBuildCommand;
}

namespace Dxc
{
	// Same contract as Compiler::ExecuteCommand, for cs_6_x models
	void ExecuteCommand( const CfgProcessor::ComboBuildCommand& command, CmdSink::IResponse*& pResponse, unsigned int flags );
}
